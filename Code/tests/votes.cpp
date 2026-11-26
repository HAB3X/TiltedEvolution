#include <catch2/catch.hpp>

#include <Game/VoteTally.h>

using VoteTally::Result;

TEST_CASE("Vote tallies", "[server.votes]")
{
    GIVEN("A kick vote among 4 other players")
    {
        REQUIRE(VoteTally::Kick(4, 1, 0) == Result::Open);
        REQUIRE(VoteTally::Kick(4, 2, 0) == Result::Open);
        REQUIRE(VoteTally::Kick(4, 3, 0) == Result::Passed);
        REQUIRE(VoteTally::Kick(4, 2, 1) == Result::Open);
        // Two no out of four: three yes can't happen any more
        REQUIRE(VoteTally::Kick(4, 2, 2) == Result::Failed);
    }

    GIVEN("A kick vote among 3 other players")
    {
        REQUIRE(VoteTally::Kick(3, 2, 0) == Result::Passed);
        REQUIRE(VoteTally::Kick(3, 1, 1) == Result::Open);
        REQUIRE(VoteTally::Kick(3, 1, 2) == Result::Failed);
    }

    GIVEN("A ready check")
    {
        REQUIRE(VoteTally::Ready(3, 2, 0) == Result::Open);
        REQUIRE(VoteTally::Ready(3, 3, 0) == Result::Passed);
        REQUIRE(VoteTally::Ready(3, 2, 1) == Result::Failed);
    }
}
