#pragma once

#include <Events/PacketEvent.h>
#include <Structs/GameId.h>
#include <Services/PartyService.h>

struct World;
struct UpdateEvent;
struct RequestQuestUpdate;
struct RequestQuestStages;
struct RequestPartyQuestSharing;
struct RequestQuestShare;
struct RequestQuestShareAnswer;
struct Player;

/**
 * @brief Dispatch quest sync messages.
 *
 * Quest updates are relayed to the party when the party shares the quest's kind (its leader chooses, the host's
 * Gameplay:sSharedQuestTypes by default), or among the members who took a quest one of them offered.
 */
class QuestService
{
public:
    QuestService(World& aWorld, entt::dispatcher& aDispatcher);

    // The SharedQuests mask the server's settings choose
    [[nodiscard]] static uint32_t GetSharedQuestTypes() noexcept;
    // The quest kinds the party shares now, a SharedQuests mask
    [[nodiscard]] static uint32_t GetSharedQuestTypes(const PartyService::Party& acParty) noexcept;
    // Tells the player the party's quest sharing, on joining it
    static void SendQuestSharing(const PartyService::Party& acParty, Player& aPlayer) noexcept;

private:
    void OnQuestChanges(const PacketEvent<RequestQuestUpdate>& aChanges) noexcept;
    void OnQuestStages(const PacketEvent<RequestQuestStages>& acMessage) noexcept;
    void OnPartyQuestSharing(const PacketEvent<RequestPartyQuestSharing>& acMessage) noexcept;
    void OnQuestShare(const PacketEvent<RequestQuestShare>& acMessage) noexcept;
    void OnQuestShareAnswer(const PacketEvent<RequestQuestShareAnswer>& acMessage) noexcept;
    // Compares the member's last report with the leader's, and tells both when the quests that differ change
    void CheckDesync(const Player& acLeader, Player& aMember) const noexcept;

    World& m_world;

    entt::scoped_connection m_questUpdateConnection;
    entt::scoped_connection m_questStagesConnection;
    entt::scoped_connection m_partyQuestSharingConnection;
    entt::scoped_connection m_questShareConnection;
    entt::scoped_connection m_questShareAnswerConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_joinConnection;
};
