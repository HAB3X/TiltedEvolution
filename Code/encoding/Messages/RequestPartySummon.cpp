#include <Messages/RequestPartySummon.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestPartySummon::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
}

void RequestPartySummon::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
