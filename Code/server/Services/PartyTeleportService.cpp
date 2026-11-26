#include <Services/PartyTeleportService.h>

#include <GameServer.h>
#include <World.h>
#include <Services/CommandService.h>

#include <Messages/NotifyTeleport.h>
#include <Messages/TeleportCommandResponse.h>
#include <Messages/TeleportRequest.h>

namespace
{
NotifyTeleport TeleportTo(World& aWorld, const Player& acTarget) noexcept
{
    const TeleportCommandResponse cWhere = CommandService::WhereIs(aWorld, acTarget);
    NotifyTeleport teleport{};
    teleport.CellId = cWhere.CellId;
    teleport.Position = cWhere.Position;
    teleport.WorldSpaceId = cWhere.WorldSpaceId;
    return teleport;
}
} // namespace

PartyTeleportService::PartyTeleportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_teleportConnection = aDispatcher.sink<PacketEvent<TeleportRequest>>().connect<&PartyTeleportService::OnTeleport>(this);
}

void PartyTeleportService::OnTeleport(const PacketEvent<TeleportRequest>& acMessage) const noexcept
{
    Player* pTargetPlayer = m_world.GetPlayerManager().GetById(acMessage.Packet.PlayerId);
    if (!pTargetPlayer)
        return;

    acMessage.pPlayer->Send(TeleportTo(m_world, *pTargetPlayer));
}
