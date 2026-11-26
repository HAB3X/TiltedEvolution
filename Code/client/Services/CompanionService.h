#pragma once

struct World;
struct TransportService;
struct UpdateEvent;
struct DisconnectedEvent;

/**
 * @brief A player's followers and horse are theirs: their game runs them, so the game takes them along through doors and
 * fast travel as it does alone, and the others see them follow that player.
 *
 * Followers are this game's state (the player teammate flag), and NPCs usually run in the party leader's game, where
 * they are nobody's follower. Every second this service names this player's followers and last ridden horse near
 * them (RequestCompanions); the server gives them to this game and refuses anyone else's claim on them, so a follower
 * never becomes the party's. One dismissed, or left behind, is no longer named and goes back to normal ownership.
 */
struct CompanionService
{
    CompanionService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~CompanionService() = default;

    TP_NOCOPYMOVE(CompanionService);

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;

    World& m_world;
    TransportService& m_transport;
    std::chrono::steady_clock::time_point m_nextScan{};
    std::chrono::steady_clock::time_point m_nextResend{};
    // The list last sent, as (server id, epoch) pairs: sent again only when it changes, or every so often
    Vector<std::pair<uint32_t, uint32_t>> m_sent;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_disconnectedConnection;
};
