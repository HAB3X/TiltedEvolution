#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <glm/glm.hpp>

using namespace TiltedPhoques;

#include <catch2/catch.hpp>

#include "../client/Systems/MovementSendFilter.h"

TEST_CASE("Snapshots of an actor standing still stop after two, but for a keep-alive", "[client.movement]")
{
    MovementSendFilter filter;
    Movement standing;
    standing.Position = glm::vec3(100.f, 200.f, 0.f);

    uint64_t tick = 1000;
    REQUIRE(filter.ShouldSend(standing, tick));
    REQUIRE(filter.ShouldSend(standing, tick += 90));
    REQUIRE_FALSE(filter.ShouldSend(standing, tick += 90));

    // Ten seconds standing still at the out-of-combat 90 ms: nine snapshots (the first 90 ms step past each second) instead of 111
    uint32_t sent = 0;
    for (const uint64_t cEnd = tick + 10000; tick < cEnd;)
        sent += filter.ShouldSend(standing, tick += 90);
    REQUIRE(sent == 9);

    SECTION("moving again is sent at once, every snapshot")
    {
        Movement walking = standing;
        for (int i = 0; i < 5; ++i)
        {
            walking.Position.x += 10.f;
            REQUIRE(filter.ShouldSend(walking, tick += 90));
        }

        // and stopping is sent twice before the snapshots stop again
        REQUIRE(filter.ShouldSend(walking, tick += 90));
        REQUIRE_FALSE(filter.ShouldSend(walking, tick += 90));
    }

    SECTION("an animation variable changing counts as moving")
    {
        Movement breathing = standing;
        breathing.Variables.Floats.push_back(0.5f);
        REQUIRE(filter.ShouldSend(breathing, tick += 90));
    }
}
