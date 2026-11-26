#include <Messages/NotifyHostWorldState.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyHostWorldState::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    State.Serialize(aWriter);
}

void NotifyHostWorldState::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    State.Deserialize(aReader);
}
