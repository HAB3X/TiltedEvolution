#include <Messages/RequestRespawnAtPartner.h>

void RequestRespawnAtPartner::SerializeRaw(TiltedPhoques::Buffer::Writer&) const noexcept
{
}

void RequestRespawnAtPartner::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
}
