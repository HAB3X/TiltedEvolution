#pragma once

#include "Message.h"
#include <Structs/Inventory.h>

// The state of a trade with a party member (RequestTrade). On kCompleted the server has moved the items itself:
// each side's game gets them as ordinary inventory changes of its character.
struct NotifyTrade final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyTrade;

    enum class Event : uint8_t
    {
        // Either side's offer or confirmation changed
        kUpdated,
        kCancelled,
        kCompleted,
        // One side no longer has what it offered: both are unconfirmed
        kMissingItems,
        // Not a party member in range, or already trading with someone else
        kUnavailable
    };

    NotifyTrade()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyTrade& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && PartnerId == acRhs.PartnerId && PartnerName == acRhs.PartnerName && What == acRhs.What && MyOffer == acRhs.MyOffer &&
               TheirOffer == acRhs.TheirOffer && MyConfirmed == acRhs.MyConfirmed && TheirConfirmed == acRhs.TheirConfirmed;
    }

    uint32_t PartnerId{};
    TiltedPhoques::String PartnerName;
    Event What{Event::kUpdated};
    Inventory MyOffer{};
    Inventory TheirOffer{};
    bool MyConfirmed{};
    bool TheirConfirmed{};
};
