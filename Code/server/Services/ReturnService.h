#pragma once

#include <Messages/TeleportCommandResponse.h>

struct World;
struct Player;

/**
 * @brief Puts players who lost their connection back where they were when they come back.
 *
 * A player whose connection is lost (a timeout, or their game crashed) is remembered for a while by their name and
 * address: their party, whether they led it, and where the server last saw them. When they log in again from the
 * same address they rejoin that party (and lead it again), and if their game restarted (AuthenticationRequest::Returning)
 * they are moved back to that place.
 */
struct ReturnService
{
    explicit ReturnService(World& aWorld) noexcept;
    ~ReturnService() noexcept = default;

    TP_NOCOPYMOVE(ReturnService);

    // Call before the player's cell is cleared
    void OnLost(Player& aPlayer) noexcept;
    // Call once the player has joined (after PlayerJoinEvent)
    void OnJoined(Player& aPlayer, bool aReturning) noexcept;

private:
    struct Lost
    {
        String Key;
        std::optional<uint32_t> PartyId;
        bool WasLeader{};
        TeleportCommandResponse Where{};
        uint64_t LostAt{};
    };

    static String KeyOf(const Player& acPlayer) noexcept;

    World& m_world;
    Vector<Lost> m_lost;
};
