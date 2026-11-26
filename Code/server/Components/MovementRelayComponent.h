#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <algorithm>
#include <cstdint>

/**
 * @brief How often an NPC's movement is relayed to each player, by distance: every snapshot nearby, fewer far away.
 *
 * Remote playback waits one snapshot interval per actor (see the client's PlaybackClock), so a far NPC just plays a
 * little further behind, never stutters: the far interval stays under its 250 ms delay cap. Players outside the loaded
 * grid get nothing, see CellIdComponent::IsInRange. Players' own characters always go out in full, see
 * CharacterService::OnReferencesMoveRequest.
 */
struct MovementRelayComponent
{
    enum class Band : uint8_t
    {
        Near,
        Mid,
        Far
    };

    // Game units (~70 per metre): about 30 m and 90 m
    static constexpr float kMidDistance = 2048.f;
    static constexpr float kFarDistance = 6144.f;
    // A band only changes this far past its edge, so an NPC pacing on the edge doesn't flip rates every snapshot
    static constexpr float kHysteresis = 256.f;
    // Shortest time between two relayed snapshots, in owner ticks (ms). Owners send every 30 ms in combat, about every
    // 90 ms otherwise, so mid distance gets at most every third combat snapshot and far at most every sixth.
    static constexpr uint64_t kMidInterval = 90;
    static constexpr uint64_t kFarInterval = 180;
    // A receiver not relayed to for this long is forgotten (it left, or went out of range)
    static constexpr uint64_t kForgetAfter = 10000;

    struct Receiver
    {
        uint32_t PlayerId;
        Band CurrentBand;
        uint64_t LastTick;
    };

    [[nodiscard]] static Band BandAt(const float aDistance, const float aEdgeShift) noexcept
    {
        if (aDistance < kMidDistance + aEdgeShift)
            return Band::Near;
        return aDistance < kFarDistance + aEdgeShift ? Band::Mid : Band::Far;
    }

    // The band at aDistance for a receiver now in aCurrent: it stays unless the distance is past an edge by kHysteresis
    [[nodiscard]] static Band NextBand(const Band aCurrent, const float aDistance) noexcept
    {
        return std::clamp(aCurrent, BandAt(aDistance, kHysteresis), BandAt(aDistance, -kHysteresis));
    }

    [[nodiscard]] static uint64_t IntervalOf(const Band aBand) noexcept
    {
        switch (aBand)
        {
        case Band::Mid: return kMidInterval;
        case Band::Far: return kFarInterval;
        default: return 0;
        }
    }

    // Whether the snapshot captured at aTick goes to the player aPlayerId, aDistance away from the NPC. Ticks only grow,
    // the server drops older snapshots before this.
    bool ShouldRelay(const uint32_t aPlayerId, const float aDistance, const uint64_t aTick) noexcept
    {
        auto it = std::find_if(Receivers.begin(), Receivers.end(), [aPlayerId](const Receiver& acReceiver) { return acReceiver.PlayerId == aPlayerId; });
        if (it == Receivers.end())
        {
            std::erase_if(Receivers, [aTick](const Receiver& acReceiver) { return aTick > acReceiver.LastTick + kForgetAfter; });
            Receivers.push_back({aPlayerId, BandAt(aDistance, 0.f), aTick});
            return true;
        }

        it->CurrentBand = NextBand(it->CurrentBand, aDistance);
        if (aTick < it->LastTick + IntervalOf(it->CurrentBand))
            return false;

        it->LastTick = aTick;
        return true;
    }

    TiltedPhoques::Vector<Receiver> Receivers;
};
