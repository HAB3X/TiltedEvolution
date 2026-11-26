#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The host's mods a server requires (ModPolicy:bRequireMods): Host a game writes the host's plugins to kFileName next
// to the server's settings, one per line, and the server refuses players missing any of them or having another
// version. A line is a plugin's file name, optionally followed by kSizeSeparator and the host's file size in bytes
// ("SomeMod.esp|12345"): mods often keep their file name across versions, so the size is what tells versions apart.
namespace RequiredMods
{
constexpr char kFileName[] = "config/required-mods.txt";
// Mods the host recommends: players missing them get in and are told which (one file name per line, like kFileName)
constexpr char kRecommendedFileName[] = "config/recommended-mods.txt";
constexpr char kSizeSeparator = '|';

// One line of a mods file
struct Listed
{
    std::string Name;
    // The host's plugin size in bytes; 0 accepts any version (a plain name, as hand-written lists have)
    uint32_t FileSize = 0;

    bool operator==(const Listed&) const = default;
};

inline std::string Lower(std::string_view aText)
{
    std::string out(aText);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Bethesda's own plugins and Creation Club content: every copy of the game has its own set of them (Steam and GOG,
// SE and Anniversary Edition differ), so they're never required
inline bool IsGamePlugin(std::string_view aFileName)
{
    const std::string cName = Lower(aFileName);
    for (const char* pName : {"skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm", "dragonborn.esm", "_resourcepack.esl"})
    {
        if (cName == pName)
            return true;
    }
    return cName.rfind("cc", 0) == 0 && cName.size() > 4 && (cName.ends_with(".esl") || cName.ends_with(".esm"));
}

inline std::string FormatLine(std::string_view aName, uint32_t aFileSize)
{
    std::string line(aName);
    if (aFileSize != 0)
        line += kSizeSeparator + std::to_string(aFileSize);
    return line;
}

// Empty when the line has no name or its size isn't a number
inline std::optional<Listed> ParseLine(std::string_view aLine)
{
    if (!aLine.empty() && aLine.back() == '\r')
        aLine.remove_suffix(1);

    Listed mod;
    const size_t cSeparator = aLine.find(kSizeSeparator);
    mod.Name = aLine.substr(0, cSeparator);
    if (mod.Name.empty())
        return std::nullopt;
    if (cSeparator == std::string_view::npos)
        return mod;

    const std::string_view cSize = aLine.substr(cSeparator + 1);
    const auto [pEnd, error] = std::from_chars(cSize.data(), cSize.data() + cSize.size(), mod.FileSize);
    if (error != std::errc{} || pEnd != cSize.data() + cSize.size())
        return std::nullopt;
    return mod;
}

// What keeps a player out: the listed mods they lack, and those they have another version of
struct Mismatch
{
    std::vector<Listed> Missing;
    std::vector<Listed> DifferentVersion;

    bool Empty() const noexcept { return Missing.empty() && DifferentVersion.empty(); }
};

// Compares a player's plugins (anything with Filename and FileSize, like Mods::Entry) with the listed ones. A size of
// 0 on either side matches any version: plain-name lists don't know it, and a player whose game couldn't read a
// plugin's size (it logs that) isn't refused for something we can't tell.
template <class TPlayerMods> Mismatch Compare(const std::vector<Listed>& acListed, const TPlayerMods& acPlayerMods)
{
    Mismatch mismatch;
    for (const Listed& cListed : acListed)
    {
        const std::string cName = Lower(cListed.Name);
        const auto cIt = std::find_if(acPlayerMods.begin(), acPlayerMods.end(), [&](const auto& acMod) { return Lower(acMod.Filename.c_str()) == cName; });
        if (cIt == acPlayerMods.end())
            mismatch.Missing.push_back(cListed);
        else if (cListed.FileSize != 0 && cIt->FileSize != 0 && cIt->FileSize != cListed.FileSize)
            mismatch.DifferentVersion.push_back(cListed);
    }
    return mismatch;
}
} // namespace RequiredMods
