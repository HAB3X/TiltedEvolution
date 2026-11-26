#pragma once

#include <cstdint>

using TiltedPhoques::String;
using TiltedPhoques::Vector;

struct Mods
{
    struct Entry
    {
        String Filename;
        uint16_t Id;
        bool IsLite;
        // The plugin file's size in bytes, which tells apart versions of a mod that keep its file name; 0 when unknown
        // (official v1.8.4 doesn't send it). In a kModsMismatch refusal: the host's size when the player has another
        // version of the mod, 0 when they lack it.
        uint32_t FileSize = 0;
        // CRC-32 of the plugin file, which tells apart versions of the same size; 0 when unknown (official v1.8.4
        // doesn't send it)
        uint32_t Checksum = 0;

        bool operator==(const Entry& acRhs) const noexcept
        {
            return Filename == acRhs.Filename && Id == acRhs.Id && IsLite == acRhs.IsLite && FileSize == acRhs.FileSize && Checksum == acRhs.Checksum;
        }

        bool operator!=(const Entry& acRhs) const noexcept { return !this->operator==(acRhs); }
    };

    Vector<Entry> ModList{};

    Mods() = default;
    ~Mods() = default;

    bool operator==(const Mods& acRhs) const noexcept;
    bool operator!=(const Mods& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;
};
