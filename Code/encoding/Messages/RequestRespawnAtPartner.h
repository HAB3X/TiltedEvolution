#pragma once

#include "Message.h"

// The player respawned under the death system: the server moves them next to a party member (TeleportCommandResponse)
struct RequestRespawnAtPartner final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestRespawnAtPartner;

    RequestRespawnAtPartner()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestRespawnAtPartner& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode(); }
};
