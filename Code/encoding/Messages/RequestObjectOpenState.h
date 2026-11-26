#pragma once

#include "Message.h"
#include <Structs/GameId.h>

// A door or activator in the player's cell is now open or closed (ObjectData::SettledDoorState), whatever opened it,
// a script included: kept by the server for players entering the cell later, and not relayed (each game runs the
// activation that did it)
struct RequestObjectOpenState final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestObjectOpenState;

    RequestObjectOpenState()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestObjectOpenState& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Id == acRhs.Id && CellId == acRhs.CellId && State == acRhs.State; }

    GameId Id{};
    GameId CellId{};
    uint8_t State{};
};
