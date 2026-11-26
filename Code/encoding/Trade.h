#pragma once

#include <Structs/Inventory.h>

#include <algorithm>

// Player trading: what one side of a trade may offer, and whether the server's copy of their inventory has it.
namespace Trade
{
// Most stacks one side offers
constexpr size_t kMaxItems = 32;

// An offer's items: whole counts, at most kMaxItems stacks, and nothing worn or belonging to a quest
inline bool IsValidOffer(const Inventory& acOffer) noexcept
{
    if (acOffer.Entries.size() > kMaxItems)
        return false;
    return std::all_of(acOffer.Entries.begin(), acOffer.Entries.end(),
                       [](const Inventory::Entry& acEntry) { return acEntry.Count > 0 && !acEntry.ExtraWorn && !acEntry.ExtraWornLeft && !acEntry.IsQuestItem; });
}

// Whether acInventory holds everything in acOffer, stacks that merge counted together
inline bool Holds(const Inventory& acInventory, const Inventory& acOffer) noexcept
{
    for (const Inventory::Entry& cOffered : acOffer.Entries)
    {
        int64_t offered = 0;
        for (const Inventory::Entry& cOther : acOffer.Entries)
        {
            if (cOther.CanBeMerged(cOffered))
                offered += cOther.Count;
        }
        int64_t held = 0;
        for (const Inventory::Entry& cHeld : acInventory.Entries)
        {
            if (cHeld.CanBeMerged(cOffered))
                held += cHeld.Count;
        }
        if (held < offered)
            return false;
    }
    return true;
}
} // namespace Trade
