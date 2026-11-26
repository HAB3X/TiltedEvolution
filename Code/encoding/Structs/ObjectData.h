#pragma once

#include <Structs/GameId.h>
#include <Structs/LockData.h>
#include <Structs/Inventory.h>
#include <Structs/GridCellCoords.h>

struct ObjectData
{
    ObjectData() = default;
    ~ObjectData() = default;

    bool operator==(const ObjectData& acRhs) const noexcept;
    bool operator!=(const ObjectData& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    // A door's state, in the game's open state numbers (TESObjectREFR::OpenState), which ActivateRequest also carries
    static constexpr uint8_t kNotADoor = 0;
    static constexpr uint8_t kDoorOpen = 1;
    static constexpr uint8_t kDoorOpening = 2;
    static constexpr uint8_t kDoorClosed = 3;
    static constexpr uint8_t kDoorClosing = 4;

    // Where a door is headed: opening counts as open, closing as closed
    [[nodiscard]] static uint8_t SettledDoorState(const uint8_t aOpenState) noexcept
    {
        if (aOpenState == kDoorOpen || aOpenState == kDoorOpening)
            return kDoorOpen;
        if (aOpenState == kDoorClosed || aOpenState == kDoorClosing)
            return kDoorClosed;
        return kNotADoor;
    }

    // Where a door activated in aOpenState ends up: activating swaps open and closed
    [[nodiscard]] static uint8_t DoorStateAfterActivation(const uint8_t aOpenState) noexcept
    {
        switch (SettledDoorState(aOpenState))
        {
        case kDoorOpen: return kDoorClosed;
        case kDoorClosed: return kDoorOpen;
        default: return kNotADoor;
        }
    }

    uint32_t ServerId{};
    GameId Id{};
    GameId CellId{};
    GameId WorldSpaceId{};
    GridCellCoords CurrentCoords{};
    LockData CurrentLockData{};
    Inventory CurrentInventory{};
    bool IsSenderFirst{};
    // Containers keep their contents on the server after everyone leaves the cell, doors do not.
    bool IsContainer{};
    // Storage containers (player homes) merge each player's local contents into the shared server state
    // the first time that player sees them, so nobody's stored items get wiped by someone else's copy.
    bool MergeInventory{};
    // Doors: open or closed (SettledDoorState), so a player entering the cell sees doors as the others left them
    uint8_t DoorState{kNotADoor};
};
