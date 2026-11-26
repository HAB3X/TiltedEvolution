#pragma once

#include <Events/PacketEvent.h>
#include <Structs/Inventory.h>

struct World;
struct Player;
struct RequestTrade;
struct PlayerLeaveEvent;

/**
 * @brief Player trading between two party members in range: each side's offer, both confirm, and the server moves
 * the items. Any change to an offer unconfirms both sides, so nobody confirms something other than what they get.
 * On both confirms the server checks each side's character still holds its offer (its copy of their inventory), takes
 * it from one and gives it to the other there, and sends the changes to everyone as inventory changes of those
 * characters, so each game, the two players' own included, applies them once. Nothing is created: an offer the giver
 * doesn't hold is refused (kMissingItems).
 */
struct TradeService
{
    TradeService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~TradeService() noexcept = default;

    TP_NOCOPYMOVE(TradeService);

private:
    struct Side
    {
        Player* pPlayer{};
        Inventory Offer{};
        bool Confirmed{};
    };

    struct Session
    {
        Side Sides[2];
    };

    void OnTrade(const PacketEvent<RequestTrade>& acMessage) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;

    // The session apPlayer is in, if any
    Session* Find(const Player* apPlayer) noexcept;
    bool CanTrade(Player* apPlayer, Player* apPartner) const noexcept;
    void SendState(const Session& acSession, uint8_t aEvent) const noexcept;
    // Moves both offers; false when a side no longer holds its offer
    bool Complete(const Session& acSession) const noexcept;
    void End(const Session* apSession) noexcept;

    World& m_world;
    Vector<Session> m_sessions;

    entt::scoped_connection m_tradeConnection;
    entt::scoped_connection m_leaveConnection;
};
