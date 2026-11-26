#pragma once

#include <Events/PacketEvent.h>

struct World;
struct TeleportRequest;

/**
 * @brief Moving players to each other: a player going to another (TeleportRequest, the player list's "Go to").
 */
struct PartyTeleportService
{
    PartyTeleportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;

    TP_NOCOPYMOVE(PartyTeleportService);

private:
    void OnTeleport(const PacketEvent<TeleportRequest>& acMessage) const noexcept;

    World& m_world;

    entt::scoped_connection m_teleportConnection;
};
