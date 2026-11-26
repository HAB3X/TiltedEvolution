#include <Services/QuestDesyncService.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/QuestService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Events/UpdateEvent.h>
#include <Messages/NotifyQuestDesync.h>
#include <Messages/NotifyQuestCatchUp.h>
#include <Messages/NotifyPartyQuestSharing.h>
#include <Messages/RequestQuestStages.h>

#include <OverlayApp.hpp>
#include <Forms/TESQuest.h>

namespace
{
constexpr std::chrono::seconds kReportInterval{60};
// The chat names this many quests, and counts the rest
constexpr size_t kNamedQuests = 5;

std::string QuestName(const GameId& acId) noexcept
{
    const uint32_t cFormId = World::Get().GetModSystem().GetGameId(acId);
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(cFormId));
    if (!pQuest)
        return fmt::format("{:X}:{:X}", acId.ModId, acId.BaseId);
    return QuestService::GetName(pQuest);
}

// The leader's stages this game can take: the quests completed here stay completed, taking an earlier stage would
// restart them
QuestLog StagesToTake(const QuestLog& acLeaderStages) noexcept
{
    QuestLog stages;
    for (const QuestLog::Entry& cEntry : acLeaderStages.Entries)
    {
        TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(World::Get().GetModSystem().GetGameId(cEntry.Id)));
        if (pQuest && !pQuest->IsCompleted())
            stages.Entries.push_back(cEntry);
    }
    return stages;
}
} // namespace

QuestDesyncService::QuestDesyncService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&QuestDesyncService::OnUpdate>(this);
    m_questDesyncConnection = aDispatcher.sink<NotifyQuestDesync>().connect<&QuestDesyncService::OnQuestDesync>(this);
    m_questCatchUpConnection = aDispatcher.sink<NotifyQuestCatchUp>().connect<&QuestDesyncService::OnQuestCatchUp>(this);
    m_questSharingConnection = aDispatcher.sink<NotifyPartyQuestSharing>().connect<&QuestDesyncService::OnQuestSharing>(this);
}

void QuestDesyncService::OnQuestSharing(const NotifyPartyQuestSharing&) noexcept
{
    // Sent on joining a party and when the shared kinds change: report right away, so a joiner is offered the
    // leader's progress (OnQuestCatchUp) without waiting a minute
    m_nextReport = {};
}

void QuestDesyncService::OnUpdate(const UpdateEvent&) noexcept
{
    const auto cNow = std::chrono::steady_clock::now();
    if (cNow < m_nextReport || !m_transport.IsOnline() || !m_world.GetPartyService().IsInParty())
        return;
    m_nextReport = cNow + kReportInterval;

    // Running and completed ones alike: the server only compares stages
    QuestLog completed;
    RequestQuestStages request;
    QuestService::CollectSharedQuests(request.Stages, completed);
    request.Stages.Entries.insert(request.Stages.Entries.end(), completed.Entries.begin(), completed.Entries.end());
    m_transport.Send(request);
}

void QuestDesyncService::OnQuestDesync(const NotifyQuestDesync& acMessage) noexcept
{
    std::string names;
    for (size_t i = 0; i < acMessage.Quests.size() && i < kNamedQuests; ++i)
        names += (i ? ", " : "") + QuestName(acMessage.Quests[i]);
    const size_t cMore = acMessage.Quests.size() > kNamedQuests ? acMessage.Quests.size() - kNamedQuests : 0;
    spdlog::warn("[QuestDesync]: {} shared quests differ from {}'s: {}{}", acMessage.Quests.size(), acMessage.OtherPlayer.c_str(), names,
                 cMore ? fmt::format(" and {} more", cMore) : "");

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acMessage.OtherPlayer.c_str());
    pArguments->SetBool(1, acMessage.OtherIsLeader);
    pArguments->SetString(2, names);
    pArguments->SetInt(3, static_cast<int>(cMore));

    // Only the member repairs, with the leader's stages; a later notice replaces what an earlier one offered
    m_repair = acMessage.OtherIsLeader ? StagesToTake(acMessage.LeaderStages) : QuestLog{};
    const uint32_t cRepairable = m_repair.Entries.empty() ? 0 : m_world.ctx().at<QuestService>().TakeStages(m_repair, false, false);
    if (cRepairable == 0)
        m_repair.Entries.clear();
    pArguments->SetInt(4, static_cast<int>(cRepairable));
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("questDesync", pArguments);
}

void QuestDesyncService::OnQuestCatchUp(const NotifyQuestCatchUp& acMessage) noexcept
{
    m_catchUp = StagesToTake(acMessage.Quests);

    const uint32_t cChanged = m_world.ctx().at<QuestService>().TakeStages(m_catchUp, false, false);
    spdlog::info("[QuestDesync]: {} offers their stages of {} quests, {} would change here", acMessage.LeaderName.c_str(), acMessage.Quests.Entries.size(), cChanged);
    if (cChanged == 0)
    {
        m_catchUp.Entries.clear();
        return;
    }

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acMessage.LeaderName.c_str());
    pArguments->SetInt(1, static_cast<int>(cChanged));
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("questCatchUp", pArguments);
}

void QuestDesyncService::Repair() noexcept
{
    const QuestLog cStages = std::move(m_repair);
    m_repair.Entries.clear();
    if (cStages.Entries.empty())
    {
        spdlog::warn("[QuestDesync]: asked to repair, but no desync notice offered the leader's stages");
        return;
    }

    const uint32_t cChanged = m_world.ctx().at<QuestService>().TakeStages(cStages, false, true);
    spdlog::info("[QuestDesync]: repaired with the leader's stages, {} quests changed", cChanged);
}

void QuestDesyncService::AnswerCatchUp(bool aTake) noexcept
{
    const QuestLog cStages = std::move(m_catchUp);
    m_catchUp.Entries.clear();
    if (!aTake || cStages.Entries.empty())
        return;

    const uint32_t cChanged = m_world.ctx().at<QuestService>().TakeStages(cStages, false, true);
    spdlog::info("[QuestDesync]: took the leader's stages, {} quests changed", cChanged);
}
