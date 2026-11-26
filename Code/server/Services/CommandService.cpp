#include <Services/CommandService.h>

#include <Components.h>
#include <GameServer.h>
#include <World.h>

#include <Messages/SetTimeCommandRequest.h>
#include <Messages/NotifySetTimeResult.h>
#include <Messages/TeleportCommandRequest.h>
#include <Messages/TeleportCommandResponse.h>
#include <Messages/RequestModeratePlayer.h>

#include <Setting.h>

namespace
{
Console::Setting bAnnounceServer{"LiveServices:bAnnounceServer", "Whether to list the server on the public server list", false};
}

CommandService::CommandService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    m_setTimeConnection = aDispatcher.sink<PacketEvent<SetTimeCommandRequest>>().connect<&CommandService::OnSetTimeCommand>(this);
    m_teleportConnection = aDispatcher.sink<PacketEvent<TeleportCommandRequest>>().connect<&CommandService::OnTeleportCommandRequest>(this);
    m_moderateConnection = aDispatcher.sink<PacketEvent<RequestModeratePlayer>>().connect<&CommandService::OnModeratePlayer>(this);
}

void CommandService::OnSetTimeCommand(const PacketEvent<SetTimeCommandRequest>& acMessage) const noexcept
{
    NotifySetTimeResult response{};

    const auto cPlayerId = static_cast<uint32_t>(acMessage.Packet.PlayerId);

    // Admin override: always allowed
    for (const auto session : GameServer::Get()->GetAdminSessions())
    {
        if (PlayerManager::Get()->GetByConnectionId(session)->GetId() == cPlayerId)
        {
            const auto cHours = static_cast<int>(acMessage.Packet.Hours);
            const auto cMinutes = static_cast<int>(acMessage.Packet.Minutes);

            m_world.GetCalendarService().SetTime(cHours, cMinutes, m_world.GetCalendarService().GetTimeScale());

            response.Result = NotifySetTimeResult::SetTimeResult::kSuccess;
            acMessage.pPlayer->Send(response);

            return;
        }
    }

    // Party leader allowed on private servers only
    const auto* pPartyService = &m_world.GetPartyService();
    if (pPartyService->IsPlayerLeader(acMessage.pPlayer) && !bAnnounceServer)
    {
        const auto cHours = static_cast<int>(acMessage.Packet.Hours);
        const auto cMinutes = static_cast<int>(acMessage.Packet.Minutes);

        m_world.GetCalendarService().SetTime(cHours, cMinutes, m_world.GetCalendarService().GetTimeScale());

        response.Result = NotifySetTimeResult::SetTimeResult::kSuccess;
        acMessage.pPlayer->Send(response);

        return;
    }

    response.Result = NotifySetTimeResult::SetTimeResult::kNoPermission;
    acMessage.pPlayer->Send(response);
}

void CommandService::OnTeleportCommandRequest(const PacketEvent<TeleportCommandRequest>& acMessage) const noexcept
{
    Player* pTargetPlayer = nullptr;
    for (Player* pPlayer : m_world.GetPlayerManager())
    {
        if (pPlayer->GetUsername() == acMessage.Packet.TargetPlayer)
            pTargetPlayer = pPlayer;
    }

    acMessage.pPlayer->Send(pTargetPlayer ? WhereIs(m_world, *pTargetPlayer) : TeleportCommandResponse{});
}

TeleportCommandResponse CommandService::WhereIs(World& aWorld, const Player& acPlayer) noexcept
{
    TeleportCommandResponse response{};
    if (auto character = acPlayer.GetCharacter())
    {
        if (const auto* pMovementComponent = aWorld.try_get<MovementComponent>(*character))
        {
            const auto& cellComponent = acPlayer.GetCellComponent();
            response.CellId = cellComponent.Cell;
            response.Position = pMovementComponent->Position;
            response.WorldSpaceId = cellComponent.WorldSpaceId;
        }
    }
    return response;
}

void CommandService::OnModeratePlayer(const PacketEvent<RequestModeratePlayer>& acMessage) const noexcept
{
    const Player* pSender = acMessage.pPlayer;

    // Whoever plays on the server's own computer hosts it (Host a game); admins moderate anywhere
    const auto& cAdmins = GameServer::Get()->GetAdminSessions();
    if (!pSender->IsOnServerComputer() && !cAdmins.contains(pSender->GetConnectionId()))
    {
        spdlog::warn("{} tried to {} player {} without being the host or an admin", pSender->GetUsername().c_str(), acMessage.Packet.Ban ? "ban" : "kick",
                     acMessage.Packet.PlayerId);
        return;
    }

    Player* pTarget = m_world.GetPlayerManager().GetById(acMessage.Packet.PlayerId);
    if (!pTarget || pTarget == pSender)
        return;

    if (acMessage.Packet.Ban)
        GameServer::Get()->Ban(*pTarget);
    else
    {
        spdlog::info("{} kicked {}", pSender->GetUsername().c_str(), pTarget->GetUsername().c_str());
        GameServer::Get()->Kick(pTarget->GetConnectionId());
    }
}
