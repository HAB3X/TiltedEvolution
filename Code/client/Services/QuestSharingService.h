#pragma once

struct World;
struct TransportService;
struct PartyLeftEvent;
struct DisconnectedEvent;
struct NotifyPartyQuestSharing;
struct NotifyQuestShareOffer;

/**
 * @brief The party's quest sharing, as its leader sets it, and quests offered to the party one at a time.
 *
 * The leader turns sharing on or off and picks the quest kinds the party shares (RequestPartyQuestSharing); the server
 * keeps it per party, starting from the host's Gameplay:sSharedQuestTypes, and tells each member
 * (NotifyPartyQuestSharing). Any member can offer one of their running quests to the others even when its kind isn't
 * shared (RequestQuestShare); a member who accepts gets its current stage and its updates from then on.
 */
struct QuestSharingService
{
    QuestSharingService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~QuestSharingService() = default;

    TP_NOCOPYMOVE(QuestSharingService);

    // The quest kinds the party shares, a SharedQuests mask; the server's outside a party
    [[nodiscard]] uint32_t GetSharedQuestTypes() const noexcept;

    // The leader's choice, from the overlay's party menu
    void SetSharing(bool aEnabled, uint32_t aTypes) const noexcept;
    // Sends the overlay the running quests the party doesn't share by kind, which this player can offer
    void ListShareableQuests() const noexcept;
    // Offers the quest to the party
    void Share(uint32_t aQuestFormId) const noexcept;
    // The answer to a party member's offer, from the overlay
    void Answer(uint32_t aQuestFormId, bool aAccept) const noexcept;

private:
    void OnQuestSharing(const NotifyPartyQuestSharing& acMessage) noexcept;
    void OnQuestShareOffer(const NotifyQuestShareOffer& acMessage) const noexcept;
    void OnPartyLeft(const PartyLeftEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;

    World& m_world;
    TransportService& m_transport;
    // From the server, while in a party
    bool m_partyKnown{false};
    bool m_enabled{true};
    uint32_t m_types{};

    entt::scoped_connection m_questSharingConnection;
    entt::scoped_connection m_questShareOfferConnection;
    entt::scoped_connection m_partyLeftConnection;
    entt::scoped_connection m_disconnectedConnection;
};
