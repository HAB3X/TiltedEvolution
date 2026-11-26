#pragma once

#include "Message.h"

// The player asks a party member to come to them (NotifyPartySummon)
struct RequestPartySummon final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPartySummon;

    RequestPartySummon()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPartySummon& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId; }

    // The member to summon
    uint32_t PlayerId{};
};
