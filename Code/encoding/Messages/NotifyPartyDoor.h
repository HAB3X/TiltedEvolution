#pragma once

#include "Message.h"
#include <Structs/GameId.h>
#include <Structs/Vector3_NetQuantize.h>

// A party member went through a load door: follow them through?
struct NotifyPartyDoor final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartyDoor;

    NotifyPartyDoor()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartyDoor& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && PlayerName == acRhs.PlayerName && WorldSpaceId == acRhs.WorldSpaceId && CellId == acRhs.CellId && Position == acRhs.Position && Place == acRhs.Place; }

    uint32_t PlayerId{};
    TiltedPhoques::String PlayerName;
    // Where the door leads: zero for an interior
    GameId WorldSpaceId{};
    GameId CellId{};
    Vector3_NetQuantize Position{};
    // The destination's name, in the member's game's language
    TiltedPhoques::String Place;
};
