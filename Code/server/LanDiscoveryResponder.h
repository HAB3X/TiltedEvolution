#pragma once

#include <LanDiscovery.h> // encoding/

#include <cstdint>

/**
 * @brief Answers the local network searches of game clients (GameServer:bLanDiscovery), see encoding/LanDiscovery.h.
 *
 * Polled from the server's update, never blocks.
 */
struct LanDiscoveryResponder
{
    LanDiscoveryResponder() noexcept;
    ~LanDiscoveryResponder() noexcept;

    LanDiscoveryResponder(const LanDiscoveryResponder&) = delete;
    LanDiscoveryResponder& operator=(const LanDiscoveryResponder&) = delete;

    // Answers each search waiting on the socket with this server's details
    void Poll(const LanDiscovery::Reply& acReply) noexcept;

private:
    uintptr_t m_socket;
    bool m_open{false};
};
