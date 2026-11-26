#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

/**
 * @brief Lets friends join through a relay (SkyrimTogetherRelay, see encoding/Relay.h) when the router port can't
 * be opened (GameServer:sRelay).
 *
 * Asks the relay for a port and logs it as the public address with the [Host] tag, so the host's invite shows it.
 * Each friend's packets arrive from the relay tagged with their id; they reach the game from a loopback socket of
 * their own, and the game's answers go back the same way. PublicAddressOf tells the server who is behind such a socket.
 */
struct RelayClient
{
    RelayClient(std::string aRelay, std::string aPassword, uint16_t aGamePort) noexcept;
    ~RelayClient() noexcept;

    RelayClient(const RelayClient&) = delete;
    RelayClient& operator=(const RelayClient&) = delete;

    // Starts relaying in the background; once, from any thread
    void Start() noexcept;

    // The public address of the friend whose packets reach the game from this loopback port, when it is one of ours
    [[nodiscard]] std::optional<std::string> PublicAddressOf(uint16_t aLoopbackPort) const noexcept;

private:
    void Run() noexcept;

    std::string m_relay;
    std::string m_password;
    uint16_t m_gamePort;
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_stop{false};
    std::thread m_thread;

    mutable std::mutex m_addressesLock;
    std::map<uint16_t, std::string> m_publicAddresses;
};
