#pragma once

#include "Message.h"

// The party leader fast travelled: members are asked whether to travel with them
struct NotifyPartyTravel final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartyTravel;

    NotifyPartyTravel()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartyTravel& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && LeaderId == acRhs.LeaderId && LeaderName == acRhs.LeaderName && Place == acRhs.Place;
    }

    uint32_t LeaderId{};
    TiltedPhoques::String LeaderName;
    // Where the leader arrived, in their game's language
    TiltedPhoques::String Place;
};
