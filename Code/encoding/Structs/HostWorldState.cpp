#include <Structs/HostWorldState.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

namespace
{
void SerializeIds(TiltedPhoques::Buffer::Writer& aWriter, const Vector<GameId>& acIds) noexcept
{
    Serialization::WriteVarInt(aWriter, acIds.size());
    for (const GameId& id : acIds)
        id.Serialize(aWriter);
}

void DeserializeIds(TiltedPhoques::Buffer::Reader& aReader, Vector<GameId>& aIds) noexcept
{
    aIds.resize(Serialization::ReadVarInt(aReader) & 0xFFFFFFFF);
    for (GameId& id : aIds)
        id.Deserialize(aReader);
}
} // namespace

bool HostWorldState::operator==(const HostWorldState& acRhs) const noexcept
{
    return RunningQuests == acRhs.RunningQuests && CompletedQuests == acRhs.CompletedQuests && ClearedLocations == acRhs.ClearedLocations &&
           DeadUniqueActors == acRhs.DeadUniqueActors;
}

void HostWorldState::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    RunningQuests.Serialize(aWriter);
    CompletedQuests.Serialize(aWriter);
    SerializeIds(aWriter, ClearedLocations);
    SerializeIds(aWriter, DeadUniqueActors);
}

void HostWorldState::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    RunningQuests.Deserialize(aReader);
    CompletedQuests.Deserialize(aReader);
    DeserializeIds(aReader, ClearedLocations);
    DeserializeIds(aReader, DeadUniqueActors);
}
