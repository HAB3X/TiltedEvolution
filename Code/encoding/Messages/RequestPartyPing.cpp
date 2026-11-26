#include <Messages/RequestPartyPing.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestPartyPing::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    WorldSpaceId.Serialize(aWriter);
    CellId.Serialize(aWriter);
    Position.Serialize(aWriter);
    Serialization::WriteString(aWriter, Target);
}

void RequestPartyPing::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    WorldSpaceId.Deserialize(aReader);
    CellId.Deserialize(aReader);
    Position.Deserialize(aReader);
    Target = Serialization::ReadString(aReader);
}
