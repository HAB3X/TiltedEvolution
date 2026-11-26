#include <Messages/NotifyPlayerHealthUpdate.h>
#include <TiltedCore/Serialization.hpp>
#include <WireProtocol.h>

void NotifyPlayerHealthUpdate::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteFloat(aWriter, Percentage);
    if (!Wire::IsOfficial184())
        Vitals.Serialize(aWriter);
}

void NotifyPlayerHealthUpdate::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Percentage = Serialization::ReadFloat(aReader);
    if (!Wire::IsOfficial184())
        Vitals.Deserialize(aReader);
}
