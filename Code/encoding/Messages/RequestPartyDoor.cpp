#include <Messages/RequestPartyDoor.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestPartyDoor::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    WorldSpaceId.Serialize(aWriter);
    CellId.Serialize(aWriter);
    Position.Serialize(aWriter);
    Serialization::WriteString(aWriter, Place);
}

void RequestPartyDoor::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    WorldSpaceId.Deserialize(aReader);
    CellId.Deserialize(aReader);
    Position.Deserialize(aReader);
    Place = Serialization::ReadString(aReader);
}
