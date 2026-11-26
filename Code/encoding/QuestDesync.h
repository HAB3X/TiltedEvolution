#pragma once

#include <algorithm>
#include <tuple>

#include <Structs/QuestLog.h>

// The party's quest desync check (RequestQuestStages, NotifyQuestDesync): which shared quests differ between the
// leader's game and a member's.
namespace QuestDesync
{
inline bool IdLess(const GameId& acLeft, const GameId& acRight) noexcept
{
    return std::tie(acLeft.ModId, acLeft.BaseId) < std::tie(acRight.ModId, acRight.BaseId);
}

// The quests at a different stage in the two logs, or in only one of them, sorted
inline Vector<GameId> Differences(const QuestLog& acLeader, const QuestLog& acMember) noexcept
{
    auto sorted = [](const QuestLog& acLog)
    {
        Vector<QuestLog::Entry> entries = acLog.Entries;
        std::sort(entries.begin(), entries.end(), [](const auto& acLeft, const auto& acRight) { return IdLess(acLeft.Id, acRight.Id); });
        return entries;
    };
    const Vector<QuestLog::Entry> cLeader = sorted(acLeader);
    const Vector<QuestLog::Entry> cMember = sorted(acMember);

    Vector<GameId> differences;
    auto leaderIt = cLeader.begin();
    auto memberIt = cMember.begin();
    while (leaderIt != cLeader.end() || memberIt != cMember.end())
    {
        if (memberIt == cMember.end() || (leaderIt != cLeader.end() && IdLess(leaderIt->Id, memberIt->Id)))
            differences.push_back((leaderIt++)->Id);
        else if (leaderIt == cLeader.end() || IdLess(memberIt->Id, leaderIt->Id))
            differences.push_back((memberIt++)->Id);
        else
        {
            if (leaderIt->Stage != memberIt->Stage)
                differences.push_back(leaderIt->Id);
            ++leaderIt;
            ++memberIt;
        }
    }
    return differences;
}

// The quests in both sorted lists: a difference seen in two reports in a row is a desync, not one game reporting just
// before the other relayed it a stage
inline Vector<GameId> InBoth(const Vector<GameId>& acPrevious, const Vector<GameId>& acCurrent) noexcept
{
    Vector<GameId> both;
    std::set_intersection(acPrevious.begin(), acPrevious.end(), acCurrent.begin(), acCurrent.end(), std::back_inserter(both), IdLess);
    return both;
}
// The leader's entries of the given quests (NotifyQuestCatchUp): the stages a member takes on catching up. The quests
// only the member has aren't there, the leader has no stage of them.
inline QuestLog LeaderStages(const QuestLog& acLeader, const Vector<GameId>& acQuests) noexcept
{
    QuestLog stages;
    for (const QuestLog::Entry& cEntry : acLeader.Entries)
    {
        if (std::find(acQuests.begin(), acQuests.end(), cEntry.Id) != acQuests.end())
            stages.Entries.push_back(cEntry);
    }
    return stages;
}
} // namespace QuestDesync
