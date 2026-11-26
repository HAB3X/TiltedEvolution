#include <Messages/NotifyTrade.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyTrade::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PartnerId);
    Serialization::WriteString(aWriter, PartnerName);
    aWriter.WriteBits(static_cast<uint8_t>(What), 3);
    MyOffer.Serialize(aWriter);
    TheirOffer.Serialize(aWriter);
    Serialization::WriteBool(aWriter, MyConfirmed);
    Serialization::WriteBool(aWriter, TheirConfirmed);
}

void NotifyTrade::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    PartnerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    PartnerName = Serialization::ReadString(aReader);
    uint64_t what = 0;
    aReader.ReadBits(what, 3);
    What = static_cast<Event>(what);
    MyOffer.Deserialize(aReader);
    TheirOffer.Deserialize(aReader);
    MyConfirmed = Serialization::ReadBool(aReader);
    TheirConfirmed = Serialization::ReadBool(aReader);
}
