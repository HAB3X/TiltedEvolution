#include <Messages/NotifyPartyTravel.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyPartyTravel::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, LeaderId);
    Serialization::WriteString(aWriter, LeaderName);
    Serialization::WriteString(aWriter, Place);
}

void NotifyPartyTravel::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    LeaderId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    LeaderName = Serialization::ReadString(aReader);
    Place = Serialization::ReadString(aReader);
}
