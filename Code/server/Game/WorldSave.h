#pragma once

// Encoding headers expect TiltedCore's containers and buffers already included
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Stl.hpp>

#include <Structs/GameId.h>
#include <Structs/Inventory.h>
#include <Structs/ObjectData.h>

#include <optional>
#include <string>

/**
 * @brief What a host world server keeps of the shared world between runs (WorldSaveService), as bytes for a file.
 *
 * Layout, numbers as TiltedCore varints and the structures in this build's network encoding:
 *   "STWORLD" and a version byte (1)
 *   the plugin ids: count, then per plugin its id, lite flag and file name. Every GameId below uses these ids, so
 *     they are registered again, in the same order, before anyone joins
 *   NPCs killed while nobody had them loaded: count, then per NPC its GameId and its Inventory (their loot)
 *   containers: count, then per container its ObjectData (id, cell, lock, contents) and the names of the players
 *     whose own storage was merged into it
 *   "END" as a check that nothing was cut off
 */
struct WorldSave
{
    struct Plugin
    {
        String Name;
        uint32_t Id{};
        bool IsLite{};

        bool operator==(const Plugin&) const noexcept = default;
    };

    struct DeadActor
    {
        GameId Id{};
        Inventory Loot{};

        bool operator==(const DeadActor&) const noexcept = default;
    };

    struct Container
    {
        ObjectData Object{};
        Vector<String> MergedPlayers{};

        bool operator==(const Container&) const noexcept = default;
    };

    Vector<Plugin> Plugins;
    Vector<DeadActor> DeadActors;
    Vector<Container> Containers;

    bool operator==(const WorldSave&) const noexcept = default;

    [[nodiscard]] std::string Serialize() const noexcept;
    // Empty when the bytes aren't a whole world save of this version
    [[nodiscard]] static std::optional<WorldSave> Parse(const std::string& acBytes) noexcept;
};
