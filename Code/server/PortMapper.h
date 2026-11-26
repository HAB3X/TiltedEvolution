#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

/**
 * @brief Opens the server's UDP port on the home router with UPnP (GameServer:bUPnP), so friends outside the local
 * network can join without forwarding it by hand, and closes it again when the server stops.
 *
 * Runs in the background: finding the router takes a few seconds. The outcome is logged with the [Host] tag, which
 * the game's Host a game button shows in chat (client LocalServerService). aOnUnreachable runs on that background
 * thread when friends outside can't reach the port: no router, a refusal, or a router behind another NAT.
 */
struct PortMapper
{
    PortMapper(uint16_t aPort, std::function<void()> aOnUnreachable) noexcept;
    ~PortMapper() noexcept;

    PortMapper(const PortMapper&) = delete;
    PortMapper& operator=(const PortMapper&) = delete;

private:
    struct Router
    {
        std::string Host;
        int Port{};
        std::string ControlPath;
        std::string ServiceType;
        std::string LocalAddress;
    };

    // False when friends outside the network can't reach the port
    bool Run() noexcept;
    bool FindRouter(Router& aRouter, std::string& aError) const noexcept;
    bool Soap(const Router& acRouter, const std::string& acAction, const std::string& acArguments, std::string& aResponse) const noexcept;

    uint16_t m_port;
    std::thread m_thread;
    // Set once the mapping exists, so that it's removed on shutdown
    std::atomic<bool> m_mapped{false};
    Router m_router;
};
