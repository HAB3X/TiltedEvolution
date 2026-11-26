#pragma once

struct World;
struct TransportService;

struct SetWaypointEvent;
struct RemoveWaypointEvent;
struct NotifySetWaypoint;
struct NotifyRemoveWaypoint;
struct NotifyPartyPing;
struct UpdateEvent;

/**
 * @brief Handles logic related to the local player.
 */
struct MapService
{
    MapService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~MapService() noexcept = default;

    TP_NOCOPYMOVE(MapService);

  protected:
    void OnSetWaypoint(const SetWaypointEvent& acMessage) noexcept;
    void OnRemoveWaypoint(const RemoveWaypointEvent& acMessage) noexcept;
    void OnNotifySetWaypoint(const NotifySetWaypoint& acMessage) noexcept;
    void OnNotifyRemoveWaypoint(const NotifyRemoveWaypoint& acMessage) noexcept;
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnNotifyPartyPing(const NotifyPartyPing& acMessage) const noexcept;

  private:
    // Pings: F3 marks what the crosshair is on, else the spot the player stands on, with a waypoint for the party
    void Ping() noexcept;

    World& m_world;
    entt::dispatcher& m_dispatcher;
    TransportService& m_transport;

    entt::scoped_connection m_playerSetWaypointConnection;
    entt::scoped_connection m_playerRemoveWaypointConnection;
    entt::scoped_connection m_playerNotifySetWaypointConnection;
    entt::scoped_connection m_playerNotifyRemoveWaypointConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_partyPingConnection;

    std::chrono::steady_clock::time_point m_lastPing{};
};
