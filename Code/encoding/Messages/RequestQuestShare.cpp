#include <Messages/RequestQuestShare.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestQuestShare::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Id.Serialize(aWriter);
    Serialization::WriteVarInt(aWriter, Stage);
    Serialization::WriteVarInt(aWriter, ClientQuestType);
}

void RequestQuestShare::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Id.Deserialize(aReader);
    Stage = Serialization::ReadVarInt(aReader) & 0xFFFF;
    ClientQuestType = Serialization::ReadVarInt(aReader) & 0xFF;
}
