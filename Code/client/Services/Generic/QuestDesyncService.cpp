#include <Services/QuestDesyncService.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/QuestService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Events/UpdateEvent.h>
#include <Messages/NotifyQuestDesync.h>
#include <Messages/RequestQuestStages.h>

#include <OverlayApp.hpp>
#include <Forms/TESQuest.h>

namespace
{
constexpr std::chrono::seconds kReportInterval{60};
// The chat names this many quests, and counts the rest
constexpr size_t kNamedQuests = 5;

// The quest's name in this game's language, else its editor id
std::string QuestName(const GameId& acId) noexcept
{
    const uint32_t cFormId = World::Get().GetModSystem().GetGameId(acId);
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(cFormId));
    if (!pQuest)
        return fmt::format("{:X}:{:X}", acId.ModId, acId.BaseId);
    if (pQuest->fullName.value.data && *pQuest->fullName.value.AsAscii())
        return pQuest->fullName.value.AsAscii();
    return pQuest->idName.AsAscii();
}
} // namespace

QuestDesyncService::QuestDesyncService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&QuestDesyncService::OnUpdate>(this);
    m_questDesyncConnection = aDispatcher.sink<NotifyQuestDesync>().connect<&QuestDesyncService::OnQuestDesync>(this);
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

void QuestDesyncService::OnQuestDesync(const NotifyQuestDesync& acMessage) const noexcept
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
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("questDesync", pArguments);
}
