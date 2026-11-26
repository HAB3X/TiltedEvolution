#include <Messages/NotifyQuestCatchUp.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyQuestCatchUp::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteString(aWriter, LeaderName);
    Quests.Serialize(aWriter);
}

void NotifyQuestCatchUp::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    LeaderName = Serialization::ReadString(aReader);
    Quests.Deserialize(aReader);
}
