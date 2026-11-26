#pragma once

#include <LanDiscovery.h> // encoding/

#include <chrono>
#include <cstdint>
#include <string>

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

    // Answers each search waiting on the socket with this server's details, and finishes the network check
    void Poll(const LanDiscovery::Reply& acReply) noexcept;

private:
    // Searches for this server through this computer's own network addresses, as a player on the network would. No
    // answer means something on this computer, usually a VPN or the firewall, keeps the network out.
    void AnswerSearches(const LanDiscovery::Reply& acReply) noexcept;
    void StartNetworkCheck() noexcept;
    void PollNetworkCheck() noexcept;

    uintptr_t m_socket;
    bool m_open{false};

    uintptr_t m_checkSocket;
    std::string m_checkedAddresses;
    std::chrono::steady_clock::time_point m_checkDeadline{};
    bool m_checking{false};
};
