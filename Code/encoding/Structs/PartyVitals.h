#pragma once

#include <TiltedCore/Buffer.hpp>

#include <cstdint>

// The rest of a party member's bar beside their health percentage, in RequestPlayerHealthUpdate and
// NotifyPlayerHealthUpdate. Official v1.8.4 has none of it, so the messages carry it only to this build's servers.
struct PartyVitals
{
    // Whole percents, 0 to 100
    uint8_t Magicka{};
    uint8_t Stamina{};
    bool InCombat{};
    // Bleeding out under the death system, waiting to get up or respawn
    bool Downed{};

    bool operator==(const PartyVitals&) const noexcept = default;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;
};
