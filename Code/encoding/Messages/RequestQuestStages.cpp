#include <Messages/RequestQuestStages.h>

void RequestQuestStages::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Stages.Serialize(aWriter);
}

void RequestQuestStages::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Stages.Deserialize(aReader);
}
