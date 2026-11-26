#include <cstdint>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Stl.hpp>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <Server.hpp>

using namespace TiltedPhoques;

#include <Components.h>
#include <catch2/catch.hpp>

TEST_CASE("Remote character visibility follows player cell transitions", "[server.sync]")
{
    const GameId worldSpace{1, 1};
    const CellIdComponent firstCell{GameId{1, 2}, worldSpace, {0, 0}};
    const CellIdComponent nextCell{GameId{1, 3}, worldSpace, {10, 10}};
    const CellIdComponent exteriorActor{GameId{1, 4}, worldSpace, {0, 0}};

    REQUIRE(CellIdComponent::GetRangeTransition({}, firstCell, exteriorActor, false) == CellIdComponent::RangeTransition::Entered);
    REQUIRE(CellIdComponent::GetRangeTransition(firstCell, firstCell, exteriorActor, false) == CellIdComponent::RangeTransition::None);
    REQUIRE(CellIdComponent::GetRangeTransition(firstCell, nextCell, exteriorActor, false) == CellIdComponent::RangeTransition::Left);

    const CellIdComponent interiorCell{GameId{1, 5}, {}, {}};
    const CellIdComponent interiorActor{GameId{1, 5}};
    REQUIRE(CellIdComponent::GetRangeTransition(firstCell, interiorCell, interiorActor, false) == CellIdComponent::RangeTransition::Entered);
    REQUIRE(CellIdComponent::GetRangeTransition(interiorCell, firstCell, interiorActor, false) == CellIdComponent::RangeTransition::Left);

    const CellIdComponent dragonActor{GameId{1, 7}, worldSpace, {8, 8}};
    REQUIRE(CellIdComponent::GetRangeTransition({}, firstCell, dragonActor, true) == CellIdComponent::RangeTransition::Entered);
    REQUIRE(CellIdComponent::GetRangeTransition({}, firstCell, dragonActor, false) == CellIdComponent::RangeTransition::None);
}

TEST_CASE("NPC movement is relayed less often the further away the player is", "[server.sync]")
{
    using Band = MovementRelayComponent::Band;
    constexpr uint32_t cNearPlayer = 1, cMidPlayer = 2, cFarPlayer = 3;

    MovementRelayComponent relay;
    uint32_t near = 0, mid = 0, far = 0;
    // Six seconds of snapshots every 30 ms, as an owner sends an NPC in combat
    for (uint64_t tick = 1000; tick < 7000; tick += 30)
    {
        near += relay.ShouldRelay(cNearPlayer, 500.f, tick);
        mid += relay.ShouldRelay(cMidPlayer, 4000.f, tick);
        far += relay.ShouldRelay(cFarPlayer, 9000.f, tick);
    }

    REQUIRE(near == 200);
    REQUIRE(mid == 67); // every third
    REQUIRE(far == 34); // every sixth

    SECTION("bands change only past their edge by the hysteresis")
    {
        REQUIRE(MovementRelayComponent::NextBand(Band::Near, 2048.f + 200.f) == Band::Near);
        REQUIRE(MovementRelayComponent::NextBand(Band::Near, 2048.f + 300.f) == Band::Mid);
        REQUIRE(MovementRelayComponent::NextBand(Band::Mid, 2048.f - 200.f) == Band::Mid);
        REQUIRE(MovementRelayComponent::NextBand(Band::Mid, 2048.f - 300.f) == Band::Near);
        REQUIRE(MovementRelayComponent::NextBand(Band::Near, 20000.f) == Band::Far);
        REQUIRE(MovementRelayComponent::NextBand(Band::Far, 10.f) == Band::Near);
    }

    SECTION("an NPC pacing on a band's edge keeps one rate")
    {
        MovementRelayComponent pacing;
        uint32_t relayed = 0;
        for (uint64_t tick = 1000; tick < 7000; tick += 30)
            relayed += pacing.ShouldRelay(cNearPlayer, tick % 60 ? 2000.f : 2200.f, tick);
        REQUIRE(relayed == 200);
    }

    SECTION("players who stopped receiving are forgotten")
    {
        REQUIRE(relay.Receivers.size() == 3);
        REQUIRE(relay.ShouldRelay(4, 500.f, 7000 + MovementRelayComponent::kForgetAfter + 1000));
        REQUIRE(relay.Receivers.size() == 1);
    }
}

TEST_CASE("Of two players taking the same items from a container at once, the later gives back the other's", "[server.sync]")
{
    InventoryComponent chest;
    Inventory::Entry sword{};
    sword.BaseId = GameId(0, 0x12EB7);

    constexpr uint32_t cFirst = 1, cSecond = 2;
    // The chest had one sword: the first gets it, the second's take finds it gone and gives it back
    REQUIRE(chest.RefusedTake(cFirst, sword, 1, 1, 1000) == 0);
    REQUIRE(chest.RefusedTake(cSecond, sword, 1, 0, 1100) == 1);

    // A shortfall no other player explains (the server didn't know the item) is kept, as before
    REQUIRE(chest.RefusedTake(cFirst, sword, 1, 0, 1200) == 0);

    // Long after the other's take it is no race either
    REQUIRE(chest.RefusedTake(cSecond, sword, 1, 0, 1000 + InventoryComponent::kTakeRaceWindow + 500) == 0);

    // Other items don't count
    Inventory::Entry shield{};
    shield.BaseId = GameId(0, 0x12EB6);
    REQUIRE(chest.RefusedTake(cFirst, sword, 1, 1, 10000) == 0);
    REQUIRE(chest.RefusedTake(cSecond, shield, 1, 0, 10100) == 0);
}

TEST_CASE("With per player loot, each player takes from their own copy of a container", "[server.sync]")
{
    InventoryComponent chest;
    Inventory::Entry sword{};
    sword.BaseId = GameId(0, 0x12EB7);
    sword.Count = 1;
    chest.Content.Entries.push_back(sword);

    Inventory::Entry take = sword;
    take.Count = -1;
    REQUIRE(chest.ChangePlayerCopy("Lydia", take) == -1);
    // Lydia's copy is empty, the chest as first seen still has the sword for everyone else
    REQUIRE(chest.ContentFor("Lydia").Entries.empty());
    REQUIRE(chest.ContentFor("Serana").Entries.size() == 1);
    REQUIRE(chest.Content.Entries.size() == 1);

    // Taking it again from her own copy finds nothing
    REQUIRE(chest.ChangePlayerCopy("Lydia", take) == 0);
    // Serana takes hers
    REQUIRE(chest.ChangePlayerCopy("Serana", take) == -1);
    REQUIRE(chest.ContentFor("Serana").Entries.empty());
}

TEST_CASE("Per player loot on bodies", "[server.loot]")
{
    InventoryComponent body;
    Inventory::Entry sword{};
    sword.BaseId = GameId(0, 0x12EB7);
    sword.Count = 1;
    body.Content.Entries.push_back(sword);
    body.DeathTick = 1000;

    Inventory::Entry salts = sword;
    salts.BaseId = GameId(0, 0x3AD5E);
    Inventory::Entry take = sword;
    take.Count = -1;

    GIVEN("A player looting it takes from their own copy only")
    {
        const auto cChange = body.ChangeBody("Lydia", false, take, 1500);
        REQUIRE(cChange.Applied == -1);
        REQUIRE_FALSE(cChange.Shared);
        REQUIRE(body.ContentFor("Lydia").Entries.empty());
        REQUIRE(body.ContentFor("Serana").Entries.size() == 1);
        // The owner looting is no different
        REQUIRE_FALSE(body.ChangeBody("Serana", true, take, 9000).Shared);
        REQUIRE(body.ContentFor("Serana").Entries.empty());
        REQUIRE(body.Content.Entries.size() == 1);
    }

    GIVEN("The owner's game adding death items right after the death gives them to everyone, copies too")
    {
        body.ChangeBody("Lydia", false, take, 1200);
        const auto cChange = body.ChangeBody("Serana", true, salts, 1000 + InventoryComponent::kDeathItemsWindow);
        REQUIRE(cChange.Shared);
        REQUIRE(body.Content.Entries.size() == 2);
        REQUIRE(body.ContentFor("Lydia").Entries.size() == 1);
        REQUIRE(body.ContentFor("Lydia").Entries[0].BaseId == salts.BaseId);
    }

    GIVEN("Items put in later, or by someone else, stay in that player's copy")
    {
        REQUIRE_FALSE(body.ChangeBody("Serana", true, salts, 1001 + InventoryComponent::kDeathItemsWindow).Shared);
        REQUIRE_FALSE(body.ChangeBody("Lydia", false, salts, 1100).Shared);
        REQUIRE(body.Content.Entries.size() == 1);
        REQUIRE(body.ContentFor("Lydia").Entries.size() == 2);
    }

    GIVEN("A body never seen alive has no death items window")
    {
        body.DeathTick = 0;
        REQUIRE_FALSE(body.ChangeBody("Serana", true, salts, 10).Shared);
    }
}
