#pragma once

struct World;
struct TransportService;
struct UpdateEvent;
struct DisconnectedEvent;

/**
 * @brief Each player's crimes stay theirs: a guard deals with an offender from the offender's game.
 *
 * Bounties are each game's own crime gold, so only the offender's game knows one, and guards run in whichever game owns
 * them (usually the party leader's), which would let a non-leader's crime go unpunished. Every second this service
 * reports the player's bounty with each crime faction to the server and names the guards of that faction near the
 * player; the server hands them to this game (RequestCrimeAuthority), whose own AI then arrests or attacks this player.
 * Others see those guards go for this player's character, never for themselves, as their games replay the guard's
 * actions against the offender's server id. When the bounty is gone (paid, served, cleared) the server gives the
 * guards back to the party leader.
 */
struct CrimeService
{
    CrimeService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~CrimeService() = default;

    TP_NOCOPYMOVE(CrimeService);

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;

    struct AskedGuard
    {
        uint32_t OwnershipEpoch{};
        std::chrono::steady_clock::time_point Retry{};
    };

    World& m_world;
    TransportService& m_transport;
    std::chrono::steady_clock::time_point m_nextScan{};
    // The bounty last reported for each crime faction (form id), until it is reported gone
    Map<uint32_t, uint32_t> m_reportedBounties;
    // Guards (server id) already asked for, at which ownership epoch: asked again when it changes or after a while
    Map<uint32_t, AskedGuard> m_askedGuards;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_disconnectedConnection;
};
