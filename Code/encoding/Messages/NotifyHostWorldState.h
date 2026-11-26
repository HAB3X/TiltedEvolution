#pragma once

#include "Message.h"
#include <Structs/HostWorldState.h>

struct NotifyHostWorldState final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyHostWorldState;

    NotifyHostWorldState()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyHostWorldState& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && State == acRhs.State; }

    HostWorldState State{};
};
