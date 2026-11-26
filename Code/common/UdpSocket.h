#pragma once

// A non-blocking IPv4 UDP socket for Windows and POSIX, header-only so small tools (Code/relay) can use it without the
// rest of CommonLib.

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Udp
{
// An IPv4 address and port, in host byte order
struct Endpoint
{
    uint32_t Address{};
    uint16_t Port{};

    bool operator==(const Endpoint& acRhs) const noexcept { return Address == acRhs.Address && Port == acRhs.Port; }
    bool operator<(const Endpoint& acRhs) const noexcept { return Address != acRhs.Address ? Address < acRhs.Address : Port < acRhs.Port; }

    // "1.2.3.4"
    std::string AddressString() const
    {
        return std::to_string(Address >> 24) + "." + std::to_string((Address >> 16) & 0xFF) + "." + std::to_string((Address >> 8) & 0xFF) + "." +
               std::to_string(Address & 0xFF);
    }
    // "1.2.3.4:5"
    std::string ToString() const { return AddressString() + ":" + std::to_string(Port); }
};

class Socket
{
public:
#ifdef _WIN32
    using Handle = SOCKET;
    static constexpr Handle kNone = INVALID_SOCKET;
#else
    using Handle = int;
    static constexpr Handle kNone = -1;
#endif

    Socket() = default;
    ~Socket() { Close(); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& aOther) noexcept
        : m_handle(std::exchange(aOther.m_handle, kNone))
    {
    }
    Socket& operator=(Socket&& aOther) noexcept
    {
        if (this != &aOther)
        {
            Close();
            m_handle = std::exchange(aOther.m_handle, kNone);
        }
        return *this;
    }

    // Winsock needs starting once per process; harmless elsewhere
    static void StartUp()
    {
#ifdef _WIN32
        static const bool s_started = []
        {
            WSADATA data;
            return WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();
        (void)s_started;
#endif
    }

    // Binds to aAddress:aPort (host byte order, 0 for any); false with LastError() set when it can't
    bool Open(uint32_t aAddress = 0, uint16_t aPort = 0)
    {
        StartUp();
        Close();
        m_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_handle == kNone)
            return false;
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(aAddress);
        local.sin_port = htons(aPort);
#ifdef _WIN32
        u_long nonBlocking = 1;
        const bool cNonBlocking = ioctlsocket(m_handle, FIONBIO, &nonBlocking) == 0;
#else
        const bool cNonBlocking = fcntl(m_handle, F_SETFL, O_NONBLOCK) == 0;
#endif
        if (!cNonBlocking || bind(m_handle, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0)
        {
            const int cError = LastError();
            Close();
            SetLastError(cError);
            return false;
        }
        return true;
    }

    void Close()
    {
        if (m_handle == kNone)
            return;
#ifdef _WIN32
        closesocket(m_handle);
#else
        close(m_handle);
#endif
        m_handle = kNone;
    }

    bool IsOpen() const noexcept { return m_handle != kNone; }

    uint16_t LocalPort() const
    {
        sockaddr_in local{};
#ifdef _WIN32
        int length = sizeof(local);
#else
        socklen_t length = sizeof(local);
#endif
        return getsockname(m_handle, reinterpret_cast<sockaddr*>(&local), &length) == 0 ? ntohs(local.sin_port) : 0;
    }

    bool SendTo(const Endpoint& acTo, const void* apData, size_t aSize) const
    {
        const sockaddr_in cTo = ToNative(acTo);
        return sendto(m_handle, static_cast<const char*>(apData), static_cast<int>(aSize), 0, reinterpret_cast<const sockaddr*>(&cTo), sizeof(cTo)) ==
               static_cast<int>(aSize);
    }
    bool SendTo(const Endpoint& acTo, const std::string& acData) const { return SendTo(acTo, acData.data(), acData.size()); }

    // One waiting datagram's size, or empty when there is none
    std::optional<size_t> Receive(char* apBuffer, size_t aSize, Endpoint& aFrom) const
    {
        for (;;)
        {
            sockaddr_in from{};
#ifdef _WIN32
            int length = sizeof(from);
#else
            socklen_t length = sizeof(from);
#endif
            const auto cRead = recvfrom(m_handle, apBuffer, static_cast<int>(aSize), 0, reinterpret_cast<sockaddr*>(&from), &length);
            if (cRead >= 0)
            {
                aFrom = {ntohl(from.sin_addr.s_addr), ntohs(from.sin_port)};
                return static_cast<size_t>(cRead);
            }
#ifdef _WIN32
            // Windows reports an earlier send's "port unreachable" on the next receive: not a datagram, look again
            if (WSAGetLastError() == WSAECONNRESET)
                continue;
#endif
            return std::nullopt;
        }
    }

    // Waits up to aMilliseconds for a datagram on any of the sockets
    static void Wait(const std::vector<const Socket*>& acSockets, int aMilliseconds)
    {
        fd_set read;
        FD_ZERO(&read);
        Handle highest = 0;
        for (const Socket* pSocket : acSockets)
        {
            if (!pSocket->IsOpen())
                continue;
            FD_SET(pSocket->m_handle, &read);
            highest = std::max(highest, pSocket->m_handle);
        }
        timeval timeout{aMilliseconds / 1000, (aMilliseconds % 1000) * 1000};
        select(static_cast<int>(highest + 1), &read, nullptr, nullptr, &timeout);
    }

    static int LastError()
    {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

private:
    static void SetLastError(int aError)
    {
#ifdef _WIN32
        WSASetLastError(aError);
#else
        errno = aError;
#endif
    }

    static sockaddr_in ToNative(const Endpoint& acEndpoint)
    {
        sockaddr_in native{};
        native.sin_family = AF_INET;
        native.sin_addr.s_addr = htonl(acEndpoint.Address);
        native.sin_port = htons(acEndpoint.Port);
        return native;
    }

    Handle m_handle{kNone};
};

// "host:port" or "host" (then aDefaultPort), by name or IPv4 address; empty when it doesn't resolve
inline std::optional<Endpoint> Resolve(const std::string& acHostPort, uint16_t aDefaultPort)
{
    Socket::StartUp();
    std::string host = acHostPort;
    uint32_t port = aDefaultPort;
    if (const size_t cColon = acHostPort.rfind(':'); cColon != std::string::npos)
    {
        host = acHostPort.substr(0, cColon);
        char* pEnd = nullptr;
        port = std::strtoul(acHostPort.c_str() + cColon + 1, &pEnd, 10);
        if (*pEnd != '\0' || port == 0 || port > 65535)
            return std::nullopt;
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* pResult = nullptr;
    if (host.empty() || getaddrinfo(host.c_str(), nullptr, &hints, &pResult) != 0 || !pResult)
        return std::nullopt;
    const Endpoint cEndpoint{ntohl(reinterpret_cast<sockaddr_in*>(pResult->ai_addr)->sin_addr.s_addr), static_cast<uint16_t>(port)};
    freeaddrinfo(pResult);
    return cEndpoint;
}
} // namespace Udp
