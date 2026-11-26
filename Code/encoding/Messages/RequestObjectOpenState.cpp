#include <Messages/RequestObjectOpenState.h>

#include <TiltedCore/Serialization.hpp>

void RequestObjectOpenState::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Id.Serialize(aWriter);
    CellId.Serialize(aWriter);
    aWriter.WriteBits(State, 3);
}

void RequestObjectOpenState::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Id.Deserialize(aReader);
    CellId.Deserialize(aReader);
    uint64_t state = 0;
    aReader.ReadBits(state, 3);
    State = static_cast<uint8_t>(state);
}
