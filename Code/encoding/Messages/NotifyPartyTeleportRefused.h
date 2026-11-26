#pragma once

#include "Message.h"

// A go-to or summons didn't happen, and why (PartyTeleport::Refusal)
struct NotifyPartyTeleportRefused final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartyTeleportRefused;

    NotifyPartyTeleportRefused()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartyTeleportRefused& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Reason == acRhs.Reason && PlayerName == acRhs.PlayerName && SecondsLeft == acRhs.SecondsLeft; }

    // A PartyTeleport::Refusal
    uint32_t Reason{};
    // The other player
    TiltedPhoques::String PlayerName;
    // For a cooldown: how long until the next move
    uint32_t SecondsLeft{};
};
