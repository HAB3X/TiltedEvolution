#include <Services/HostWorldService.h>

#include <Services/QuestService.h>
#include <Services/TransportService.h>

#include <World.h>

#include <Events/PartyJoinedEvent.h>

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

namespace
{
// The quests a joiner takes over: the ones the party syncs, without the background (None) and Miscellaneous ones,
// which the server only relays with Gameplay:bEnableMiscQuestSync, and which run per player for things like radiant
// favours and the game's own systems
bool IsHostWorldQuest(TESQuest* apQuest) noexcept
{
    return apQuest->type != TESQuest::Type::None && apQuest->type != TESQuest::Type::Miscellaneous && !QuestService::IsNonSyncableQuest(apQuest);
}

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
    , m_hostWorldRequestedConnection(aDispatcher.sink<NotifyHostWorldRequested>().connect<&HostWorldService::OnHostWorldRequested>(this))
    , m_hostWorldStateConnection(aDispatcher.sink<NotifyHostWorldState>().connect<&HostWorldService::OnHostWorldState>(this))
{
}

void HostWorldService::OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept
{
    if (acEvent.IsLeader || !m_world.GetServerSettings().HostWorld)
        return;

    spdlog::info("[HostWorld]: joined the host's party, asking for the host's world");
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

    for (TESQuest* pQuest : ModManager::Get()->quests)
    {
        GameId id;
        if (!IsHostWorldQuest(pQuest) || !modSystem.GetServerModId(pQuest->formID, id))
            continue;

        if (IsCompleted(pQuest))
            state.CompletedQuests.Entries.push_back({id, pQuest->currentStage});
        else if (!pQuest->IsStopped())
            state.RunningQuests.Entries.push_back({id, pQuest->currentStage});
    }

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
    spdlog::info("[HostWorld]: taking over the host's world: {} running and {} completed quests, {} cleared locations, {} dead unique NPCs",
                 acMessage.State.RunningQuests.Entries.size(), acMessage.State.CompletedQuests.Entries.size(), acMessage.State.ClearedLocations.size(),
                 acMessage.State.DeadUniqueActors.size());

    ApplyQuests(acMessage.State);
    ApplyLocations(acMessage.State);
    ApplyUniqueActorDeaths(acMessage.State);
}

void HostWorldService::ApplyQuests(const HostWorldState& acState) noexcept
{
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
            questService.ApplyRemoteUpdate(update);
        }
    };
    apply(acState.CompletedQuests, true);
    apply(acState.RunningQuests, false);

    // Our own quests that aren't in the host's world
    ScopedQuestOverride _;
    for (TESQuest* pQuest : ModManager::Get()->quests)
    {
        if (IsHostWorldQuest(pQuest) && !pQuest->IsStopped() && !IsCompleted(pQuest) && !hostQuests.contains(pQuest->formID))
        {
            spdlog::info("[HostWorld]: stopping our quest {:X} ({}), the host doesn't have it", pQuest->formID, pQuest->idName.AsAscii());
            QuestService::StopQuest(pQuest->formID);
        }
    }
}

void HostWorldService::ApplyLocations(const HostWorldState& acState) const noexcept
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

        pLocation->cleared = cHostCleared;
        pLocation->everCleared |= cHostCleared;
        // Saved with the location, like the game's own clearing
        pLocation->MarkChanged(BGSLocation::CHANGE_CLEARED);
        ++changed;
    }
    spdlog::info("[HostWorld]: {} locations changed to the host's cleared state", changed);
}

void HostWorldService::ApplyUniqueActorDeaths(const HostWorldState& acState) const noexcept
{
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

            spdlog::info("[HostWorld]: {} unique NPC {:X}, as in the host's world", cHostDead ? "killing" : "bringing back", apActor->formID);
            if (cHostDead)
                // No attacker: nobody here killed them, so no crime, bounty or kill credit
                apActor->KillImpl(nullptr, 100.f, true, true);
            else
                apActor->Respawn();
        });
}
