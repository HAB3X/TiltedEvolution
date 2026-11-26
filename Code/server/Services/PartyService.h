#pragma once

#include <Events/PacketEvent.h>

struct World;
struct UpdateEvent;
struct PlayerJoinEvent;
struct PlayerLeaveEvent;
struct PartyInviteRequest;
struct PartyAcceptInviteRequest;
struct PartyLeaveRequest;
struct NotifyPartyInfo;
struct PartyCreateRequest;
struct PartyChangeLeaderRequest;
struct PartyKickRequest;
struct RequestPartyTravel;
struct RequestPartyDoor;

/**
 * @brief Manages every party in the server.
 */
struct PartyService
{
    struct Party
    {
        uint32_t LeaderPlayerId{0}; // PlayerIDs start at 1, so zero is "no leader."
        Vector<Player*> Members;
        GameId CachedWeather{};
        // Stages each quest reached since it started, as relayed to the party
        Map<GameId, TiltedPhoques::Set<uint16_t>> QuestStages{};

        // The quest kinds the party shares, set by its leader (RequestPartyQuestSharing); the host's until then
        bool QuestSharingEnabled{true};
        std::optional<uint32_t> SharedQuestTypes{};
        // A quest a member offered the party although its kind isn't shared (RequestQuestShare)
        struct OfferedQuest
        {
            // The players running it together, whose updates of it are relayed among them
            TiltedPhoques::Set<uint32_t> Players{};
            // Those asked who haven't answered yet
            TiltedPhoques::Set<uint32_t> Asked{};
            uint16_t Stage{};
            uint8_t ClientQuestType{};
        };
        Map<GameId, OfferedQuest> OfferedQuests{};
    };

    PartyService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~PartyService() noexcept = default;

    TP_NOCOPYMOVE(PartyService);

    const Party* GetById(uint32_t aId) const noexcept;
    bool IsPlayerInParty(Player* const apPlayer) const noexcept;
    bool IsPlayerLeader(const Player* const apPlayer) const noexcept;
    Party* GetPlayerParty(Player* const apPlayer) noexcept;

protected:
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnPlayerJoin(const PlayerJoinEvent& acEvent) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;
    void OnPartyInvite(const PacketEvent<PartyInviteRequest>& acPacket) noexcept;
    void OnPartyAcceptInvite(const PacketEvent<PartyAcceptInviteRequest>& acPacket) noexcept;
    void OnPartyLeave(const PacketEvent<PartyLeaveRequest>& acPacket) noexcept;
    void OnPartyCreate(const PacketEvent<PartyCreateRequest>& acPacket) noexcept;
    void CreateParty(Player* apPlayer) noexcept;
    void OnPartyChangeLeader(const PacketEvent<PartyChangeLeaderRequest>& acPacket) noexcept;
    void OnPartyKick(const PacketEvent<PartyKickRequest>& acPacket) noexcept;
    void OnPartyTravel(const PacketEvent<RequestPartyTravel>& acPacket) const noexcept;
    void OnPartyDoor(const PacketEvent<RequestPartyDoor>& acPacket) const noexcept;
    void RemovePlayerFromParty(Player* apPlayer) noexcept;
    void RemoveMemberFromParty(Party& aParty, uint32_t aPartyId, Player* apPlayer) noexcept;

    void BroadcastPlayerList(Player* apPlayer = nullptr) const noexcept;
    void BroadcastPartyInfo(uint32_t aPartyId) const noexcept;

private:
    World& m_world;

    TiltedPhoques::Map<uint32_t, Party> m_parties;
    uint32_t m_nextId{0};
    uint64_t m_nextInvitationExpire{0};

    entt::scoped_connection m_updateEvent;
    entt::scoped_connection m_playerJoinConnection;
    entt::scoped_connection m_playerLeaveConnection;
    entt::scoped_connection m_partyInviteConnection;
    entt::scoped_connection m_partyAcceptInviteConnection;
    entt::scoped_connection m_partyLeaveConnection;
    entt::scoped_connection m_partyCreateConnection;
    entt::scoped_connection m_partyChangeLeaderConnection;
    entt::scoped_connection m_partyKickConnection;
    entt::scoped_connection m_partyTravelConnection;
    entt::scoped_connection m_partyDoorConnection;

    void SendPartyJoinedEvent(Party& aParty, Player* aPlayer) noexcept;
};
