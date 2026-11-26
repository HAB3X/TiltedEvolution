#pragma once

#include "Message.h"

// The party leader turns quest sharing on or off for their party, or changes the kinds it shares, live. The server
// keeps it per party (a new party starts with the host's Gameplay:sSharedQuestTypes) and tells the party with
// NotifyPartyQuestSharing.
struct RequestPartyQuestSharing final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestPartyQuestSharing;

    RequestPartyQuestSharing()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestPartyQuestSharing& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Enabled == acRhs.Enabled && Types == acRhs.Types; }

    // Off: the party shares no quest by its kind, only those offered with RequestQuestShare
    bool Enabled{};
    // A SharedQuests mask
    uint32_t Types{};
};
