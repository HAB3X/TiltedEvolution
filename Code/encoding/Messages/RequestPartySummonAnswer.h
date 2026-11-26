#pragma once

#include "Message.h"

// The player's answer to a party member's summons (NotifyPartySummon)
struct RequestPartySummonAnswer final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPartySummonAnswer;

    RequestPartySummonAnswer()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPartySummonAnswer& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && Accept == acRhs.Accept; }

    // Who summoned
    uint32_t PlayerId{};
    bool Accept{};
};
