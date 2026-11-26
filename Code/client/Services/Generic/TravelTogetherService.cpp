#include <Services/TravelTogetherService.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Messages/NotifyPartyTravel.h>
#include <Messages/RequestPartyTravel.h>
#include <Messages/TeleportCommandRequest.h>

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
