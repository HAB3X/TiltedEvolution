#pragma once

#include "Message.h"

// Every follower and the horse of this player near them, the whole list each time it changes: the server gives them to
// this player's game and keeps them there, and stops keeping those left off (RequestCompanions)
struct RequestCompanions final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestCompanions;

    RequestCompanions()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    struct Companion
    {
        uint32_t ServerId{};
        uint32_t ExpectedOwnershipEpoch{};

        bool operator==(const Companion& acRhs) const noexcept = default;
    };

    bool operator==(const RequestCompanions& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Companions == acRhs.Companions; }

    // More than a player has (followers, their horse, mods' extra followers); a longer list is a corrupt message
    static constexpr size_t kMaxCompanions = 32;

    TiltedPhoques::Vector<Companion> Companions;
};
