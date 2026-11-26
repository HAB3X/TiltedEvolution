#include <LanDiscoveryResponder.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
using SocketLength = int;
namespace
{
constexpr uintptr_t kNoSocket = INVALID_SOCKET;
int LastError() { return WSAGetLastError(); }
bool WouldBlock(int aError) { return aError == WSAEWOULDBLOCK || aError == WSAECONNRESET; }
void CloseSocket(uintptr_t aSocket) { closesocket(static_cast<SOCKET>(aSocket)); }
bool MakeNonBlocking(uintptr_t aSocket)
{
    u_long nonBlocking = 1;
    return ioctlsocket(static_cast<SOCKET>(aSocket), FIONBIO, &nonBlocking) == 0;
}
} // namespace
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketLength = socklen_t;
namespace
{
constexpr uintptr_t kNoSocket = static_cast<uintptr_t>(-1);
int LastError() { return errno; }
bool WouldBlock(int aError) { return aError == EWOULDBLOCK || aError == EAGAIN; }
void CloseSocket(uintptr_t aSocket) { close(static_cast<int>(aSocket)); }
bool MakeNonBlocking(uintptr_t aSocket) { return fcntl(static_cast<int>(aSocket), F_SETFL, O_NONBLOCK) == 0; }
} // namespace
#endif

#include <cstring>

LanDiscoveryResponder::LanDiscoveryResponder() noexcept
    : m_socket(kNoSocket)
{
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    m_socket = static_cast<uintptr_t>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (m_socket == kNoSocket)
    {
        spdlog::error("Local network discovery is off: no socket ({})", LastError());
        return;
    }

    // Several servers on one computer can all answer
    int reuse = 1;
    setsockopt(static_cast<decltype(socket(0, 0, 0))>(m_socket), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(LanDiscovery::kPort);
    if (bind(static_cast<decltype(socket(0, 0, 0))>(m_socket), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || !MakeNonBlocking(m_socket))
    {
        spdlog::error("Local network discovery is off: UDP port {} is unavailable ({})", LanDiscovery::kPort, LastError());
        CloseSocket(m_socket);
        return;
    }

    m_open = true;
    spdlog::info("Players on the local network can find this server (UDP port {})", LanDiscovery::kPort);
}

LanDiscoveryResponder::~LanDiscoveryResponder() noexcept
{
    if (m_open)
        CloseSocket(m_socket);
}

void LanDiscoveryResponder::Poll(const LanDiscovery::Reply& acReply) noexcept
{
    if (!m_open)
        return;

    const auto cSocket = static_cast<decltype(socket(0, 0, 0))>(m_socket);
    std::string reply;
    for (;;)
    {
        char buffer[64];
        sockaddr_in from{};
        SocketLength fromLength = sizeof(from);
        const auto cRead = recvfrom(cSocket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (cRead < 0)
        {
            if (!WouldBlock(LastError()))
                spdlog::warn("Local network discovery: receiving failed ({})", LastError());
            return;
        }

        if (static_cast<size_t>(cRead) != std::strlen(LanDiscovery::kQuery) || std::memcmp(buffer, LanDiscovery::kQuery, cRead) != 0)
            continue;

        if (reply.empty())
            reply = acReply.Serialize();
        sendto(cSocket, reply.data(), static_cast<int>(reply.size()), 0, reinterpret_cast<sockaddr*>(&from), fromLength);
    }
}
