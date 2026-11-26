#pragma once

#include <cstddef>

// How a vote ends, from the answers so far (VoteService): pure, so it's unit tested
namespace VoteTally
{
enum class Result
{
    Open,
    Passed,
    Failed
};

// Kick: a majority of the eligible voters (everyone but the player to kick) says yes; it fails once that can't happen
inline Result Kick(size_t aEligible, size_t aYes, size_t aNo) noexcept
{
    if (aYes * 2 > aEligible)
        return Result::Passed;
    if (aNo * 2 >= aEligible)
        return Result::Failed;
    return Result::Open;
}

// Ready check: everyone says yes; one no fails it
inline Result Ready(size_t aEligible, size_t aYes, size_t aNo) noexcept
{
    if (aNo > 0)
        return Result::Failed;
    return aYes >= aEligible ? Result::Passed : Result::Open;
}
} // namespace VoteTally
