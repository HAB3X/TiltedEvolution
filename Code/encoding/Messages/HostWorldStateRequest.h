#pragma once

#include "Message.h"
#include <Structs/HostWorldState.h>

struct HostWorldStateRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kHostWorldStateRequest;

    HostWorldStateRequest()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const HostWorldStateRequest& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && State == acRhs.State; }

    // The joiner the state is for, from NotifyHostWorldRequested
    uint32_t PlayerId{};
    HostWorldState State{};
};
