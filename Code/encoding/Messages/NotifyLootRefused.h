#pragma once

#include "Message.h"
#include <Structs/Inventory.h>

// Another player took some of what this player just took out of a container, at the same time: the container
// (ServerId) only had the rest. The receiver gives back the Item.Count it got that was no longer there.
struct NotifyLootRefused final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyLootRefused;

    NotifyLootRefused()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyLootRefused& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && ServerId == acRhs.ServerId && Item == acRhs.Item; }

    uint32_t ServerId{};
    // How many were refused, a positive count
    Inventory::Entry Item{};
};
