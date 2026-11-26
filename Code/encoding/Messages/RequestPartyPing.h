#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Vector3_NetQuantize.h>

// The player pinged a spot or a target for their party (NotifyPartyPing)
struct RequestPartyPing final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPartyPing;

    RequestPartyPing()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPartyPing& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && WorldSpaceId == acRhs.WorldSpaceId && CellId == acRhs.CellId && Position == acRhs.Position && Target == acRhs.Target; }

    // Zero in an interior
    GameId WorldSpaceId{};
    GameId CellId{};
    Vector3_NetQuantize Position{};
    // What was pinged, in the pinger's game's language; empty for the spot they stand on
    TiltedPhoques::String Target;
};
