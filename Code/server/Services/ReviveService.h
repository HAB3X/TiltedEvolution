#pragma once

#include <Events/PacketEvent.h>

struct World;
struct RequestPlayerHealthUpdate;
struct RequestRevive;

/**
 * @brief Reviving in a party: a downed player stays in bleedout until a party member activates them (RequestRevive,
 * checked here: same party, near, the target down and the reviver not), and dies as without a party once every
 * member is down (NotifyRevive). Whether a player is down comes from their party bar (RequestPlayerHealthUpdate).
 */
struct ReviveService
{
    ReviveService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~ReviveService() noexcept = default;

    TP_NOCOPYMOVE(ReviveService);

    // Party members revive each other instead of dying (Gameplay:bReviveInParty)
    [[nodiscard]] static bool IsEnabled() noexcept;

private:
    void OnHealthUpdate(const PacketEvent<RequestPlayerHealthUpdate>& acMessage) const noexcept;
    void OnRevive(const PacketEvent<RequestRevive>& acMessage) const noexcept;

    World& m_world;

    entt::scoped_connection m_healthUpdateConnection;
    entt::scoped_connection m_reviveConnection;
};
