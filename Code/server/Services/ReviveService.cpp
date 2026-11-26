#include <Services/ReviveService.h>

#include <Components.h>
#include <GameServer.h>
#include <Setting.h>
#include <World.h>

#include <Messages/NotifyRevive.h>
#include <Messages/RequestPlayerHealthUpdate.h>
#include <Messages/RequestRevive.h>

namespace
{
Console::Setting bReviveInParty{"Gameplay:bReviveInParty", "A downed player waits for a party member to revive them instead of dying, until the whole party is down", true};
} // namespace

bool ReviveService::IsEnabled() noexcept
{
    return bReviveInParty;
}

ReviveService::ReviveService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_healthUpdateConnection(aDispatcher.sink<PacketEvent<RequestPlayerHealthUpdate>>().connect<&ReviveService::OnHealthUpdate>(this))
    , m_reviveConnection(aDispatcher.sink<PacketEvent<RequestRevive>>().connect<&ReviveService::OnRevive>(this))
{
}

void ReviveService::OnHealthUpdate(const PacketEvent<RequestPlayerHealthUpdate>& acMessage) const noexcept
{
    Player* const pPlayer = acMessage.pPlayer;
    const bool cWasDowned = pPlayer->IsDowned();
    pPlayer->SetDowned(acMessage.Packet.Vitals.Downed);
    if (!IsEnabled() || cWasDowned || !pPlayer->IsDowned())
        return;

    const PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(pPlayer);
    if (!pParty || pParty->Members.size() < 2)
        return;
    if (!std::all_of(pParty->Members.begin(), pParty->Members.end(), [](const Player* apMember) { return apMember->IsDowned(); }))
        return;

    spdlog::info("Every member of player {:X}'s party is down: nobody is left to revive them", pPlayer->GetId());
    NotifyRevive notify;
    notify.Result = NotifyRevive::Outcome::kPartyDown;
    for (const Player* pMember : pParty->Members)
        pMember->Send(notify);
}

void ReviveService::OnRevive(const PacketEvent<RequestRevive>& acMessage) const noexcept
{
    Player* const pReviver = acMessage.pPlayer;
    Player* const pTarget = m_world.GetPlayerManager().GetById(acMessage.Packet.PlayerId);
    const auto refuse = [&](const char* apReason) { spdlog::info("Player {:X} can't revive player {:X}: {}", pReviver->GetId(), acMessage.Packet.PlayerId, apReason); };

    if (!IsEnabled())
        return refuse("reviving is off on this server");
    if (!pTarget || pTarget == pReviver)
        return refuse("no such other player");
    if (!pTarget->IsDowned())
        return refuse("they aren't down");
    if (pReviver->IsDowned())
        return refuse("the reviver is down too");
    const auto& cReviverParty = pReviver->GetParty().JoinedPartyId;
    if (!cReviverParty || cReviverParty != pTarget->GetParty().JoinedPartyId)
        return refuse("they aren't in the same party");
    if (!pReviver->GetCellComponent().IsInRange(pTarget->GetCellComponent(), false))
        return refuse("they are too far away");

    // Up from now on, so a second reviver or the party-down check doesn't count them as down
    pTarget->SetDowned(false);
    NotifyRevive notify;
    notify.Result = NotifyRevive::Outcome::kRevived;
    notify.ReviverId = pReviver->GetId();
    notify.ReviverName = pReviver->GetUsername();
    pTarget->Send(notify);
    spdlog::info("Player {:X} revives player {:X}", pReviver->GetId(), pTarget->GetId());
}
