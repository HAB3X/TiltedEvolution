#include <Game/WorldSave.h>

#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>

#include <cstring>

using TiltedPhoques::Buffer;
using TiltedPhoques::Serialization;

namespace
{
constexpr char kMagic[] = "STWORLD";
constexpr uint8_t kVersion = 2;
constexpr char kEnd[] = "END";

void WriteRaw(Buffer::Writer& aWriter, const char* apText, size_t aSize)
{
    aWriter.WriteBytes(reinterpret_cast<const uint8_t*>(apText), aSize);
}

bool ReadRaw(Buffer::Reader& aReader, const char* apExpected, size_t aSize)
{
    std::string read(aSize, '\0');
    return aReader.ReadBytes(reinterpret_cast<uint8_t*>(read.data()), aSize) && std::memcmp(read.data(), apExpected, aSize) == 0;
}
} // namespace

std::string WorldSave::Serialize() const noexcept
{
    // The writer stops at the end of its buffer: grow it until the closing marker fits
    for (size_t size = 1 << 20;; size *= 2)
    {
        Buffer buffer(size);
        Buffer::Writer writer(&buffer);
        WriteRaw(writer, kMagic, sizeof(kMagic) - 1);
        writer.WriteBits(kVersion, 8);

        Serialization::WriteVarInt(writer, Plugins.size());
        for (const Plugin& cPlugin : Plugins)
        {
            Serialization::WriteVarInt(writer, cPlugin.Id);
            Serialization::WriteBool(writer, cPlugin.IsLite);
            Serialization::WriteString(writer, cPlugin.Name);
        }

        Serialization::WriteVarInt(writer, DeadActors.size());
        for (const DeadActor& cActor : DeadActors)
        {
            cActor.Id.Serialize(writer);
            cActor.Loot.Serialize(writer);
        }

        Serialization::WriteVarInt(writer, Containers.size());
        for (const Container& cContainer : Containers)
        {
            cContainer.Object.Serialize(writer);
            Serialization::WriteVarInt(writer, cContainer.MergedPlayers.size());
            for (const String& cName : cContainer.MergedPlayers)
                Serialization::WriteString(writer, cName);
        }

        if (writer.WriteBytes(reinterpret_cast<const uint8_t*>(kEnd), sizeof(kEnd)))
            return std::string(reinterpret_cast<const char*>(buffer.GetData()), writer.Size());
    }
}

std::optional<WorldSave> WorldSave::Parse(const std::string& acBytes) noexcept
{
    Buffer buffer(reinterpret_cast<const uint8_t*>(acBytes.data()), acBytes.size());
    Buffer::Reader reader(&buffer);
    uint64_t version = 0;
    if (!ReadRaw(reader, kMagic, sizeof(kMagic) - 1) || !reader.ReadBits(version, 8) || version != kVersion)
        return std::nullopt;

    // A count can't be larger than the bytes left, whatever a damaged file says
    auto readCount = [&](size_t& aCount)
    {
        aCount = Serialization::ReadVarInt(reader);
        return aCount <= acBytes.size();
    };

    WorldSave save;
    size_t count = 0;
    if (!readCount(count))
        return std::nullopt;
    for (size_t i = 0; i < count; ++i)
    {
        Plugin plugin;
        plugin.Id = Serialization::ReadVarInt(reader) & 0xFFFFFFFF;
        plugin.IsLite = Serialization::ReadBool(reader);
        plugin.Name = Serialization::ReadString(reader);
        save.Plugins.push_back(std::move(plugin));
    }

    if (!readCount(count))
        return std::nullopt;
    for (size_t i = 0; i < count; ++i)
    {
        DeadActor actor;
        actor.Id.Deserialize(reader);
        actor.Loot.Deserialize(reader);
        save.DeadActors.push_back(std::move(actor));
    }

    if (!readCount(count))
        return std::nullopt;
    for (size_t i = 0; i < count; ++i)
    {
        Container container;
        container.Object.Deserialize(reader);
        size_t names = 0;
        if (!readCount(names))
            return std::nullopt;
        for (size_t j = 0; j < names; ++j)
            container.MergedPlayers.push_back(Serialization::ReadString(reader));
        save.Containers.push_back(std::move(container));
    }

    if (!ReadRaw(reader, kEnd, sizeof(kEnd)))
        return std::nullopt;
    return save;
}
