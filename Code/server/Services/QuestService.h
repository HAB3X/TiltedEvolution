#pragma once

#include <Events/PacketEvent.h>
#include <Structs/GameId.h>

struct World;
struct UpdateEvent;
struct RequestQuestUpdate;
struct RequestQuestStages;
struct Player;

/**
 * @brief Dispatch quest sync messages.
 *
 * This service is currently not in use.
 */
class QuestService
{
public:
    QuestService(World& aWorld, entt::dispatcher& aDispatcher);

    // The SharedQuests mask the server's settings choose
    [[nodiscard]] static uint32_t GetSharedQuestTypes() noexcept;

private:
    void OnQuestChanges(const PacketEvent<RequestQuestUpdate>& aChanges) noexcept;
    void OnQuestStages(const PacketEvent<RequestQuestStages>& acMessage) noexcept;
    // Compares the member's last report with the leader's, and tells both when the quests that differ change
    void CheckDesync(const Player& acLeader, Player& aMember) const noexcept;

    World& m_world;

    entt::scoped_connection m_questUpdateConnection;
    entt::scoped_connection m_questStagesConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_joinConnection;
};
