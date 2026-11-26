#include <Services/PartyTeleportService.h>

#include <GameServer.h>
#include <World.h>
#include <Events/PlayerLeaveEvent.h>
#include <Services/CommandService.h>

#include <Messages/NotifyPartySummon.h>
#include <Messages/NotifyPartyTeleportRefused.h>
#include <Messages/NotifyTeleport.h>
#include <Messages/RequestPartySummon.h>
#include <Messages/RequestPartySummonAnswer.h>
#include <Messages/RequestRespawnAtPartner.h>
#include <Messages/TeleportCommandResponse.h>
#include <Messages/TeleportRequest.h>

#include <Setting.h>

namespace
{
Console::Setting bRespawnAtPartner{"Gameplay:bRespawnAtPartner", "Whether a player respawning under the death system is moved next to a party member", true};
Console::Setting uPartyTeleportCooldown{"Gameplay:uPartyTeleportCooldown", "Seconds a player waits between going to or summoning party members", 30u};

// How long a summons waits for its answer, as long as the overlay shows it
constexpr auto kSummonsTimeout = std::chrono::seconds(30);

NotifyTeleport TeleportTo(World& aWorld, const Player& acTarget) noexcept
{
    const TeleportCommandResponse cWhere = CommandService::WhereIs(aWorld, acTarget);
    NotifyTeleport teleport{};
    teleport.CellId = cWhere.CellId;
    teleport.Position = cWhere.Position;
    teleport.WorldSpaceId = cWhere.WorldSpaceId;
    return teleport;
}

bool SameParty(const Player& acLeft, const Player& acRight) noexcept
{
    const auto& cLeft = acLeft.GetParty().JoinedPartyId;
    return cLeft.has_value() && cLeft == acRight.GetParty().JoinedPartyId;
}
} // namespace

PartyTeleportService::PartyTeleportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_teleportConnection = aDispatcher.sink<PacketEvent<TeleportRequest>>().connect<&PartyTeleportService::OnTeleport>(this);
    m_summonConnection = aDispatcher.sink<PacketEvent<RequestPartySummon>>().connect<&PartyTeleportService::OnSummon>(this);
    m_summonAnswerConnection = aDispatcher.sink<PacketEvent<RequestPartySummonAnswer>>().connect<&PartyTeleportService::OnSummonAnswer>(this);
    m_playerLeaveConnection = aDispatcher.sink<PlayerLeaveEvent>().connect<&PartyTeleportService::OnPlayerLeave>(this);
    m_respawnAtPartnerConnection = aDispatcher.sink<PacketEvent<RequestRespawnAtPartner>>().connect<&PartyTeleportService::OnRespawnAtPartner>(this);
}

void PartyTeleportService::Refuse(const Player* apTo, PartyTeleport::Refusal aReason, const Player* apOther, uint32_t aSecondsLeft) noexcept
{
    NotifyPartyTeleportRefused refused{};
    refused.Reason = aReason;
    refused.PlayerName = apOther ? apOther->GetUsername() : String{};
    refused.SecondsLeft = aSecondsLeft;
    apTo->Send(refused);
}

bool PartyTeleportService::Allowed(const Player* apAsker, const Player* apMover, const Player* apOther) const noexcept
{
    const auto cNow = std::chrono::steady_clock::now();
    const auto cLast = m_lastMoves.find(apAsker->GetId());
    const auto cSinceLast = cLast == m_lastMoves.end() ? std::chrono::steady_clock::duration::max() : cNow - cLast->second;
    const auto cCooldown = std::chrono::seconds(uPartyTeleportCooldown.value_as<uint32_t>());

    const PartyTeleport::Refusal cRefusal = PartyTeleport::Check(SameParty(*apMover, *apOther), apMover->IsInCombat(), cSinceLast, cCooldown);
    if (cRefusal == PartyTeleport::kAllowed)
        return true;

    uint32_t secondsLeft = 0;
    if (cRefusal == PartyTeleport::kCooldown)
        secondsLeft = static_cast<uint32_t>(std::chrono::ceil<std::chrono::seconds>(cCooldown - cSinceLast).count());
    spdlog::info("[PartyTeleport]: {} may not move {} to {}: refusal {}", apAsker->GetUsername().c_str(), apMover->GetUsername().c_str(),
                 apOther->GetUsername().c_str(), static_cast<int>(cRefusal));
    Refuse(apAsker, cRefusal, apAsker == apMover ? apOther : apMover, secondsLeft);
    return false;
}

void PartyTeleportService::OnTeleport(const PacketEvent<TeleportRequest>& acMessage) noexcept
{
    const Player* pPlayer = acMessage.pPlayer;
    Player* pTargetPlayer = m_world.GetPlayerManager().GetById(acMessage.Packet.PlayerId);
    if (!pTargetPlayer || pTargetPlayer == pPlayer)
    {
        Refuse(pPlayer, PartyTeleport::kUnavailable, pTargetPlayer);
        return;
    }

    if (!Allowed(pPlayer, pPlayer, pTargetPlayer))
        return;

    m_lastMoves[pPlayer->GetId()] = std::chrono::steady_clock::now();
    pPlayer->Send(TeleportTo(m_world, *pTargetPlayer));
}

void PartyTeleportService::OnSummon(const PacketEvent<RequestPartySummon>& acMessage) noexcept
{
    const Player* pSummoner = acMessage.pPlayer;
    Player* pTarget = m_world.GetPlayerManager().GetById(acMessage.Packet.PlayerId);
    if (!pTarget || pTarget == pSummoner)
    {
        Refuse(pSummoner, PartyTeleport::kUnavailable, pTarget);
        return;
    }

    if (!Allowed(pSummoner, pTarget, pSummoner))
        return;

    m_summons[pTarget->GetId()] = {pSummoner->GetId(), std::chrono::steady_clock::now()};
    NotifyPartySummon summons{};
    summons.PlayerId = pSummoner->GetId();
    summons.PlayerName = pSummoner->GetUsername();
    pTarget->Send(summons);
}

void PartyTeleportService::OnSummonAnswer(const PacketEvent<RequestPartySummonAnswer>& acMessage) noexcept
{
    const Player* pTarget = acMessage.pPlayer;
    const auto cSummons = m_summons.find(pTarget->GetId());
    if (cSummons == m_summons.end() || cSummons->second.SummonerId != acMessage.Packet.PlayerId)
    {
        spdlog::warn("[PartyTeleport]: {} answered summons from {} that it doesn't have", pTarget->GetUsername().c_str(), acMessage.Packet.PlayerId);
        return;
    }
    const bool cExpired = std::chrono::steady_clock::now() - cSummons->second.Time > kSummonsTimeout;
    m_summons.erase(cSummons);

    Player* pSummoner = m_world.GetPlayerManager().GetById(acMessage.Packet.PlayerId);
    if (!pSummoner || cExpired)
    {
        Refuse(pTarget, PartyTeleport::kUnavailable, pSummoner);
        return;
    }

    if (!acMessage.Packet.Accept)
    {
        Refuse(pSummoner, PartyTeleport::kDeclined, pTarget);
        return;
    }

    // Things may have changed while the summons waited: still in the party, not in a fight since. The cooldown was
    // checked when it was sent.
    const PartyTeleport::Refusal cRefusal = PartyTeleport::Check(SameParty(*pTarget, *pSummoner), pTarget->IsInCombat(), std::chrono::steady_clock::duration::max(), {});
    if (cRefusal != PartyTeleport::kAllowed)
    {
        Refuse(pTarget, cRefusal, pSummoner);
        Refuse(pSummoner, cRefusal, pTarget);
        return;
    }

    m_lastMoves[pSummoner->GetId()] = std::chrono::steady_clock::now();
    pTarget->Send(TeleportTo(m_world, *pSummoner));
}

void PartyTeleportService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    const uint32_t cId = acEvent.pPlayer->GetId();
    m_lastMoves.erase(cId);
    m_summons.erase(cId);
}

void PartyTeleportService::OnRespawnAtPartner(const PacketEvent<RequestRespawnAtPartner>& acMessage) const noexcept
{
    const Player* pPlayer = acMessage.pPlayer;
    if (!bRespawnAtPartner)
        return;

    // A member out of combat if there is one, so the respawned player doesn't land back in the fight
    std::vector<PartyTeleport::Partner> members;
    for (const Player* pOther : m_world.GetPlayerManager())
    {
        if (pOther != pPlayer && SameParty(*pPlayer, *pOther))
            members.push_back({pOther->GetId(), pOther->GetCharacter().has_value(), pOther->IsInCombat()});
    }
    const auto cPartnerId = PartyTeleport::RespawnPartner(members);
    const Player* pPartner = cPartnerId ? m_world.GetPlayerManager().GetById(*cPartnerId) : nullptr;

    if (!pPartner)
    {
        spdlog::info("[PartyTeleport]: {} respawned with no party member to join, staying at the respawn point", pPlayer->GetUsername().c_str());
        return;
    }

    spdlog::info("[PartyTeleport]: {} respawns next to {}", pPlayer->GetUsername().c_str(), pPartner->GetUsername().c_str());
    pPlayer->Send(CommandService::WhereIs(m_world, *pPartner));
}
