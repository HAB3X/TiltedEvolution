#pragma once

#include "Message.h"

#include <Structs/PartyVitals.h>

struct RequestPlayerHealthUpdate final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPlayerHealthUpdate;

    RequestPlayerHealthUpdate()
        : ClientMessage(Opcode)
    {
    }

    virtual ~RequestPlayerHealthUpdate() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPlayerHealthUpdate& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Percentage == acRhs.Percentage && Vitals == acRhs.Vitals; }

    float Percentage{};
    // Not on official v1.8.4's wire
    PartyVitals Vitals{};
};
