#include <Messages/RequestVote.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestVote::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(static_cast<uint8_t>(Kind), 8);
    Serialization::WriteVarInt(aWriter, TargetId);
}

void RequestVote::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    uint64_t bits = 0;
    aReader.ReadBits(bits, 8);
    Kind = static_cast<VoteKind>(bits);
    TargetId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
