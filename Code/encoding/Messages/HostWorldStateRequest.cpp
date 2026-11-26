#include <Messages/HostWorldStateRequest.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void HostWorldStateRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
    State.Serialize(aWriter);
}

void HostWorldStateRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    State.Deserialize(aReader);
}
