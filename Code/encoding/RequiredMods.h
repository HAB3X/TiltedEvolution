#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

// The host's mods a server requires (ModPolicy:bRequireMods): Host a game writes the host's plugins to kFileName next
// to the server's settings, one file name per line, and the server refuses players missing any of them.
namespace RequiredMods
{
constexpr char kFileName[] = "config/required-mods.txt";

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
} // namespace RequiredMods
