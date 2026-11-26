#pragma once

#include "Message.h"

struct RequestHostWorld final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestHostWorld;

    RequestHostWorld()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestHostWorld& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode(); }

};
