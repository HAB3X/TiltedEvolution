#include <Messages/NotifyHostWorldRequested.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyHostWorldRequested::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
}

void NotifyHostWorldRequested::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
