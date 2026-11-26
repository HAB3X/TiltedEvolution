#include <Messages/NotifyQuestDesync.h>

#include <WireProtocol.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyQuestDesync::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteString(aWriter, OtherPlayer);
    Serialization::WriteBool(aWriter, OtherIsLeader);
    Serialization::WriteVarInt(aWriter, Quests.size());
    for (const GameId& cId : Quests)
        cId.Serialize(aWriter);
    if (!Wire::IsOfficial184())
        LeaderStages.Serialize(aWriter);
}

void NotifyQuestDesync::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    OtherPlayer = Serialization::ReadString(aReader);
    OtherIsLeader = Serialization::ReadBool(aReader);
    Quests.resize(Serialization::ReadVarInt(aReader) & 0xFFFFFFFF);
    for (GameId& id : Quests)
        id.Deserialize(aReader);
    LeaderStages = {};
    if (!Wire::IsOfficial184())
        LeaderStages.Deserialize(aReader);
}
