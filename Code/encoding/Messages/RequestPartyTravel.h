#pragma once

#include "Message.h"

// The party leader finished a fast travel: the server offers the party to follow (NotifyPartyTravel)
struct RequestPartyTravel final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPartyTravel;

    RequestPartyTravel()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPartyTravel& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Place == acRhs.Place; }

    // Where the leader arrived, in their game's language
    TiltedPhoques::String Place;
};
