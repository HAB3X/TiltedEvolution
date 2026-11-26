#include <Services/TravelTogetherService.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Messages/NotifyPartyTravel.h>
#include <Messages/RequestPartyTravel.h>
#include <Messages/TeleportCommandRequest.h>
#include <Messages/NotifyPartyDoor.h>
#include <Messages/RequestPartyDoor.h>
#include <Messages/NotifyPartySummon.h>
#include <Messages/NotifyPartyTeleportRefused.h>
#include <Messages/RequestPartySummon.h>
#include <Messages/RequestPartySummonAnswer.h>
#include <Services/CommandService.h>

#include <OverlayApp.hpp>
#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>

namespace
{
// Where the player is, as the game names it: their location (a town, a dungeon), else the world or cell
String CurrentPlaceName() noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return {};

    const char* pName = nullptr;
    if (TESForm* pLocation = pPlayer->GetCurrentLocation())
        pName = pLocation->GetName();
    if ((!pName || !*pName) && pPlayer->GetWorldSpace())
        pName = pPlayer->GetWorldSpace()->GetName();
    if ((!pName || !*pName) && pPlayer->parentCell)
        pName = pPlayer->parentCell->GetName();
    return pName ? String(pName) : String{};
}
} // namespace

TravelTogetherService::TravelTogetherService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_partyTravelConnection = aDispatcher.sink<NotifyPartyTravel>().connect<&TravelTogetherService::OnPartyTravel>(this);
    m_partyDoorConnection = aDispatcher.sink<NotifyPartyDoor>().connect<&TravelTogetherService::OnPartyDoor>(this);
    m_partySummonConnection = aDispatcher.sink<NotifyPartySummon>().connect<&TravelTogetherService::OnPartySummon>(this);
    m_teleportRefusedConnection = aDispatcher.sink<NotifyPartyTeleportRefused>().connect<&TravelTogetherService::OnTeleportRefused>(this);

    // The game sends this when the player arrives from a fast travel (the map or a carriage), not for teleports such
    // as the one a member who says yes takes, so a member following never offers a trip of their own
    EventDispatcherManager::Get()->fastTravelEndEvent.RegisterSink(this);
}

BSTEventResult TravelTogetherService::OnEvent(const TESFastTravelEndEvent*, const EventDispatcher<TESFastTravelEndEvent>*)
{
    m_world.GetRunner().Queue([this]() { OfferTravel(); });
    return BSTEventResult::kOk;
}

void TravelTogetherService::OfferTravel() const noexcept
{
    const auto& cParty = m_world.GetPartyService();
    if (!m_transport.IsOnline() || !cParty.IsLeader() || cParty.GetPartyMembers().size() < 2)
        return;

    RequestPartyTravel request;
    request.Place = CurrentPlaceName();
    spdlog::info("[TravelTogether]: fast travelled to '{}', offering the party to follow", request.Place.c_str());
    m_transport.Send(request);
}

void TravelTogetherService::OnPartyTravel(const NotifyPartyTravel& acMessage) noexcept
{
    spdlog::info("[TravelTogether]: {} fast travelled to '{}'", acMessage.LeaderName.c_str(), acMessage.Place.c_str());
    m_offeredLeader = acMessage.LeaderName;

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acMessage.LeaderName.c_str());
    pArguments->SetString(1, acMessage.Place.c_str());
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("partyTravel", pArguments);
}

void TravelTogetherService::Answer(bool aTravel) noexcept
{
    if (m_offeredLeader.empty())
    {
        spdlog::warn("[TravelTogether]: answered a travel offer that isn't there");
        return;
    }

    if (aTravel)
    {
        // The server answers with where the leader is now, which CommandService moves us to
        TeleportCommandRequest request;
        request.TargetPlayer = m_offeredLeader;
        m_transport.Send(request);
    }
    m_offeredLeader.clear();
}

void TravelTogetherService::OfferDoor(uint32_t aCellId, const NiPoint3& acPosition) const noexcept
{
    const auto& cParty = m_world.GetPartyService();
    if (!m_transport.IsOnline() || !cParty.IsInParty() || cParty.GetPartyMembers().size() < 2)
        return;

    const TESObjectCELL* pCell = Cast<TESObjectCELL>(TESForm::GetById(aCellId));
    if (!pCell)
    {
        spdlog::warn("[TravelTogether]: the door leads to cell {:X}, which isn't there, not offering it", aCellId);
        return;
    }

    RequestPartyDoor request;
    ModSystem& modSystem = m_world.GetModSystem();
    if (!modSystem.GetServerModId(pCell->formID, request.CellId) ||
        (pCell->worldspace && !modSystem.GetServerModId(pCell->worldspace->formID, request.WorldSpaceId)))
    {
        spdlog::warn("[TravelTogether]: the door's cell {:X} has no server id, not offering it", aCellId);
        return;
    }
    request.Position = acPosition;
    // An interior's own name; an exterior cell rarely has one, its worldspace does
    const char* pName = pCell->GetName();
    if ((!pName || !*pName) && pCell->worldspace)
        pName = pCell->worldspace->GetName();
    request.Place = pName ? pName : "";

    spdlog::info("[TravelTogether]: went through a door to '{}' ({:X}), offering the party to follow", request.Place.c_str(), aCellId);
    m_transport.Send(request);
}

void TravelTogetherService::OnPartyDoor(const NotifyPartyDoor& acMessage) noexcept
{
    spdlog::info("[TravelTogether]: {} went through a door to '{}'", acMessage.PlayerName.c_str(), acMessage.Place.c_str());
    TeleportCommandResponse destination{};
    destination.CellId = acMessage.CellId;
    destination.WorldSpaceId = acMessage.WorldSpaceId;
    destination.Position = acMessage.Position;
    m_offeredDoor = destination;

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acMessage.PlayerName.c_str());
    pArguments->SetString(1, acMessage.Place.c_str());
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("partyDoor", pArguments);
}

void TravelTogetherService::AnswerDoor(bool aFollow) noexcept
{
    if (!m_offeredDoor)
    {
        spdlog::warn("[TravelTogether]: answered a door offer that isn't there");
        return;
    }

    if (aFollow)
        m_world.ctx().at<CommandService>().MoveTo(*m_offeredDoor);
    m_offeredDoor.reset();
}

void TravelTogetherService::Summon(uint32_t aPlayerId) const noexcept
{
    RequestPartySummon request;
    request.PlayerId = aPlayerId;
    m_transport.Send(request);
}

void TravelTogetherService::OnPartySummon(const NotifyPartySummon& acMessage) const noexcept
{
    spdlog::info("[TravelTogether]: {} summons this player", acMessage.PlayerName.c_str());
    auto pArguments = CefListValue::Create();
    pArguments->SetInt(0, acMessage.PlayerId);
    pArguments->SetString(1, acMessage.PlayerName.c_str());
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("partySummon", pArguments);
}

void TravelTogetherService::AnswerSummon(uint32_t aSummonerId, bool aAccept) const noexcept
{
    // The server moves this player on yes (NotifyTeleport), after checking the party and combat again
    RequestPartySummonAnswer answer;
    answer.PlayerId = aSummonerId;
    answer.Accept = aAccept;
    m_transport.Send(answer);
}

void TravelTogetherService::OnTeleportRefused(const NotifyPartyTeleportRefused& acMessage) const noexcept
{
    spdlog::info("[TravelTogether]: move with {} refused, reason {}", acMessage.PlayerName.c_str(), acMessage.Reason);
    auto pArguments = CefListValue::Create();
    pArguments->SetInt(0, acMessage.Reason);
    pArguments->SetString(1, acMessage.PlayerName.c_str());
    pArguments->SetInt(2, acMessage.SecondsLeft);
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("partyTeleportRefused", pArguments);
}
