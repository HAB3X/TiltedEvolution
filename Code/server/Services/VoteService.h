#pragma once

#include <Events/PacketEvent.h>
#include <Messages/NotifyVote.h>

#include <map>
#include <optional>

struct World;
struct UpdateEvent;
struct PlayerLeaveEvent;
struct RequestVote;
struct RequestVoteAnswer;

/**
 * @brief Votes for bigger groups: kicking a player, and ready checks.
 *
 * Any player starts one, one at a time. A kick passes when a majority of the other players say yes, within a minute;
 * it needs at least three players, never reaches the host (who plays on the server's computer), and a player who
 * started one, or a player a failed vote was about, waits before the next. A ready check passes when everyone says
 * ready within 30 seconds. Every change goes to everyone as NotifyVote; refusals only to the player who asked.
 */
struct VoteService
{
    VoteService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~VoteService() noexcept = default;

    TP_NOCOPYMOVE(VoteService);

private:
    struct Vote
    {
        uint32_t Id{};
        VoteKind Kind{};
        uint32_t StarterId{};
        uint32_t TargetId{};
        // Player id: yes
        std::map<uint32_t, bool> Answers;
        uint64_t EndsAt{};
    };

    void OnStart(const PacketEvent<RequestVote>& acPacket) noexcept;
    void OnAnswer(const PacketEvent<RequestVoteAnswer>& acPacket) noexcept;
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept;

    // aWaitUntil: the tick a cooldown ends, for its seconds left
    void Refuse(Player& aPlayer, VoteKind aKind, VoteState aState, uint64_t aWaitUntil = 0) const noexcept;
    // Sends the open vote's state, and ends it once decided (or when aTimedOut)
    void Update(bool aTimedOut = false) noexcept;
    NotifyVote Describe(VoteState aState) const noexcept;
    uint32_t CountEligible() const noexcept;

    World& m_world;
    std::optional<Vote> m_vote;
    uint32_t m_nextId{1};
    // Until when a player can't start another vote of a kind, by player id
    std::map<std::pair<uint32_t, VoteKind>, uint64_t> m_starterCooldowns;
    // Until when a player a kick vote failed against can't be voted on again, by name (ids change on reconnecting)
    std::map<String, uint64_t> m_targetCooldowns;

    entt::scoped_connection m_startConnection;
    entt::scoped_connection m_answerConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_leaveConnection;
};
