#pragma once

#include <Events/PacketEvent.h>

struct World;
struct RequestHostWorld;
struct HostWorldStateRequest;

/**
 * @brief Host world mode (Gameplay:bHostWorld): passes the host's world to each player who joins.
 *
 * A joiner asks for it, the host's game collects it, and the joiner applies it and is moved to the host. The server
 * only relays it and stores no game state. The host is the party leader.
 */
struct HostWorldService
{
    HostWorldService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~HostWorldService() noexcept = default;

    TP_NOCOPYMOVE(HostWorldService);

protected:
    void OnRequestHostWorld(const PacketEvent<RequestHostWorld>& acPacket) noexcept;
    void OnHostWorldState(const PacketEvent<HostWorldStateRequest>& acPacket) noexcept;

private:
    // The leader of the player's party, null outside host world mode or a party
    Player* GetHost(Player* apPlayer) noexcept;

    World& m_world;

    entt::scoped_connection m_requestConnection;
    entt::scoped_connection m_stateConnection;
};
