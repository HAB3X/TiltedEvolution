#pragma once

#include <Events/EventDispatcher.h>
#include <Games/Events.h>

struct World;
struct TransportService;
struct NotifyPartyTravel;

/**
 * @brief Travel together: when the party leader fast travels, members are asked whether to go with them, and
 * those who say yes are moved next to the leader (the same teleport as the party's "teleport to player").
 */
struct TravelTogetherService final : BSTEventSink<TESFastTravelEndEvent>
{
    TravelTogetherService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~TravelTogetherService() override = default;

    TP_NOCOPYMOVE(TravelTogetherService);

    // The member's answer to the overlay's prompt
    void Answer(bool aTravel) noexcept;

private:
    BSTEventResult OnEvent(const TESFastTravelEndEvent*, const EventDispatcher<TESFastTravelEndEvent>*) override;
    void OnPartyTravel(const NotifyPartyTravel& acMessage) noexcept;
    void OfferTravel() const noexcept;

    World& m_world;
    TransportService& m_transport;
    // The leader whose travel the prompt offers, empty when nothing is offered
    TiltedPhoques::String m_offeredLeader;

    entt::scoped_connection m_partyTravelConnection;
};
