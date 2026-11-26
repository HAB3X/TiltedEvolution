#pragma once

struct World;
struct TransportService;
struct ActivateEvent;
struct DisconnectedEvent;
struct PartyLeftEvent;
struct NotifyPlayerHealthUpdate;
struct NotifyRevive;

/**
 * @brief Reviving in a party, when the server allows it (ServerSettings::ReviveEnabled).
 *
 * A player who would die stays in bleedout (the death system already makes the player essential with no bleedout
 * recovery) while a party member is up: activating them sends RequestRevive, and the server's NotifyRevive gets them
 * back up here. When every party member is down, after kReviveWindow, or out of a party, PlayerService's usual death
 * and respawn takes over.
 */
struct ReviveService
{
    ReviveService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~ReviveService() = default;

    TP_NOCOPYMOVE(ReviveService);

    enum class Wait
    {
        kWaiting,
        kRevived,
        kGiveUp
    };

    // The local player just went down: true when they wait for a party member instead of dying
    [[nodiscard]] bool StartWaiting() noexcept;
    // While waiting: still down, revived (once), or give up and die
    [[nodiscard]] Wait Poll() noexcept;

private:
    void OnActivate(const ActivateEvent& acEvent) noexcept;
    void OnPlayerHealthUpdate(const NotifyPlayerHealthUpdate& acMessage) noexcept;
    void OnRevive(const NotifyRevive& acMessage) noexcept;
    void OnPartyLeft(const PartyLeftEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;

    World& m_world;
    TransportService& m_transport;
    // Party members (player id) whose party bar says they are down
    Set<uint32_t> m_downedPartners;
    bool m_waiting = false;
    bool m_revived = false;
    bool m_partyDown = false;
    std::chrono::steady_clock::time_point m_giveUpAt{};
    std::chrono::steady_clock::time_point m_nextReviveRequest{};

    entt::scoped_connection m_activateConnection;
    entt::scoped_connection m_healthConnection;
    entt::scoped_connection m_reviveConnection;
    entt::scoped_connection m_partyLeftConnection;
    entt::scoped_connection m_disconnectedConnection;
};
