#pragma once

#include "Message.h"

struct NotifyHostWorldRequested final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyHostWorldRequested;

    NotifyHostWorldRequested()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyHostWorldRequested& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId; }

    // The player who joined and wants the host's world
    uint32_t PlayerId{};
};
