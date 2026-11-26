#include <Structs/Mods.h>
#include <TiltedCore/Serialization.hpp>
#include <WireProtocol.h>
#include <algorithm>

using TiltedPhoques::Serialization;

bool Mods::operator==(const Mods& acRhs) const noexcept
{
    return ModList == acRhs.ModList;
}

bool Mods::operator!=(const Mods& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void Mods::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    const uint16_t modCount = std::min(ModList.size(), size_t(4096)) & 0xFFFF;
    aWriter.WriteBits(modCount, 13);

    for (auto& entry : ModList)
    {
        aWriter.WriteBits(entry.Id, 16);
        Serialization::WriteBool(aWriter, entry.IsLite);
        Serialization::WriteString(aWriter, entry.Filename);
        // Official v1.8.4 doesn't know mod versions
        if (!Wire::IsOfficial184())
            Serialization::WriteVarInt(aWriter, entry.FileSize);
    }
}

void Mods::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    uint64_t data = 0;
    aReader.ReadBits(data, 13);

    const size_t modCount = data & 0xFFFF;
    ModList.resize(modCount);
    for (size_t i = 0; i < modCount; ++i)
    {
        aReader.ReadBits(data, 16);
        ModList[i].Id = data & 0xFFFF;
        ModList[i].IsLite = Serialization::ReadBool(aReader);
        ModList[i].Filename = Serialization::ReadString(aReader);
        ModList[i].FileSize = Wire::IsOfficial184() ? 0 : Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    }
}
