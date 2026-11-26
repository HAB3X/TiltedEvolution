#pragma once

#include "Message.h"

// What a vote decides: kicking a player (a majority of the others), or whether everyone is ready (all of them)
enum class VoteKind : uint8_t
{
    Kick,
    Ready
};

enum class VoteState : uint8_t
{
    Open,
    Passed,
    Failed,
    // The player to kick left
    Cancelled,
    // Not started, said to the player who asked only: another vote is open, their cooldown or the target's isn't over,
    // too few players for a kick, the target is the host, or there's no such player
    RefusedBusy,
    RefusedCooldown,
    RefusedTooFew,
    RefusedHost,
    RefusedTarget,
};

// A vote's state, to every player whenever it changes
struct NotifyVote final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyVote;

    NotifyVote()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyVote& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && VoteId == acRhs.VoteId && Kind == acRhs.Kind && State == acRhs.State && StarterId == acRhs.StarterId &&
               TargetId == acRhs.TargetId && Yes == acRhs.Yes && No == acRhs.No && Eligible == acRhs.Eligible && SecondsLeft == acRhs.SecondsLeft;
    }

    uint32_t VoteId{};
    VoteKind Kind{};
    VoteState State{};
    uint32_t StarterId{};
    // Kick: the player to kick
    uint32_t TargetId{};
    // Answers so far, and how many may answer (everyone but the player to kick)
    uint32_t Yes{};
    uint32_t No{};
    uint32_t Eligible{};
    // While open; for a refused cooldown, how long it still runs
    uint32_t SecondsLeft{};
};
