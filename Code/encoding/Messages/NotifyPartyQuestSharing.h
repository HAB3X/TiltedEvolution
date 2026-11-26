#pragma once

#include "Message.h"

// The party's quest sharing (RequestPartyQuestSharing), sent to a player joining a party and to the party when the
// leader changes it
struct NotifyPartyQuestSharing final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartyQuestSharing;

    NotifyPartyQuestSharing()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartyQuestSharing& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Enabled == acRhs.Enabled && Types == acRhs.Types; }

    // Off: no quest is shared by its kind
    bool Enabled{};
    // A SharedQuests mask, kept while sharing is off
    uint32_t Types{};
};
