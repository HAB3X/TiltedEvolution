#pragma once

struct World;
struct TransportService;
struct UpdateEvent;
struct NotifyQuestDesync;
struct NotifyQuestCatchUp;
struct NotifyPartyQuestSharing;

#include <Structs/QuestLog.h>

/**
 * @brief The party's quest desync check: every minute in a party, reports the stages of the shared quests to the
 * server, which compares them with the leader's; when they differ the chat names the quests.
 *
 * Catching up: on the first comparison with a leader (on joining a party, or when it gets a new leader) the server
 * sends the leader's stages of the quests that differ (NotifyQuestCatchUp). The player takes the leader's progress or
 * keeps their own. Quests completed here are kept, and stages already passed here aren't undone.
 *
 * Repairing: a member whose quests keep differing from the leader's is sent the leader's stages of those quests with
 * the desync notice, and can take them the same way (Repair).
 */
struct QuestDesyncService
{
    QuestDesyncService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~QuestDesyncService() = default;

    TP_NOCOPYMOVE(QuestDesyncService);

    // The answer to the overlay's catch-up prompt: take the leader's stages or keep this game's
    void AnswerCatchUp(bool aTake) noexcept;
    // The overlay's Repair on a desync notice: takes the leader's stages of the quests that differ
    void Repair() noexcept;

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnQuestDesync(const NotifyQuestDesync& acMessage) noexcept;
    void OnQuestCatchUp(const NotifyQuestCatchUp& acMessage) noexcept;
    void OnQuestSharing(const NotifyPartyQuestSharing&) noexcept;

    World& m_world;
    TransportService& m_transport;
    std::chrono::steady_clock::time_point m_nextReport{};
    // The leader's stages the catch-up prompt offers, empty when nothing is offered
    QuestLog m_catchUp{};
    // The leader's stages the last desync notice offers to repair with, empty when there's nothing to repair
    QuestLog m_repair{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_questDesyncConnection;
    entt::scoped_connection m_questCatchUpConnection;
    entt::scoped_connection m_questSharingConnection;
};
