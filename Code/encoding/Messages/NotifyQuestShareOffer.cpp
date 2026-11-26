#include <Messages/NotifyQuestShareOffer.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyQuestShareOffer::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteString(aWriter, PlayerName);
    Id.Serialize(aWriter);
}

void NotifyQuestShareOffer::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    PlayerName = Serialization::ReadString(aReader);
    Id.Deserialize(aReader);
}
