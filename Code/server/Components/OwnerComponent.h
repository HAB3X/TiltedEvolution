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
    // A guard whose owner holds it for their own crime (RequestCrimeAuthority): only that owner's game knows the bounty,
    // so nobody else may claim the guard until that player's bounty with CrimeFaction is gone. Always the owner or null.
    const Player* pCrimeHolder{};
    GameId CrimeFaction{};
};
