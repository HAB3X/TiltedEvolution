#include <Messages/RequestQuestShareAnswer.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestQuestShareAnswer::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Id.Serialize(aWriter);
    Serialization::WriteBool(aWriter, Accept);
}

void RequestQuestShareAnswer::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Id.Deserialize(aReader);
    Accept = Serialization::ReadBool(aReader);
}
