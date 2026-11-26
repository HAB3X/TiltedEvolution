#pragma once

#include <chrono>
#include <cstdint>

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
} // namespace PartyTeleport
