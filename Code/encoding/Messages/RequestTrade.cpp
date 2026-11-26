#include <Messages/RequestTrade.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestTrade::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PartnerId);
    aWriter.WriteBits(static_cast<uint8_t>(Act), 2);
    Offer.Serialize(aWriter);
}

void RequestTrade::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    PartnerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    uint64_t action = 0;
    aReader.ReadBits(action, 2);
    Act = static_cast<Action>(action);
    Offer.Deserialize(aReader);
}
