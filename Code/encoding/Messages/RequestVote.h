#pragma once

#include "Message.h"
#include <Messages/NotifyVote.h>

// A player starts a vote: to kick TargetId, or a ready check (VoteService on the server)
struct RequestVote final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestVote;

    RequestVote()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestVote& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Kind == acRhs.Kind && TargetId == acRhs.TargetId; }

    VoteKind Kind{};
    uint32_t TargetId{};
};
