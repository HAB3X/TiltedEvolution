#include <Services/MapService.h>
#include <GameServer.h>

#include <Messages/NotifyRemoveWaypoint.h>
#include <Messages/NotifySetWaypoint.h>
#include <Messages/RequestRemoveWaypoint.h>
#include <Messages/RequestSetWaypoint.h>
#include <Messages/RequestPartyPing.h>
#include <Messages/NotifyPartyPing.h>

MapService::MapService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
      m_playerSetWaypointConnection = 
          aDispatcher.sink<PacketEvent<RequestSetWaypoint>>().connect<&MapService::OnSetWaypointRequest>(this);
      m_playerRemoveWaypointConnection =
          aDispatcher.sink<PacketEvent<RequestRemoveWaypoint>>().connect<&MapService::OnRemoveWaypointRequest>(this);
      m_partyPingConnection = aDispatcher.sink<PacketEvent<RequestPartyPing>>().connect<&MapService::OnPartyPing>(this);
}

void MapService::OnSetWaypointRequest(const PacketEvent<RequestSetWaypoint>& acMessage) const noexcept
{
    auto& message = acMessage.Packet;

    NotifySetWaypoint notify{};
    notify.Position = message.Position;
    notify.WorldSpaceFormID = message.WorldSpaceFormID;

    const auto& partyComponent = acMessage.pPlayer->GetParty();
    if (!partyComponent.JoinedPartyId.has_value())
        return;

    GameServer::Get()->SendToParty(notify, partyComponent, acMessage.GetSender());
}

void MapService::OnRemoveWaypointRequest(const PacketEvent<RequestRemoveWaypoint>& acMessage) const noexcept
{
    NotifyRemoveWaypoint notify{};

    const auto& partyComponent = acMessage.pPlayer->GetParty();
    if (!partyComponent.JoinedPartyId.has_value())
        return;

    GameServer::Get()->SendToParty(notify, partyComponent, acMessage.GetSender());
}


void MapService::OnPartyPing(const PacketEvent<RequestPartyPing>& acMessage) const noexcept
{
    const Player* pPlayer = acMessage.pPlayer;
    const auto& cParty = pPlayer->GetParty();
    if (!cParty.JoinedPartyId.has_value())
    {
        spdlog::warn("[MapService]: {} pinged without a party", pPlayer->GetUsername().c_str());
        return;
    }

    const auto& cPing = acMessage.Packet;
    NotifyPartyPing notify{};
    notify.PlayerId = pPlayer->GetId();
    notify.PlayerName = pPlayer->GetUsername();
    notify.WorldSpaceId = cPing.WorldSpaceId;
    notify.CellId = cPing.CellId;
    notify.Position = cPing.Position;
    notify.Target = cPing.Target;
    GameServer::Get()->SendToParty(notify, cParty, acMessage.GetSender());
}
