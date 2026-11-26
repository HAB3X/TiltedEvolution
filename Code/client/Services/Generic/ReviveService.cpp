#include <Services/ReviveService.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Events/ActivateEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Messages/NotifyPlayerHealthUpdate.h>
#include <Messages/NotifyRevive.h>
#include <Messages/RequestRevive.h>

#include <Games/ActorExtension.h>
#include <PlayerCharacter.h>
#include <WireProtocol.h>

namespace
{
// A downed player nobody revives dies after this, so someone whose partners are far away isn't stuck
constexpr std::chrono::seconds kReviveWindow{120};
// Activations of the same downed partner closer than this send one revive
constexpr std::chrono::seconds kReviveRequestInterval{1};
} // namespace

ReviveService::ReviveService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_activateConnection = aDispatcher.sink<ActivateEvent>().connect<&ReviveService::OnActivate>(this);
    m_healthConnection = aDispatcher.sink<NotifyPlayerHealthUpdate>().connect<&ReviveService::OnPlayerHealthUpdate>(this);
    m_reviveConnection = aDispatcher.sink<NotifyRevive>().connect<&ReviveService::OnRevive>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&ReviveService::OnPartyLeft>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&ReviveService::OnDisconnected>(this);
}

bool ReviveService::StartWaiting() noexcept
{
    m_revived = false;
    m_partyDown = false;
    // Official v1.8.4 servers have no reviving; a party of one has nobody to do it
    if (!m_transport.IsOnline() || Wire::IsOfficial184() || !m_world.GetServerSettings().ReviveEnabled || m_world.GetPartyService().GetPartyMembers().size() < 2)
        return false;

    m_waiting = true;
    m_giveUpAt = std::chrono::steady_clock::now() + kReviveWindow;
    m_world.GetOverlayService().SendSystemMessage("You are down. A party member can revive you by activating you.");
    return true;
}

ReviveService::Wait ReviveService::Poll() noexcept
{
    if (m_revived)
    {
        m_waiting = m_revived = false;
        return Wait::kRevived;
    }

    if (m_partyDown || !m_world.GetPartyService().IsInParty() || std::chrono::steady_clock::now() >= m_giveUpAt)
    {
        spdlog::info("Nobody revived the player ({}), they die", m_partyDown ? "the whole party is down" : "out of the party or out of time");
        m_waiting = m_partyDown = false;
        return Wait::kGiveUp;
    }

    return Wait::kWaiting;
}

void ReviveService::OnActivate(const ActivateEvent& acEvent) noexcept
{
    // A downed partner's character, activated by this player
    if (!m_transport.IsOnline() || Wire::IsOfficial184() || acEvent.ActivatorId != 0x14)
        return;
    Actor* pTarget = Cast<Actor>(TESForm::GetById(acEvent.ObjectId));
    if (!pTarget || pTarget != acEvent.pObject || !pTarget->GetExtension()->IsRemotePlayer())
        return;

    const auto view = m_world.view<FormIdComponent, PlayerComponent>();
    const auto cIt = std::find_if(view.begin(), view.end(), [&](const entt::entity aEntity) { return view.get<FormIdComponent>(aEntity).Id == acEvent.ObjectId; });
    if (cIt == view.end())
        return;
    const uint32_t cPlayerId = view.get<PlayerComponent>(*cIt).Id;
    if (!m_downedPartners.contains(cPlayerId))
        return;

    const auto cNow = std::chrono::steady_clock::now();
    if (cNow < m_nextReviveRequest)
        return;
    m_nextReviveRequest = cNow + kReviveRequestInterval;

    RequestRevive request;
    request.PlayerId = cPlayerId;
    m_transport.Send(request);
    spdlog::info("Reviving player {:X}", cPlayerId);
}

void ReviveService::OnPlayerHealthUpdate(const NotifyPlayerHealthUpdate& acMessage) noexcept
{
    if (acMessage.Vitals.Downed)
        m_downedPartners.insert(acMessage.PlayerId);
    else
        m_downedPartners.erase(acMessage.PlayerId);
}

void ReviveService::OnRevive(const NotifyRevive& acMessage) noexcept
{
    if (!m_waiting)
        return;

    if (acMessage.Result == NotifyRevive::Outcome::kPartyDown)
    {
        m_partyDown = true;
        m_world.GetOverlayService().SendSystemMessage("Your whole party is down.");
        return;
    }

    m_revived = true;
    m_world.GetOverlayService().SendSystemMessage(fmt::format("{} revived you.", acMessage.ReviverName.c_str()));
}

void ReviveService::OnPartyLeft(const PartyLeftEvent&) noexcept
{
    m_downedPartners.clear();
}

void ReviveService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_downedPartners.clear();
    m_waiting = m_revived = m_partyDown = false;
}
