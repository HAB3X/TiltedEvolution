#pragma once

#include "Message.h"

// To a downed player: a party member got them back up, or everyone in the party is down, so they die as without one
struct NotifyRevive final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyRevive;

    enum class Outcome : uint8_t
    {
        kRevived,
        kPartyDown
    };

    NotifyRevive()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyRevive& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Result == acRhs.Result && ReviverId == acRhs.ReviverId && ReviverName == acRhs.ReviverName; }

    Outcome Result{Outcome::kRevived};
    // Who revived them; empty when the party is down
    uint32_t ReviverId{};
    TiltedPhoques::String ReviverName;
};
