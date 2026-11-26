#pragma once

#include <Structs/GameId.h>
#include <Structs/QuestLog.h>

using TiltedPhoques::Vector;

// What a joiner takes over from the host's game in host world mode (Gameplay:bHostWorld)
struct HostWorldState
{
    bool operator==(const HostWorldState& acRhs) const noexcept;
    bool operator!=(const HostWorldState& acRhs) const noexcept { return !this->operator==(acRhs); }

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    // Each running quest at the host's current stage
    QuestLog RunningQuests{};
    // Each completed quest at the stage it completed on
    QuestLog CompletedQuests{};
    // Locations the host has cleared
    Vector<GameId> ClearedLocations{};
    // Unique NPCs (each location's unique actor references) that are dead in the host's game
    Vector<GameId> DeadUniqueActors{};
};
