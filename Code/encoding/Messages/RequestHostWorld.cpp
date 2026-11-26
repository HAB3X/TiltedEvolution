#include <Messages/RequestHostWorld.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestHostWorld::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
}

void RequestHostWorld::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
}
