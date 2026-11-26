#include <Structs/PartyVitals.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void PartyVitals::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Magicka, 8);
    aWriter.WriteBits(Stamina, 8);
    Serialization::WriteBool(aWriter, InCombat);
    Serialization::WriteBool(aWriter, Downed);
}

void PartyVitals::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    uint64_t value = 0;
    aReader.ReadBits(value, 8);
    Magicka = static_cast<uint8_t>(value);
    aReader.ReadBits(value, 8);
    Stamina = static_cast<uint8_t>(value);
    InCombat = Serialization::ReadBool(aReader);
    Downed = Serialization::ReadBool(aReader);
}
