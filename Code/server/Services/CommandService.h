#pragma once

#include <Events/PacketEvent.h>

struct World;
struct Player;
struct TeleportCommandRequest;
struct SetTimeCommandRequest;
struct RequestModeratePlayer;
struct TeleportCommandResponse;
struct RequestTimeSkip;

/**
 * @brief Processes incoming commands.
 */
struct CommandService
{
    CommandService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~CommandService() noexcept = default;

    TP_NOCOPYMOVE(CommandService);

    // A teleport to the player's location, empty while they have no character
    static TeleportCommandResponse WhereIs(World& aWorld, const Player& acPlayer) noexcept;

protected:
    void OnSetTimeCommand(const PacketEvent<SetTimeCommandRequest>& acMessage) const noexcept;
    /**
     * @brief Advances the shared time after a player slept or waited (#467). Same permissions as /settime.
     */
    void OnTimeSkipRequest(const PacketEvent<RequestTimeSkip>& acMessage) const noexcept;
    bool CanChangeTime(const Player* apPlayer) const noexcept;
    /**
     * @brief Returns the location of the target player of the teleport command.
     */
    void OnTeleportCommandRequest(const PacketEvent<TeleportCommandRequest>& acMessage) const noexcept;
    // Kick or ban, for the host (on the server's computer) or an admin
    void OnModeratePlayer(const PacketEvent<RequestModeratePlayer>& acMessage) const noexcept;

private:
    World& m_world;

    entt::scoped_connection m_setTimeConnection;
    entt::scoped_connection m_timeSkipConnection;
    entt::scoped_connection m_teleportConnection;
    entt::scoped_connection m_moderateConnection;
};
