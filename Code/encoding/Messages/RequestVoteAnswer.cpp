#include <Messages/RequestVoteAnswer.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestVoteAnswer::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, VoteId);
    Serialization::WriteBool(aWriter, Yes);
}

void RequestVoteAnswer::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    VoteId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Yes = Serialization::ReadBool(aReader);
}
