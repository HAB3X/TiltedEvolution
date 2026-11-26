#include <Messages/RequestPartySummonAnswer.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestPartySummonAnswer::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteBool(aWriter, Accept);
}

void RequestPartySummonAnswer::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Accept = Serialization::ReadBool(aReader);
}
