#pragma once

#include "Message.h"
#include <Structs/GameId.h>

// The player's bounty with a crime faction (a hold), and the guards of that faction near them that their game should
// run so the guards deal with this player's crimes. A bounty of 0 ends this player's hold on that faction's guards.
struct RequestCrimeAuthority final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestCrimeAuthority;

    RequestCrimeAuthority()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    struct Guard
    {
        uint32_t ServerId{};
        uint32_t ExpectedOwnershipEpoch{};

        bool operator==(const Guard& acRhs) const noexcept = default;
    };

    bool operator==(const RequestCrimeAuthority& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Faction == acRhs.Faction && Bounty == acRhs.Bounty && Guards == acRhs.Guards; }

    // Guards one message names at most (the rest wait for the next one)
    static constexpr size_t kMaxGuards = 64;

    GameId Faction{};
    uint32_t Bounty{};
    TiltedPhoques::Vector<Guard> Guards;
};
