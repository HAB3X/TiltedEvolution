#include <Services/HostWorldService.h>

#include <GameServer.h>
#include <World.h>

#include <Services/CommandService.h>

#include <Messages/HostWorldStateRequest.h>
#include <Messages/NotifyHostWorldRequested.h>
#include <Messages/NotifyHostWorldState.h>
#include <Messages/RequestHostWorld.h>
#include <Messages/TeleportCommandResponse.h>

HostWorldService::HostWorldService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_requestConnection(aDispatcher.sink<PacketEvent<RequestHostWorld>>().connect<&HostWorldService::OnRequestHostWorld>(this))
    , m_stateConnection(aDispatcher.sink<PacketEvent<HostWorldStateRequest>>().connect<&HostWorldService::OnHostWorldState>(this))
{
}

Player* HostWorldService::GetHost(Player* apPlayer) noexcept
{
    if (!GameServer::Get()->IsHostWorld())
        return nullptr;

    const PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(apPlayer);
    return pParty ? m_world.GetPlayerManager().GetById(pParty->LeaderPlayerId) : nullptr;
}

void HostWorldService::OnRequestHostWorld(const PacketEvent<RequestHostWorld>& acPacket) noexcept
{
    Player* pJoiner = acPacket.pPlayer;
    Player* pHost = GetHost(pJoiner);
    if (!pHost || pHost == pJoiner)
    {
        spdlog::warn("[HostWorld]: {} asked for the host's world, but there's no other host for them", pJoiner->GetUsername().c_str());
        return;
    }

    spdlog::info("[HostWorld]: {} joins {}'s world", pJoiner->GetUsername().c_str(), pHost->GetUsername().c_str());

    NotifyHostWorldRequested request;
    request.PlayerId = pJoiner->GetId();
    pHost->Send(request);
}

void HostWorldService::OnHostWorldState(const PacketEvent<HostWorldStateRequest>& acPacket) noexcept
{
    const auto& message = acPacket.Packet;

    // Only the joiner's host may hand them a world, and they may have left while the host collected it
    Player* pJoiner = m_world.GetPlayerManager().GetById(message.PlayerId);
    if (!pJoiner || GetHost(pJoiner) != acPacket.pPlayer)
    {
        spdlog::warn("[HostWorld]: dropped a world from {} for player {}, who isn't a joiner of theirs", acPacket.pPlayer->GetUsername().c_str(), message.PlayerId);
        return;
    }

    spdlog::info(
        "[HostWorld]: sending {} the host's world: {} running and {} completed quests, {} cleared locations, {} dead unique NPCs", pJoiner->GetUsername().c_str(),
        message.State.RunningQuests.Entries.size(), message.State.CompletedQuests.Entries.size(), message.State.ClearedLocations.size(), message.State.DeadUniqueActors.size());

    NotifyHostWorldState notify;
    notify.HostName = acPacket.pPlayer->GetUsername();
    notify.State = message.State;
    pJoiner->Send(notify);

    // After the world, so that the joiner arrives in it
    pJoiner->Send(CommandService::WhereIs(m_world, *acPacket.pPlayer));
}
