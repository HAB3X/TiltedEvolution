#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Vector3_NetQuantize.h>

// The player went through a load door: the server offers the party to follow (NotifyPartyDoor)
struct RequestPartyDoor final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPartyDoor;

    RequestPartyDoor()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPartyDoor& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && WorldSpaceId == acRhs.WorldSpaceId && CellId == acRhs.CellId && Position == acRhs.Position && Place == acRhs.Place; }

    // Where the door leads: zero for an interior
    GameId WorldSpaceId{};
    GameId CellId{};
    Vector3_NetQuantize Position{};
    // The destination's name, in the player's game's language
    TiltedPhoques::String Place;
};
