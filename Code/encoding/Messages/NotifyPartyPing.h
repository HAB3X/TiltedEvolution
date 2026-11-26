#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Vector3_NetQuantize.h>

// A party member pinged a spot or a target: a waypoint and a chat line
struct NotifyPartyPing final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartyPing;

    NotifyPartyPing()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartyPing& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && PlayerName == acRhs.PlayerName && WorldSpaceId == acRhs.WorldSpaceId && CellId == acRhs.CellId && Position == acRhs.Position && Target == acRhs.Target; }

    uint32_t PlayerId{};
    TiltedPhoques::String PlayerName;
    // Zero in an interior
    GameId WorldSpaceId{};
    GameId CellId{};
    Vector3_NetQuantize Position{};
    // What was pinged, in the pinger's game's language; empty for the spot they stood on
    TiltedPhoques::String Target;
};
