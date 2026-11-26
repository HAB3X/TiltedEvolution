#include <Messages/RequestPlayerHealthUpdate.h>
#include <TiltedCore/Serialization.hpp>
#include <WireProtocol.h>

void RequestPlayerHealthUpdate::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteFloat(aWriter, Percentage);
    if (!Wire::IsOfficial184())
        Vitals.Serialize(aWriter);
}

void RequestPlayerHealthUpdate::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    Percentage = Serialization::ReadFloat(aReader);
    if (!Wire::IsOfficial184())
        Vitals.Deserialize(aReader);
}
