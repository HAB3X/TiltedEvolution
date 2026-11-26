#include <Services/VoteService.h>

#include <GameServer.h>
#include <World.h>
#include <Game/VoteTally.h>

#include <Events/PlayerLeaveEvent.h>
#include <Events/UpdateEvent.h>
#include <Messages/RequestVote.h>
#include <Messages/RequestVoteAnswer.h>

namespace
{
constexpr uint64_t kKickVoteMs = 60'000;
constexpr uint64_t kReadyCheckMs = 30'000;
// Before the same player starts another vote of the same kind
constexpr uint64_t kKickCooldownMs = 120'000;
constexpr uint64_t kReadyCooldownMs = 30'000;
// Before a player a kick vote failed against can be voted on again
constexpr uint64_t kTargetCooldownMs = 300'000;
// The player to kick, the one who starts it and at least one more to agree
constexpr size_t kKickMinPlayers = 3;

uint32_t SecondsUntil(uint64_t aTick) noexcept
{
    const uint64_t cNow = GameServer::Get()->GetTick();
    return aTick > cNow ? static_cast<uint32_t>((aTick - cNow + 999) / 1000) : 0;
}
} // namespace

VoteService::VoteService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_startConnection(aDispatcher.sink<PacketEvent<RequestVote>>().connect<&VoteService::OnStart>(this))
    , m_answerConnection(aDispatcher.sink<PacketEvent<RequestVoteAnswer>>().connect<&VoteService::OnAnswer>(this))
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&VoteService::OnUpdate>(this))
    , m_leaveConnection(aDispatcher.sink<PlayerLeaveEvent>().connect<&VoteService::OnPlayerLeave>(this))
{
}

void VoteService::Refuse(Player& aPlayer, VoteKind aKind, VoteState aState, uint64_t aWaitUntil) const noexcept
{
    NotifyVote refusal;
    refusal.Kind = aKind;
    refusal.State = aState;
    refusal.StarterId = aPlayer.GetId();
    refusal.SecondsLeft = aWaitUntil ? SecondsUntil(aWaitUntil) : 0;
    aPlayer.Send(refusal);
}

void VoteService::OnStart(const PacketEvent<RequestVote>& acPacket) noexcept
{
    Player& starter = *acPacket.pPlayer;
    const RequestVote& cRequest = acPacket.Packet;
    if (cRequest.Kind != VoteKind::Kick && cRequest.Kind != VoteKind::Ready)
        return;

    const uint64_t cNow = GameServer::Get()->GetTick();
    if (m_vote)
        return Refuse(starter, cRequest.Kind, VoteState::RefusedBusy);
    if (const auto cIt = m_starterCooldowns.find({starter.GetId(), cRequest.Kind}); cIt != m_starterCooldowns.end() && cIt->second > cNow)
        return Refuse(starter, cRequest.Kind, VoteState::RefusedCooldown, cIt->second);

    Vote vote;
    vote.Kind = cRequest.Kind;
    vote.StarterId = starter.GetId();
    if (cRequest.Kind == VoteKind::Kick)
    {
        Player* pTarget = m_world.GetPlayerManager().GetById(cRequest.TargetId);
        if (!pTarget || pTarget == &starter)
            return Refuse(starter, cRequest.Kind, VoteState::RefusedTarget);
        if (pTarget->IsOnServerComputer())
            return Refuse(starter, cRequest.Kind, VoteState::RefusedHost);
        if (m_world.GetPlayerManager().Count() < kKickMinPlayers)
            return Refuse(starter, cRequest.Kind, VoteState::RefusedTooFew);
        if (const auto cIt = m_targetCooldowns.find(pTarget->GetUsername()); cIt != m_targetCooldowns.end() && cIt->second > cNow)
            return Refuse(starter, cRequest.Kind, VoteState::RefusedCooldown, cIt->second);
        vote.TargetId = pTarget->GetId();
    }

    vote.Id = m_nextId++;
    vote.EndsAt = cNow + (vote.Kind == VoteKind::Kick ? kKickVoteMs : kReadyCheckMs);
    // Starting one is saying yes
    vote.Answers[starter.GetId()] = true;
    m_starterCooldowns[{starter.GetId(), vote.Kind}] = cNow + (vote.Kind == VoteKind::Kick ? kKickCooldownMs : kReadyCooldownMs);
    m_vote = std::move(vote);

    if (m_vote->Kind == VoteKind::Kick)
        spdlog::info("[Host] {} started a vote to kick {}", starter.GetUsername().c_str(),
                     m_world.GetPlayerManager().GetById(m_vote->TargetId)->GetUsername().c_str());
    Update();
}

void VoteService::OnAnswer(const PacketEvent<RequestVoteAnswer>& acPacket) noexcept
{
    const uint32_t cVoter = acPacket.pPlayer->GetId();
    // The player to kick has no say
    if (!m_vote || acPacket.Packet.VoteId != m_vote->Id || (m_vote->Kind == VoteKind::Kick && cVoter == m_vote->TargetId))
        return;
    m_vote->Answers[cVoter] = acPacket.Packet.Yes;
    Update();
}

void VoteService::OnUpdate(const UpdateEvent&) noexcept
{
    if (m_vote && GameServer::Get()->GetTick() >= m_vote->EndsAt)
        Update(true);
}

void VoteService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    if (!m_vote)
        return;
    const uint32_t cLeaver = acEvent.pPlayer->GetId();
    if (m_vote->Kind == VoteKind::Kick && cLeaver == m_vote->TargetId)
    {
        const NotifyVote cCancelled = Describe(VoteState::Cancelled);
        m_vote.reset();
        GameServer::Get()->SendToPlayers(cCancelled, acEvent.pPlayer);
        return;
    }
    m_vote->Answers.erase(cLeaver);
    Update();
}

uint32_t VoteService::CountEligible() const noexcept
{
    uint32_t eligible = 0;
    for (const Player* pPlayer : m_world.GetPlayerManager())
    {
        if (!(m_vote->Kind == VoteKind::Kick && pPlayer->GetId() == m_vote->TargetId))
            ++eligible;
    }
    return eligible;
}

NotifyVote VoteService::Describe(VoteState aState) const noexcept
{
    NotifyVote notify;
    notify.VoteId = m_vote->Id;
    notify.Kind = m_vote->Kind;
    notify.State = aState;
    notify.StarterId = m_vote->StarterId;
    notify.TargetId = m_vote->TargetId;
    for (const auto& [cVoter, cYes] : m_vote->Answers)
        ++(cYes ? notify.Yes : notify.No);
    notify.Eligible = CountEligible();
    notify.SecondsLeft = aState == VoteState::Open ? SecondsUntil(m_vote->EndsAt) : 0;
    return notify;
}

void VoteService::Update(bool aTimedOut) noexcept
{
    NotifyVote notify = Describe(VoteState::Open);
    const auto cResult = m_vote->Kind == VoteKind::Kick ? VoteTally::Kick(notify.Eligible, notify.Yes, notify.No) : VoteTally::Ready(notify.Eligible, notify.Yes, notify.No);
    if (cResult == VoteTally::Result::Open && !aTimedOut)
    {
        GameServer::Get()->SendToPlayers(notify);
        return;
    }

    notify.State = cResult == VoteTally::Result::Passed ? VoteState::Passed : VoteState::Failed;
    notify.SecondsLeft = 0;
    const Vote cVote = std::move(*m_vote);
    m_vote.reset();
    GameServer::Get()->SendToPlayers(notify);

    if (cVote.Kind != VoteKind::Kick)
        return;
    Player* pTarget = m_world.GetPlayerManager().GetById(cVote.TargetId);
    if (!pTarget)
        return;
    if (notify.State == VoteState::Failed)
    {
        m_targetCooldowns[pTarget->GetUsername()] = GameServer::Get()->GetTick() + kTargetCooldownMs;
        spdlog::info("[Host] The vote to kick {} failed ({} yes, {} no of {})", pTarget->GetUsername().c_str(), notify.Yes, notify.No, notify.Eligible);
        return;
    }
    spdlog::info("[Host] The others voted to kick {} ({} yes of {})", pTarget->GetUsername().c_str(), notify.Yes, notify.Eligible);
    GameServer::Get()->Kick(pTarget->GetConnectionId());
}
