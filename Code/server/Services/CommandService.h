#pragma once

#include <Events/PacketEvent.h>

struct World;
struct TeleportCommandRequest;
struct SetTimeCommandRequest;
struct TeleportCommandResponse;

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
     * @brief Returns the location of the target player of the teleport command.
     */
    void OnTeleportCommandRequest(const PacketEvent<TeleportCommandRequest>& acMessage) const noexcept;

private:
    World& m_world;

    entt::scoped_connection m_setTimeConnection;
    entt::scoped_connection m_teleportConnection;
};
