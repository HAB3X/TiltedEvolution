#include <Services/HostWorldService.h>

#include <Services/QuestService.h>
#include <Services/CommandService.h>
#include <OverlayApp.hpp>
#include <Services/TransportService.h>

#include <World.h>

#include <Events/PartyJoinedEvent.h>
#include <Events/UpdateEvent.h>
#include <Services/OverlayService.h>

#include <Messages/HostWorldStateRequest.h>
#include <Messages/NotifyHostWorldRequested.h>
#include <Messages/NotifyHostWorldState.h>
#include <Messages/NotifyQuestUpdate.h>
#include <Messages/RequestHostWorld.h>

#include <Actor.h>
#include <Forms/BGSLocation.h>
#include <Forms/TESQuest.h>
#include <Games/Overrides.h>
#include <Games/TES.h>
#include <Games/Skyrim/SaveLoad.h>

namespace
{
constexpr char kOwnWorldSave[] = "BeforeHostWorld";
// How long to wait for the player to be free to save (not in combat or a menu)
constexpr std::chrono::seconds kOwnWorldSaveTimeout{60};

bool IsCompleted(const TESQuest* apQuest) noexcept
{
    return (apQuest->flags & TESQuest::Flags::Completed) != 0;
}

// Calls the function on each actor that is some location's unique NPC (LCUN), once each
template <class TFunc> void ForEachUniqueActor(TFunc aFunc) noexcept
{
    TiltedPhoques::Set<uint32_t> seen;
    for (BGSLocation* pLocation : ModManager::Get()->locations)
    {
        for (const BGSLocation::UniqueNPC& uniqueNpc : pLocation->uniqueNPCs)
        {
            if (!seen.insert(uniqueNpc.refId).second)
                continue;
            if (Actor* pActor = Cast<Actor>(TESForm::GetById(uniqueNpc.refId)))
                aFunc(pActor);
        }
    }
}
} // namespace

HostWorldService::HostWorldService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransportService) noexcept
    : m_world(aWorld)
    , m_transport(aTransportService)
    , m_partyJoinedConnection(aDispatcher.sink<PartyJoinedEvent>().connect<&HostWorldService::OnPartyJoined>(this))
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&HostWorldService::OnUpdate>(this))
    , m_hostWorldRequestedConnection(aDispatcher.sink<NotifyHostWorldRequested>().connect<&HostWorldService::OnHostWorldRequested>(this))
    , m_hostWorldStateConnection(aDispatcher.sink<NotifyHostWorldState>().connect<&HostWorldService::OnHostWorldState>(this))
{
}

void HostWorldService::OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept
{
    if (acEvent.IsLeader || !m_world.GetServerSettings().HostWorld)
        return;

    // The joiner sees what would change and answers before anything happens (Accept)
    RequestWorld();
}

void HostWorldService::OnUpdate(const UpdateEvent&) noexcept
{
    if (!m_savingOwnWorld)
        return;

    if (BGSSaveLoadManager::IsSaveQueued())
    {
        if (std::chrono::steady_clock::now() < m_ownWorldSaveDeadline)
            return;

        // Joining matters more than the extra save: the earlier saves are still on disk
        BGSSaveLoadManager::CancelQueuedSave();
        spdlog::warn("[HostWorld]: couldn't save our own world in time, joining without it");
        m_world.GetOverlayService().SendSystemMessage(
            "Couldn't save your own world first (you were busy for a minute). Your earlier saves are untouched: load one of them to go back to your own world.");
    }
    else
    {
        m_world.GetOverlayService().SendSystemMessage(
            fmt::format("Saved your own world as {}_<your character>. Load it to go back to your own world after playing in the host's.", kOwnWorldSave));
    }

    m_savingOwnWorld = false;
    ApplyPending();
}

void HostWorldService::RequestWorld() noexcept
{
    spdlog::info("[HostWorld]: asking for the host's world");
    m_transport.Send(RequestHostWorld{});
}

void HostWorldService::OnHostWorldRequested(const NotifyHostWorldRequested& acMessage) noexcept
{
    HostWorldStateRequest response;
    response.PlayerId = acMessage.PlayerId;
    response.State = CollectWorld();

    spdlog::info("[HostWorld]: sending our world to player {}: {} running and {} completed quests, {} cleared locations, {} dead unique NPCs", acMessage.PlayerId,
                 response.State.RunningQuests.Entries.size(), response.State.CompletedQuests.Entries.size(), response.State.ClearedLocations.size(),
                 response.State.DeadUniqueActors.size());
    m_transport.Send(response);
}

HostWorldState HostWorldService::CollectWorld() const noexcept
{
    const auto& modSystem = m_world.GetModSystem();
    HostWorldState state;

    // The quests a joiner takes over
    QuestService::CollectSharedQuests(state.RunningQuests, state.CompletedQuests);

    for (BGSLocation* pLocation : ModManager::Get()->locations)
    {
        GameId id;
        if (pLocation->cleared && modSystem.GetServerModId(pLocation->formID, id))
            state.ClearedLocations.push_back(id);
    }

    ForEachUniqueActor(
        [&](Actor* apActor)
        {
            GameId id;
            if (apActor->IsDead() && modSystem.GetServerModId(apActor->formID, id))
                state.DeadUniqueActors.push_back(id);
        });

    return state;
}

void HostWorldService::OnHostWorldState(const NotifyHostWorldState& acMessage) noexcept
{
    spdlog::info("[HostWorld]: {}'s world: {} running and {} completed quests, {} cleared locations, {} dead unique NPCs", acMessage.HostName.c_str(),
                 acMessage.State.RunningQuests.Entries.size(), acMessage.State.CompletedQuests.Entries.size(), acMessage.State.ClearedLocations.size(),
                 acMessage.State.DeadUniqueActors.size());
    m_pending = acMessage;

    // Rejoining in the same session: the game is the host's world already, the joiner said yes to it
    if (m_appliedThisSession)
    {
        ApplyPending();
        return;
    }

    const uint32_t cQuests = ApplyQuests(acMessage.State, Mode::Count);
    const uint32_t cLocations = ApplyLocations(acMessage.State, Mode::Count);
    const uint32_t cActors = ApplyUniqueActorDeaths(acMessage.State, Mode::Count);

    auto* pOverlay = m_world.GetOverlayService().GetOverlayApp();
    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acMessage.HostName.c_str());
    pArguments->SetInt(1, static_cast<int>(cQuests));
    pArguments->SetInt(2, static_cast<int>(cLocations));
    pArguments->SetInt(3, static_cast<int>(cActors));
    pOverlay->ExecuteAsync("hostWorldOffer", pArguments);
    // The question needs the mouse
    m_world.GetOverlayService().SetActive(true);
}

void HostWorldService::Answer(bool aJoin) noexcept
{
    if (!m_pending)
        return;

    if (!aJoin)
    {
        spdlog::info("[HostWorld]: keeping our own world, leaving {}'s server", m_pending->HostName.c_str());
        m_world.GetOverlayService().SendSystemMessage(fmt::format("You kept your own world and left {}'s server.", m_pending->HostName.c_str()));
        m_pending.reset();
        m_heldTeleport.reset();
        m_transport.Close();
        return;
    }

    // Save the joiner's own world first, then apply the host's (OnUpdate)
    m_world.GetOverlayService().SendSystemMessage("Saving your own world before joining the host's...");
    BGSSaveLoadManager::QueueSaveAs(kOwnWorldSave);
    m_savingOwnWorld = true;
    m_ownWorldSaveDeadline = std::chrono::steady_clock::now() + kOwnWorldSaveTimeout;
}

bool HostWorldService::HoldTeleport(const TeleportCommandResponse& acTeleport) noexcept
{
    if (!m_pending)
        return false;
    m_heldTeleport = acTeleport;
    return true;
}

void HostWorldService::ApplyPending() noexcept
{
    if (!m_pending)
        return;

    const NotifyHostWorldState cWorld = std::move(*m_pending);
    m_pending.reset();

    const uint32_t cQuests = ApplyQuests(cWorld.State, Mode::Apply);
    const uint32_t cLocations = ApplyLocations(cWorld.State, Mode::Apply);
    const uint32_t cActors = ApplyUniqueActorDeaths(cWorld.State, Mode::Apply);
    m_appliedThisSession = true;

    m_world.GetOverlayService().SendSystemMessage(fmt::format("You're in {}'s world: {} quests, {} cleared or uncleared places and {} people changed to match it.",
                                                              cWorld.HostName.c_str(), cQuests, cLocations, cActors));

    // The server sent the host's place with the world; go there now
    if (m_heldTeleport)
    {
        const TeleportCommandResponse cTeleport = std::move(*m_heldTeleport);
        m_heldTeleport.reset();
        m_world.ctx().at<CommandService>().MoveTo(cTeleport);
    }
}

uint32_t HostWorldService::ApplyQuests(const HostWorldState& acState, Mode aMode) noexcept
{
    uint32_t changed = 0;
    const auto& modSystem = m_world.GetModSystem();
    auto& questService = m_world.ctx().at<QuestService>();

    // Through the party's quest sync, which already knows how to start, advance and skip quests safely
    TiltedPhoques::Set<uint32_t> hostQuests;
    const auto apply = [&](const QuestLog& acLog, bool aCompleted)
    {
        for (const QuestLog::Entry& entry : acLog.Entries)
        {
            const uint32_t cFormId = modSystem.GetGameId(entry.Id);
            hostQuests.insert(cFormId);

            TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(cFormId));
            if (!pQuest || (aCompleted && IsCompleted(pQuest)))
                continue;

            NotifyQuestUpdate update;
            update.Id = entry.Id;
            update.Stage = entry.Stage;
            update.Status = NotifyQuestUpdate::StageUpdate;
            update.ClientQuestType = static_cast<uint8_t>(pQuest->type);
            // Counted when the quest isn't there yet: the update itself skips stages that were already reached
            if (aCompleted || pQuest->IsStopped() || !pQuest->IsStageReached(entry.Stage))
                ++changed;
            if (aMode == Mode::Apply)
                questService.ApplyRemoteUpdate(update);
        }
    };
    apply(acState.CompletedQuests, true);
    apply(acState.RunningQuests, false);

    // Our own quests that aren't in the host's world
    ScopedQuestOverride _;
    for (TESQuest* pQuest : ModManager::Get()->quests)
    {
        if (QuestService::IsSharedQuest(pQuest) && !pQuest->IsStopped() && !IsCompleted(pQuest) && !hostQuests.contains(pQuest->formID))
        {
            ++changed;
            if (aMode == Mode::Count)
                continue;
            spdlog::info("[HostWorld]: stopping our quest {:X} ({}), the host doesn't have it", pQuest->formID, pQuest->idName.AsAscii());
            QuestService::StopQuest(pQuest->formID);
        }
    }
    return changed;
}

uint32_t HostWorldService::ApplyLocations(const HostWorldState& acState, Mode aMode) const noexcept
{
    const auto& modSystem = m_world.GetModSystem();

    TiltedPhoques::Set<uint32_t> cleared;
    for (const GameId& id : acState.ClearedLocations)
        cleared.insert(modSystem.GetGameId(id));

    uint32_t changed = 0;
    for (BGSLocation* pLocation : ModManager::Get()->locations)
    {
        const bool cHostCleared = cleared.contains(pLocation->formID);
        if (pLocation->cleared == cHostCleared)
            continue;

        ++changed;
        if (aMode == Mode::Count)
            continue;
        pLocation->cleared = cHostCleared;
        pLocation->everCleared |= cHostCleared;
        // Saved with the location, like the game's own clearing
        pLocation->MarkChanged(BGSLocation::CHANGE_CLEARED);
    }
    if (aMode == Mode::Apply)
        spdlog::info("[HostWorld]: {} locations changed to the host's cleared state", changed);
    return changed;
}

uint32_t HostWorldService::ApplyUniqueActorDeaths(const HostWorldState& acState, Mode aMode) const noexcept
{
    uint32_t changed = 0;
    const auto& modSystem = m_world.GetModSystem();

    TiltedPhoques::Set<uint32_t> dead;
    for (const GameId& id : acState.DeadUniqueActors)
        dead.insert(modSystem.GetGameId(id));

    ForEachUniqueActor(
        [&](Actor* apActor)
        {
            const bool cHostDead = dead.contains(apActor->formID);
            if (apActor->IsDead() == cHostDead)
                return;

            ++changed;
            if (aMode == Mode::Count)
                return;
            spdlog::info("[HostWorld]: {} unique NPC {:X}, as in the host's world", cHostDead ? "killing" : "bringing back", apActor->formID);
            if (cHostDead)
                // No attacker: nobody here killed them, so no crime, bounty or kill credit
                apActor->KillImpl(nullptr, 100.f, true, true);
            else
                apActor->Respawn();
        });
    return changed;
}
