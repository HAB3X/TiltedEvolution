#pragma once

#include "Message.h"
#include <Structs/QuestLog.h>

// A player joining a party, or getting a new leader, whose shared quests differ from the leader's: the leader's stages
// of those quests, to take them over or keep their own
struct NotifyQuestCatchUp final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyQuestCatchUp;

    NotifyQuestCatchUp()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyQuestCatchUp& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && LeaderName == acRhs.LeaderName && Quests == acRhs.Quests; }

    TiltedPhoques::String LeaderName{};
    // The leader's stage of each quest that differs and that the leader has running or completed
    QuestLog Quests{};
};
