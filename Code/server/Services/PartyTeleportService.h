#pragma once

#include <Events/PacketEvent.h>

#include <PartyTeleport.h>

struct World;
struct Player;
struct PlayerLeaveEvent;
struct TeleportRequest;
struct RequestPartySummon;
struct RequestPartySummonAnswer;
struct RequestRespawnAtPartner;

/**
 * @brief Moving party members to each other: going to one (TeleportRequest, the player list's "Go to") and summoning
 * one, who must say yes (RequestPartySummon, RequestPartySummonAnswer). Both only within a party, not for a player in
 * combat, and at most once per Gameplay:uPartyTeleportCooldown seconds for the player who asks.
 * And a player respawning under the death system is moved next to a party member (Gameplay:bRespawnAtPartner).
 */
struct PartyTeleportService
{
    PartyTeleportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;

    TP_NOCOPYMOVE(PartyTeleportService);

private:
    void OnTeleport(const PacketEvent<TeleportRequest>& acMessage) noexcept;
    void OnSummon(const PacketEvent<RequestPartySummon>& acMessage) noexcept;
    void OnSummonAnswer(const PacketEvent<RequestPartySummonAnswer>& acMessage) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;
    void OnRespawnAtPartner(const PacketEvent<RequestRespawnAtPartner>& acMessage) const noexcept;

    // Whether apAsker may move apMover to the other one, by the rules above; tells apAsker when not
    bool Allowed(const Player* apAsker, const Player* apMover, const Player* apOther) const noexcept;
    static void Refuse(const Player* apTo, PartyTeleport::Refusal aReason, const Player* apOther, uint32_t aSecondsLeft = 0) noexcept;

    World& m_world;
    // When each player last moved someone or themselves, by player id
    TiltedPhoques::Map<uint32_t, std::chrono::steady_clock::time_point> m_lastMoves;
    struct Summons
    {
        uint32_t SummonerId;
        std::chrono::steady_clock::time_point Time;
    };
    // The summons waiting for an answer, by the summoned player's id
    TiltedPhoques::Map<uint32_t, Summons> m_summons;

    entt::scoped_connection m_teleportConnection;
    entt::scoped_connection m_summonConnection;
    entt::scoped_connection m_summonAnswerConnection;
    entt::scoped_connection m_playerLeaveConnection;
    entt::scoped_connection m_respawnAtPartnerConnection;
};
