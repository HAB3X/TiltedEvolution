#include <Messages/NotifyVote.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyVote::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, VoteId);
    aWriter.WriteBits(static_cast<uint8_t>(Kind), 8);
    aWriter.WriteBits(static_cast<uint8_t>(State), 8);
    Serialization::WriteVarInt(aWriter, StarterId);
    Serialization::WriteVarInt(aWriter, TargetId);
    Serialization::WriteVarInt(aWriter, Yes);
    Serialization::WriteVarInt(aWriter, No);
    Serialization::WriteVarInt(aWriter, Eligible);
    Serialization::WriteVarInt(aWriter, SecondsLeft);
}

void NotifyVote::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    VoteId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    uint64_t bits = 0;
    aReader.ReadBits(bits, 8);
    Kind = static_cast<VoteKind>(bits);
    aReader.ReadBits(bits, 8);
    State = static_cast<VoteState>(bits);
    StarterId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    TargetId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Yes = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    No = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Eligible = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    SecondsLeft = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
