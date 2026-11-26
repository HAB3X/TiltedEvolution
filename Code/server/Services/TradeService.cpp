#include <Services/TradeService.h>

#include <Components.h>
#include <GameServer.h>
#include <World.h>

#include <Events/PlayerLeaveEvent.h>
#include <Messages/NotifyInventoryChanges.h>
#include <Messages/NotifyTrade.h>
#include <Messages/RequestTrade.h>
#include <Trade.h>

TradeService::TradeService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_tradeConnection(aDispatcher.sink<PacketEvent<RequestTrade>>().connect<&TradeService::OnTrade>(this))
    , m_leaveConnection(aDispatcher.sink<PlayerLeaveEvent>().connect<&TradeService::OnPlayerLeave>(this))
{
}

TradeService::Session* TradeService::Find(const Player* apPlayer) noexcept
{
    const auto cIt = std::find_if(m_sessions.begin(), m_sessions.end(), [apPlayer](const Session& acSession) { return acSession.Sides[0].pPlayer == apPlayer || acSession.Sides[1].pPlayer == apPlayer; });
    return cIt != m_sessions.end() ? &*cIt : nullptr;
}

bool TradeService::CanTrade(Player* apPlayer, Player* apPartner) const noexcept
{
    if (!apPartner || apPartner == apPlayer || !apPlayer->GetCharacter() || !apPartner->GetCharacter())
        return false;
    const auto& cParty = apPlayer->GetParty().JoinedPartyId;
    return cParty && cParty == apPartner->GetParty().JoinedPartyId && apPlayer->GetCellComponent().IsInRange(apPartner->GetCellComponent(), false);
}

void TradeService::SendState(const Session& acSession, const uint8_t aEvent) const noexcept
{
    for (int side = 0; side < 2; ++side)
    {
        const Side& cMine = acSession.Sides[side];
        const Side& cTheirs = acSession.Sides[1 - side];
        NotifyTrade notify;
        notify.PartnerId = cTheirs.pPlayer->GetId();
        notify.PartnerName = cTheirs.pPlayer->GetUsername();
        notify.What = static_cast<NotifyTrade::Event>(aEvent);
        notify.MyOffer = cMine.Offer;
        notify.TheirOffer = cTheirs.Offer;
        notify.MyConfirmed = cMine.Confirmed;
        notify.TheirConfirmed = cTheirs.Confirmed;
        cMine.pPlayer->Send(notify);
    }
}

void TradeService::OnTrade(const PacketEvent<RequestTrade>& acMessage) noexcept
{
    Player* const pPlayer = acMessage.pPlayer;
    const auto& message = acMessage.Packet;
    Player* const pPartner = m_world.GetPlayerManager().GetById(message.PartnerId);
    Session* pSession = Find(pPlayer);

    // A request about someone other than the current partner, while trading, is stale
    if (pSession && pSession->Sides[0].pPlayer != pPartner && pSession->Sides[1].pPlayer != pPartner)
        return;

    if (message.Act == RequestTrade::Action::kCancel)
    {
        if (pSession)
        {
            SendState(*pSession, static_cast<uint8_t>(NotifyTrade::Event::kCancelled));
            End(pSession);
        }
        return;
    }

    if (!CanTrade(pPlayer, pPartner) || (!pSession && Find(pPartner)))
    {
        spdlog::info("Player {:X} can't trade with player {:X}: not a party member in range, or busy", pPlayer->GetId(), message.PartnerId);
        NotifyTrade refused;
        refused.PartnerId = message.PartnerId;
        refused.What = NotifyTrade::Event::kUnavailable;
        pPlayer->Send(refused);
        if (pSession)
        {
            SendState(*pSession, static_cast<uint8_t>(NotifyTrade::Event::kCancelled));
            End(pSession);
        }
        return;
    }

    if (!pSession)
    {
        if (message.Act != RequestTrade::Action::kOffer)
            return;
        m_sessions.push_back({{{pPlayer}, {pPartner}}});
        pSession = &m_sessions.back();
    }

    Side& mine = pSession->Sides[pSession->Sides[0].pPlayer == pPlayer ? 0 : 1];
    if (message.Act == RequestTrade::Action::kOffer)
    {
        if (!Trade::IsValidOffer(message.Offer))
        {
            spdlog::warn("Player {:X} offered worn, quest or too many items in a trade", pPlayer->GetId());
            return;
        }
        mine.Offer = message.Offer;
        pSession->Sides[0].Confirmed = pSession->Sides[1].Confirmed = false;
        SendState(*pSession, static_cast<uint8_t>(NotifyTrade::Event::kUpdated));
        return;
    }

    mine.Confirmed = true;
    if (!pSession->Sides[0].Confirmed || !pSession->Sides[1].Confirmed)
    {
        SendState(*pSession, static_cast<uint8_t>(NotifyTrade::Event::kUpdated));
        return;
    }

    if (!Complete(*pSession))
    {
        pSession->Sides[0].Confirmed = pSession->Sides[1].Confirmed = false;
        SendState(*pSession, static_cast<uint8_t>(NotifyTrade::Event::kMissingItems));
        return;
    }

    SendState(*pSession, static_cast<uint8_t>(NotifyTrade::Event::kCompleted));
    End(pSession);
}

bool TradeService::Complete(const Session& acSession) const noexcept
{
    entt::entity characters[2];
    for (int side = 0; side < 2; ++side)
    {
        characters[side] = *acSession.Sides[side].pPlayer->GetCharacter();
        const auto* pInventory = m_world.try_get<InventoryComponent>(characters[side]);
        if (!pInventory || !m_world.all_of<OwnerComponent>(characters[side]) || !Trade::Holds(pInventory->Content, acSession.Sides[side].Offer))
        {
            spdlog::info("Trade refused: player {:X} no longer holds what they offered", acSession.Sides[side].pPlayer->GetId());
            return false;
        }
    }

    // Each item leaves the giver and reaches the taker, in the server's copies and as changes everyone applies once
    const auto move = [&](const entt::entity aCharacter, Inventory::Entry aItem)
    {
        m_world.get<InventoryComponent>(aCharacter).Content.AddOrRemoveEntry(aItem);
        NotifyInventoryChanges notify;
        notify.ServerId = World::ToInteger(aCharacter);
        notify.OwnershipEpoch = m_world.get<OwnerComponent>(aCharacter).OwnershipEpoch;
        notify.Item = std::move(aItem);
        GameServer::Get()->SendToPlayersInRange(notify, aCharacter);
    };
    for (int side = 0; side < 2; ++side)
    {
        for (Inventory::Entry item : acSession.Sides[side].Offer.Entries)
        {
            move(characters[1 - side], item);
            item.Count = -item.Count;
            move(characters[side], item);
        }
    }
    spdlog::info("Players {:X} and {:X} traded {} and {} stacks", acSession.Sides[0].pPlayer->GetId(), acSession.Sides[1].pPlayer->GetId(), acSession.Sides[0].Offer.Entries.size(),
                 acSession.Sides[1].Offer.Entries.size());
    return true;
}

void TradeService::End(const Session* apSession) noexcept
{
    std::erase_if(m_sessions, [apSession](const Session& acSession) { return &acSession == apSession; });
}

void TradeService::OnPlayerLeave(const PlayerLeaveEvent& acEvent) noexcept
{
    if (const Session* pSession = Find(acEvent.pPlayer))
    {
        const Side& cOther = pSession->Sides[pSession->Sides[0].pPlayer == acEvent.pPlayer ? 1 : 0];
        NotifyTrade cancelled;
        cancelled.PartnerId = acEvent.pPlayer->GetId();
        cancelled.What = NotifyTrade::Event::kCancelled;
        cOther.pPlayer->Send(cancelled);
        End(pSession);
    }
}
