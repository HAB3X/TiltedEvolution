#pragma once

#include <Events/EventDispatcher.h>
#include <Games/Events.h>

#include <Messages/TeleportCommandResponse.h>

struct World;
struct TransportService;
struct NotifyPartyTravel;
struct NotifyPartyDoor;
struct NotifyPartySummon;
struct NotifyPartyTeleportRefused;

/**
 * @brief Travel together: when the party leader fast travels, members are asked whether to go with them, and
 * those who say yes are moved next to the leader (the same teleport as the party's "teleport to player").
 * Load doors the same way: when the leader (or any member, Gameplay:bPartyDoorsAnyMember) goes through one, the
 * others are asked whether to follow, and those who say yes are moved to where the door leads.
 * And summons: a member asks another to come to them, who is asked and moved there on yes (the server decides,
 * PartyTeleportService).
 */
struct TravelTogetherService final : BSTEventSink<TESFastTravelEndEvent>
{
    TravelTogetherService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~TravelTogetherService() override = default;

    TP_NOCOPYMOVE(TravelTogetherService);

    // The member's answer to the overlay's prompt
    void Answer(bool aTravel) noexcept;

    // The local player went through a load door into this cell, arriving here: offers the party to follow
    void OfferDoor(uint32_t aCellId, const NiPoint3& acPosition) const noexcept;
    // The member's answer to the overlay's door prompt
    void AnswerDoor(bool aFollow) noexcept;

    // Asks a party member to come to this player
    void Summon(uint32_t aPlayerId) const noexcept;
    // The member's answer to the overlay's summons prompt
    void AnswerSummon(uint32_t aSummonerId, bool aAccept) const noexcept;

private:
    BSTEventResult OnEvent(const TESFastTravelEndEvent*, const EventDispatcher<TESFastTravelEndEvent>*) override;
    void OnPartyTravel(const NotifyPartyTravel& acMessage) noexcept;
    void OnPartyDoor(const NotifyPartyDoor& acMessage) noexcept;
    void OnPartySummon(const NotifyPartySummon& acMessage) const noexcept;
    void OnTeleportRefused(const NotifyPartyTeleportRefused& acMessage) const noexcept;
    void OfferTravel() const noexcept;

    World& m_world;
    TransportService& m_transport;
    // The leader whose travel the prompt offers, empty when nothing is offered
    TiltedPhoques::String m_offeredLeader;
    // Where the door the prompt offers leads, empty when nothing is offered
    std::optional<TeleportCommandResponse> m_offeredDoor;

    entt::scoped_connection m_partyTravelConnection;
    entt::scoped_connection m_partyDoorConnection;
    entt::scoped_connection m_partySummonConnection;
    entt::scoped_connection m_teleportRefusedConnection;
};
