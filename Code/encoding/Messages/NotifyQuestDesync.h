#pragma once

#include "Message.h"
#include <Structs/QuestLog.h>
#include <Structs/GameId.h>

// A party member's shared quests differ from the leader's (see RequestQuestStages). Sent to both of them, and only
// when the quests that differ change.
struct NotifyQuestDesync final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyQuestDesync;

    NotifyQuestDesync()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyQuestDesync& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && OtherPlayer == acRhs.OtherPlayer && OtherIsLeader == acRhs.OtherIsLeader && Quests == acRhs.Quests && LeaderStages == acRhs.LeaderStages;
    }

    // Whose quests differ from the receiver's: the leader's for a member, a member's for the leader
    TiltedPhoques::String OtherPlayer;
    bool OtherIsLeader{};
    // The quests at a different stage, or running or completed for only one of the two
    TiltedPhoques::Vector<GameId> Quests;
    // To the member: the leader's stages of those quests, which the member can repair theirs with. Plus only on the wire
    QuestLog LeaderStages{};
};
