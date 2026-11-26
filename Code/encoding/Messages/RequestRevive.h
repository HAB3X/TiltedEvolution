#pragma once

#include "Message.h"

// The player activated a downed party member to get them back up (NotifyRevive)
struct RequestRevive final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestRevive;

    RequestRevive()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestRevive& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId; }

    // The downed player
    uint32_t PlayerId{};
};
