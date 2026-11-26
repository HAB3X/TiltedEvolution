#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/GameId.h>

struct Player;
struct OwnerComponent
{
    OwnerComponent(Player* apPlayer, uint32_t aOwnershipEpoch = 1)
        : pOwner(apPlayer)
        , OwnershipEpoch(aOwnershipEpoch)
    {
    }

    Player* GetOwner() const { return reinterpret_cast<Player*>(pOwner); }

    void SetOwner(Player* apPlayer) { pOwner = apPlayer; }

    bool IsCurrentOwner(const Player* apPlayer, uint32_t aOwnershipEpoch) const noexcept
    {
        return apPlayer && GetOwner() == apPlayer && aOwnershipEpoch != 0 && OwnershipEpoch == aOwnershipEpoch;
    }

    Player* pOwner;
    uint32_t OwnershipEpoch;
    Vector<const Player*> InvalidOwners{};
    // Why the owner holds the actor for themselves, so nobody else may claim it
    enum class Hold : uint8_t
    {
        kNone,
        // A guard dealing with the owner's crime (RequestCrimeAuthority): only their game knows the bounty with
        // CrimeFaction, until it is gone
        kCrime,
        // The owner's follower or horse (RequestCompanions), while they name it
        kCompanion
    };

    // The player the actor is held for: always the owner, or null with kNone
    const Player* pHolder{};
    Hold HeldFor{Hold::kNone};
    GameId CrimeFaction{};

    void HoldFor(const Player* apPlayer, const Hold aHold) noexcept
    {
        pHolder = apPlayer;
        HeldFor = aHold;
    }
    void Release() noexcept { HoldFor(nullptr, Hold::kNone); }
};
