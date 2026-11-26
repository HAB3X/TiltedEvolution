#pragma once

#include <Structs/Inventory.h>

struct World;
struct TransportService;
struct NotifyTrade;
struct DisconnectedEvent;

/**
 * @brief Player trading with a party member: the overlay's trade window. The window picks stacks of the player's own
 * inventory (sent to it as "tradeInventory": what can be traded, nothing worn or belonging to a quest) and amounts;
 * this service turns them into the offer it sends (RequestTrade), and passes the server's trade state (NotifyTrade) on
 * as "trade". The server moves the items itself once both confirm, as inventory changes of the two characters.
 */
struct TradeService
{
    TradeService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~TradeService() = default;

    TP_NOCOPYMOVE(TradeService);

    // The overlay's Trade on a party member: opens the window with nothing offered yet
    void Start(uint32_t aPartnerId) noexcept;
    // The window's offer: (index in the last "tradeInventory", count) pairs
    void Offer(const Vector<std::pair<uint32_t, int32_t>>& acPicks) noexcept;
    void Confirm() noexcept;
    void Cancel() noexcept;

private:
    void OnTrade(const NotifyTrade& acMessage) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    // Sends the window the player's tradeable stacks, and keeps them for Offer
    void SendInventory() noexcept;
    void Send(uint8_t aAction, const Inventory& acOffer = {}) noexcept;

    World& m_world;
    TransportService& m_transport;
    uint32_t m_partnerId = 0;
    Vector<Inventory::Entry> m_tradeable;

    entt::scoped_connection m_tradeConnection;
    entt::scoped_connection m_disconnectedConnection;
};
