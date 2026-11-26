#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/QuestLog.h>

struct QuestLogComponent
{
    QuestLog QuestContent;

    // The quest desync check (RequestQuestStages): the player's last report of their shared quests' stages
    std::optional<QuestLog> ReportedStages;
    // For a member: the leader the two lists below compare with, the quests that differed at the last comparison,
    // and those the member and leader were last told about
    uint32_t DesyncLeaderId{};
    Vector<GameId> LastDifferences;
    Vector<GameId> NotifiedDifferences;

    // Forgets the comparison with a leader, for a player leaving their party
    void ResetDesync() noexcept
    {
        ReportedStages.reset();
        DesyncLeaderId = 0;
        LastDifferences.clear();
        NotifiedDifferences.clear();
    }
};
