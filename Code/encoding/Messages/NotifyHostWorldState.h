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

    bool operator==(const NotifyHostWorldState& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && HostName == acRhs.HostName && State == acRhs.State; }

    // Whose world it is, for the joiner's summary
    String HostName{};
    HostWorldState State{};
};
