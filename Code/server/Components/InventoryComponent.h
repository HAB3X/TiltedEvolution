#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/Inventory.h>
#include <TiltedCore/Stl.hpp>

#include <algorithm>

struct InventoryComponent
{
    Inventory Content{};

    // Per player loot (Gameplay:bPerPlayerLoot): each player's own copy of a container, by name so it outlives a
    // reconnect. A player who hasn't changed the container yet sees Content, as it was when first seen.
    TiltedPhoques::Map<String, Inventory> PlayerCopies{};

    [[nodiscard]] const Inventory& ContentFor(const String& acPlayerName) const noexcept
    {
        const auto cIt = PlayerCopies.find(acPlayerName);
        return cIt != PlayerCopies.end() ? cIt->second : Content;
    }

    // Applies a player's change to their own copy, as far as it has the items; returns how much it applied
    int32_t ChangePlayerCopy(const String& acPlayerName, const Inventory::Entry& acItem) noexcept
    {
        auto it = PlayerCopies.try_emplace(acPlayerName, Content).first;
        return it.value().AddOrRemoveEntryClamped(acItem);
    }

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
