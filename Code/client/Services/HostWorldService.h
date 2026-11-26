#pragma once

#include <Structs/HostWorldState.h>

#include <chrono>

struct World;
struct TransportService;
struct PartyJoinedEvent;
struct UpdateEvent;
struct NotifyHostWorldRequested;
struct NotifyHostWorldState;

/**
 * @brief Host world mode (the server's Gameplay:bHostWorld): a player who joins takes over the host's world.
 *
 * The host is the party leader. On joining the party, a member asks for the host's world; the host's game collects
 * its quests, cleared locations and dead unique NPCs, and the member's game is made to match, after which the server
 * moves them to the host. From then on the regular party sync keeps the two games together.
 *
 * The member's own world is saved first, as BeforeHostWorld_<character>, so that their newest save isn't the only one
 * and going back to their own world is one load away.
 */
struct HostWorldService
{
    HostWorldService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransportService) noexcept;
    ~HostWorldService() = default;

    TP_NOCOPYMOVE(HostWorldService);

protected:
    void OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept;
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnHostWorldRequested(const NotifyHostWorldRequested& acMessage) noexcept;
    void OnHostWorldState(const NotifyHostWorldState& acMessage) noexcept;

private:
    HostWorldState CollectWorld() const noexcept;
    void RequestWorld() noexcept;
    // Each returns how many things it changed here
    uint32_t ApplyQuests(const HostWorldState& acState) noexcept;
    uint32_t ApplyLocations(const HostWorldState& acState) const noexcept;
    uint32_t ApplyUniqueActorDeaths(const HostWorldState& acState) const noexcept;

    World& m_world;
    TransportService& m_transport;

    // Waiting for the save of the member's own world, up to this time
    bool m_savingOwnWorld{false};
    std::chrono::steady_clock::time_point m_ownWorldSaveDeadline{};
    // The host's world was applied in this game session: saving again would save it as the member's own
    bool m_appliedThisSession{false};

    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_hostWorldRequestedConnection;
    entt::scoped_connection m_hostWorldStateConnection;
};
