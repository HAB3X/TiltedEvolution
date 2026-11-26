#pragma once

#include "Message.h"

// The host kicking a player, or banning them: from the server's own computer (Host a game) or an admin
struct RequestModeratePlayer final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestModeratePlayer;

    RequestModeratePlayer()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestModeratePlayer& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && Ban == acRhs.Ban; }

    uint32_t PlayerId{};
    // Also refuse their address from now on
    bool Ban{};
};
