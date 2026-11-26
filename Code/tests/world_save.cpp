#include <catch2/catch.hpp>

#include <Game/WorldSave.h>

TEST_CASE("World saves", "[server.world_save]")
{
    WorldSave save;
    save.Plugins.push_back({"Skyrim.esm", 0, false});
    save.Plugins.push_back({"SomeMod.esl", 1, true});

    Inventory loot;
    Inventory::Entry gold;
    gold.BaseId = GameId(0, 0xF);
    gold.Count = 250;
    loot.Entries.push_back(gold);
    save.DeadActors.push_back({GameId(0, 0x1A69C), loot});

    WorldSave::Container chest;
    chest.Object.Id = GameId(1, 0x812);
    chest.Object.CellId = GameId(0, 0x3C);
    chest.Object.IsContainer = true;
    chest.Object.CurrentInventory = loot;
    chest.MergedPlayers.push_back("Lydia");
    chest.PlayerCopies.push_back({"Serana", Inventory{}});
    save.Containers.push_back(chest);

    const std::string cBytes = save.Serialize();
    const auto cLoaded = WorldSave::Parse(cBytes);
    REQUIRE(cLoaded.has_value());
    REQUIRE(*cLoaded == save);

    // Cut off, or another file
    REQUIRE_FALSE(WorldSave::Parse(cBytes.substr(0, cBytes.size() - 2)).has_value());
    REQUIRE_FALSE(WorldSave::Parse("STWORLD\x02").has_value());
    REQUIRE_FALSE(WorldSave::Parse("not a world").has_value());
    REQUIRE(WorldSave::Parse(WorldSave{}.Serialize()) == WorldSave{});
}
