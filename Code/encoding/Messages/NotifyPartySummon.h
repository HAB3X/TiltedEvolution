#pragma once

#include "Message.h"

// A party member asks this player to come to them: RequestPartySummonAnswer
struct NotifyPartySummon final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartySummon;

    NotifyPartySummon()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartySummon& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && PlayerName == acRhs.PlayerName; }

    // Who summons
    uint32_t PlayerId{};
    TiltedPhoques::String PlayerName;
};
