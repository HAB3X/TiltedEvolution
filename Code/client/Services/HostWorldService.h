#pragma once

#include <Structs/HostWorldState.h>

struct World;
struct TransportService;
struct PartyJoinedEvent;
struct NotifyHostWorldRequested;
struct NotifyHostWorldState;

/**
 * @brief Host world mode (the server's Gameplay:bHostWorld): a player who joins takes over the host's world.
 *
 * The host is the party leader. On joining the party, a member asks for the host's world; the host's game collects
 * its quests, cleared locations and dead unique NPCs, and the member's game is made to match, after which the server
 * moves them to the host. From then on the regular party sync keeps the two games together.
 */
struct HostWorldService
{
    HostWorldService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransportService) noexcept;
    ~HostWorldService() = default;

    TP_NOCOPYMOVE(HostWorldService);

protected:
    void OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept;
    void OnHostWorldRequested(const NotifyHostWorldRequested& acMessage) noexcept;
    void OnHostWorldState(const NotifyHostWorldState& acMessage) noexcept;

private:
    HostWorldState CollectWorld() const noexcept;
    void ApplyQuests(const HostWorldState& acState) noexcept;
    void ApplyLocations(const HostWorldState& acState) const noexcept;
    void ApplyUniqueActorDeaths(const HostWorldState& acState) const noexcept;

    World& m_world;
    TransportService& m_transport;

    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_hostWorldRequestedConnection;
    entt::scoped_connection m_hostWorldStateConnection;
};
