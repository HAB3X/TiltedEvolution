#pragma once

#include "Message.h"

// A player's answer to the open vote: yes (kick, or ready) or no
struct RequestVoteAnswer final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestVoteAnswer;

    RequestVoteAnswer()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestVoteAnswer& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && VoteId == acRhs.VoteId && Yes == acRhs.Yes; }

    uint32_t VoteId{};
    bool Yes{};
};
