#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/Inventory.h>

#include <algorithm>

struct InventoryComponent
{
    Inventory Content{};

    // Two players taking the same items out of a container at once both get them in their game. The later one's
    // take finds the server's copy short; what other players took just before is that player's to give back. A
    // shortfall nobody else explains is the server not knowing the item, and the player keeps it, as before.
    static constexpr uint64_t kTakeRaceWindow = 2000;

    struct Take
    {
        uint32_t PlayerId;
        Inventory::Entry Item;
        uint64_t Tick;
    };

    // aPlayerId took aRequested (a positive count) of acItem at aNow (ms), of which the container had aTaken. Returns how
    // many to give back, and records the take.
    int32_t RefusedTake(const uint32_t aPlayerId, const Inventory::Entry& acItem, const int32_t aRequested, const int32_t aTaken, const uint64_t aNow) noexcept
    {
        std::erase_if(RecentTakes, [aNow](const Take& acTake) { return aNow > acTake.Tick + kTakeRaceWindow; });

        int32_t takenByOthers = 0;
        for (const Take& cTake : RecentTakes)
        {
            if (cTake.PlayerId != aPlayerId && cTake.Item.CanBeMerged(acItem))
                takenByOthers += cTake.Item.Count;
        }

        if (aTaken > 0)
        {
            Take take{aPlayerId, acItem, aNow};
            take.Item.Count = aTaken;
            RecentTakes.push_back(std::move(take));
        }

        return std::min(aRequested - aTaken, takenByOthers);
    }

    TiltedPhoques::Vector<Take> RecentTakes{};
};
