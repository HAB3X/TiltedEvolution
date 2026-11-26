#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Which quest categories the party shares (Gameplay:sSharedQuestTypes). A category is the game's quest type
// (TESQuest::Type on the client, sent as RequestQuestUpdate::ClientQuestType); the server only relays updates of
// shared ones, and host world only takes over shared ones. On the wire it's a mask with bit N for type N.
namespace SharedQuests
{
// TESQuest::Type values, kept here so the server needs no client header
enum Type : uint8_t
{
    kNone = 0,
    kMain = 1,
    kMagesGuild = 2,
    kThievesGuild = 3,
    kDarkBrotherhood = 4,
    kCompanions = 5,
    kMisc = 6,
    kDaedric = 7,
    kSide = 8,
    kCivilWar = 9,
    kDawnguard = 10,
    kDragonborn = 11,
    kTypeCount
};

// The names used in the server settings and by Host a game, indexed by type. None has no name: see IsShared.
constexpr std::string_view kNames[kTypeCount]{"", "Main", "MagesGuild", "ThievesGuild", "DarkBrotherhood", "Companions", "Misc",
                                              "Daedric", "Side", "CivilWar", "Dawnguard", "Dragonborn"};

constexpr uint32_t Bit(uint8_t aType) noexcept
{
    return aType < kTypeCount ? 1u << aType : 0u;
}

// Every category but Miscellaneous: what Skyrim Together always shared before the choice existed
constexpr uint32_t kDefault = ((1u << kTypeCount) - 1) & ~Bit(kNone) & ~Bit(kMisc);
constexpr char kDefaultText[] = "Main,MagesGuild,ThievesGuild,DarkBrotherhood,Companions,Daedric,Side,CivilWar,Dawnguard,Dragonborn";

// Untyped (None) quests go with Miscellaneous: the server always treated the two alike, as radiant favours and the
// game's own background quests are mostly one or the other
constexpr bool IsShared(uint32_t aMask, uint8_t aType) noexcept
{
    return (aMask & Bit(aType == kNone ? kMisc : aType)) != 0;
}

// A comma separated list of names (any case, spaces ignored) to a mask; names it doesn't know go to aUnknown
inline uint32_t Parse(std::string_view aText, std::vector<std::string>& aUnknown) noexcept
{
    uint32_t mask = 0;
    size_t start = 0;
    while (start <= aText.size())
    {
        const size_t cEnd = std::min(aText.find(',', start), aText.size());
        std::string name;
        for (const char c : aText.substr(start, cEnd - start))
        {
            if (!std::isspace(static_cast<unsigned char>(c)))
                name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        start = cEnd + 1;
        if (name.empty())
            continue;

        bool known = false;
        for (uint8_t type = kMain; type < kTypeCount; ++type)
        {
            std::string lowered(kNames[type]);
            for (char& c : lowered)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lowered == name)
            {
                mask |= Bit(type);
                known = true;
                break;
            }
        }
        if (!known)
            aUnknown.push_back(name);
    }
    return mask;
}
} // namespace SharedQuests
