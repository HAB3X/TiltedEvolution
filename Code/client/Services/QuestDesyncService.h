#pragma once

struct World;
struct TransportService;
struct UpdateEvent;
struct NotifyQuestDesync;

/**
 * @brief The party's quest desync check: every minute in a party, reports the stages of the shared quests to the
 * server, which compares them with the leader's; when they differ the chat names the quests.
 */
struct QuestDesyncService
{
    QuestDesyncService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~QuestDesyncService() = default;

    TP_NOCOPYMOVE(QuestDesyncService);

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnQuestDesync(const NotifyQuestDesync& acMessage) const noexcept;

    World& m_world;
    TransportService& m_transport;
    std::chrono::steady_clock::time_point m_nextReport{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_questDesyncConnection;
};
