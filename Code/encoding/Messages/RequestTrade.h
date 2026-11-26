#pragma once

#include "Message.h"
#include <Structs/Inventory.h>

// Player trading with a party member: this side's offer (which unconfirms both sides), a confirm, or a cancel (NotifyTrade)
struct RequestTrade final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestTrade;

    enum class Action : uint8_t
    {
        kOffer,
        kConfirm,
        kCancel
    };

    RequestTrade()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestTrade& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PartnerId == acRhs.PartnerId && Act == acRhs.Act && Offer == acRhs.Offer; }

    uint32_t PartnerId{};
    Action Act{Action::kOffer};
    // kOffer: everything this side gives, as it should be now
    Inventory Offer{};
};
