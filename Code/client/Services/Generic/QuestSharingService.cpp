#include <Services/QuestSharingService.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/QuestService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Messages/NotifyPartyQuestSharing.h>
#include <Messages/NotifyQuestShareOffer.h>
#include <Messages/RequestPartyQuestSharing.h>
#include <Messages/RequestQuestShare.h>
#include <Messages/RequestQuestShareAnswer.h>

#include <OverlayApp.hpp>
#include <Forms/TESQuest.h>
#include <Games/TES.h>
#include <SharedQuests.h>

QuestSharingService::QuestSharingService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_questSharingConnection = aDispatcher.sink<NotifyPartyQuestSharing>().connect<&QuestSharingService::OnQuestSharing>(this);
    m_questShareOfferConnection = aDispatcher.sink<NotifyQuestShareOffer>().connect<&QuestSharingService::OnQuestShareOffer>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&QuestSharingService::OnPartyLeft>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&QuestSharingService::OnDisconnected>(this);
}

uint32_t QuestSharingService::GetSharedQuestTypes() const noexcept
{
    if (!m_partyKnown || !m_world.GetPartyService().IsInParty())
        return m_world.GetServerSettings().SharedQuestTypes;
    return m_enabled ? m_types : 0u;
}

void QuestSharingService::SetSharing(bool aEnabled, uint32_t aTypes) const noexcept
{
    if (!m_world.GetPartyService().IsLeader())
    {
        spdlog::warn("[QuestSharing]: only the party leader changes the party's quest sharing");
        return;
    }

    RequestPartyQuestSharing request;
    request.Enabled = aEnabled;
    request.Types = aTypes;
    m_transport.Send(request);
}

void QuestSharingService::ListShareableQuests() const noexcept
{
    std::string quests;
    for (TESQuest* pQuest : ModManager::Get()->quests)
    {
        // The journal's quests: running, with a kind (None are the game's background quests), and not shared already
        if (pQuest->IsStopped() || pQuest->IsCompleted() || pQuest->type == TESQuest::Type::None || QuestService::IsNonSyncableQuest(pQuest) ||
            QuestService::IsSharedQuest(pQuest))
            continue;
        // Signed, as the overlay hands form IDs back as an int
        quests += fmt::format(R"({}{{"id":{},"name":{}}})", quests.empty() ? "" : ",", static_cast<int32_t>(pQuest->formID), OverlayService::ToJsonString(QuestService::GetName(pQuest)));
    }

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, "[" + quests + "]");
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("shareableQuests", pArguments);
}

void QuestSharingService::Share(uint32_t aQuestFormId) const noexcept
{
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(aQuestFormId));
    RequestQuestShare request;
    if (!pQuest || !m_world.GetModSystem().GetServerModId(aQuestFormId, request.Id))
    {
        spdlog::error("[QuestSharing]: no quest {:X} to offer", aQuestFormId);
        return;
    }

    request.Stage = pQuest->currentStage;
    request.ClientQuestType = static_cast<uint8_t>(pQuest->type);
    spdlog::info("[QuestSharing]: offering quest {:X} ({}) at stage {} to the party", aQuestFormId, pQuest->idName.AsAscii(), request.Stage);
    m_transport.Send(request);
}

void QuestSharingService::Answer(uint32_t aQuestFormId, bool aAccept) const noexcept
{
    RequestQuestShareAnswer answer;
    if (!m_world.GetModSystem().GetServerModId(aQuestFormId, answer.Id))
    {
        spdlog::error("[QuestSharing]: no quest {:X} to answer for", aQuestFormId);
        return;
    }
    answer.Accept = aAccept;
    m_transport.Send(answer);
}

void QuestSharingService::OnQuestSharing(const NotifyPartyQuestSharing& acMessage) noexcept
{
    spdlog::info("[QuestSharing]: the party shares quests {}, kinds {:X}", acMessage.Enabled ? "by kind" : "only when offered", acMessage.Types);
    m_partyKnown = true;
    m_enabled = acMessage.Enabled;
    m_types = acMessage.Types;

    auto pArguments = CefListValue::Create();
    pArguments->SetBool(0, m_enabled);
    pArguments->SetInt(1, static_cast<int>(m_types));
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("partyQuestSharing", pArguments);
}

void QuestSharingService::OnQuestShareOffer(const NotifyQuestShareOffer& acMessage) const noexcept
{
    const uint32_t cFormId = m_world.GetModSystem().GetGameId(acMessage.Id);
    TESQuest* pQuest = Cast<TESQuest>(TESForm::GetById(cFormId));
    if (!pQuest)
    {
        // A quest of a mod this game doesn't have: it can't be taken
        spdlog::warn("[QuestSharing]: {} offered quest {:X}, which this game doesn't have", acMessage.PlayerName.c_str(), acMessage.Id.LogFormat());
        RequestQuestShareAnswer answer;
        answer.Id = acMessage.Id;
        m_transport.Send(answer);
        return;
    }

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acMessage.PlayerName.c_str());
    pArguments->SetString(1, QuestService::GetName(pQuest));
    pArguments->SetInt(2, static_cast<int>(cFormId));
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("questShareOffer", pArguments);
}

void QuestSharingService::OnPartyLeft(const PartyLeftEvent&) noexcept
{
    m_partyKnown = false;
}

void QuestSharingService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_partyKnown = false;
}
