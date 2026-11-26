#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

// Moving party members to each other (the player list's "Go to" and "Summon"): when the server allows it.
namespace PartyTeleport
{
// Why a move didn't happen, as NotifyPartyTeleportRefused carries it
enum Refusal : uint8_t
{
    kAllowed,
    kNotInParty,
    // The player who would be moved is fighting: no escaping a fight this way
    kInCombat,
    kCooldown,
    kDeclined,
    // The other player left, or has no character yet
    kUnavailable,
};

// Whether the player who asked may move someone, or be moved: both in the same party, the one who would move not in
// combat, and the asker's last move at least a cooldown ago
inline Refusal Check(bool aSameParty, bool aMoverInCombat, std::chrono::steady_clock::duration aSinceLastMove,
                     std::chrono::steady_clock::duration aCooldown) noexcept
{
    if (!aSameParty)
        return kNotInParty;
    if (aMoverInCombat)
        return kInCombat;
    if (aSinceLastMove < aCooldown)
        return kCooldown;
    return kAllowed;
}
// A party member a respawning player could join
struct Partner
{
    uint32_t PlayerId;
    // Whether the server knows where they are (a character with a position)
    bool HasCharacter;
    bool InCombat;
};

// Whom a player respawning under the death system joins: the first member out of combat, else the first in combat;
// nobody when no member has a character to stand next to
inline std::optional<uint32_t> RespawnPartner(const std::vector<Partner>& acMembers) noexcept
{
    std::optional<uint32_t> inCombat;
    for (const Partner& cMember : acMembers)
    {
        if (!cMember.HasCharacter)
            continue;
        if (!cMember.InCombat)
            return cMember.PlayerId;
        if (!inCombat)
            inCombat = cMember.PlayerId;
    }
    return inCombat;
}
} // namespace PartyTeleport
