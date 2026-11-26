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
#include <netdb.h>
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
#include <vector>

namespace
{
// How long the network check waits for this server's own answer
constexpr std::chrono::seconds kCheckTime{3};

// This computer's IPv4 addresses on its networks
std::vector<in_addr> NetworkAddresses()
{
    std::vector<in_addr> addresses;
    char name[256]{};
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* pResults = nullptr;
    if (gethostname(name, sizeof(name)) != 0 || getaddrinfo(name, nullptr, &hints, &pResults) != 0)
        return addresses;
    for (const addrinfo* pResult = pResults; pResult; pResult = pResult->ai_next)
    {
        const in_addr cAddress = reinterpret_cast<const sockaddr_in*>(pResult->ai_addr)->sin_addr;
        const uint32_t cHost = ntohl(cAddress.s_addr);
        // Not loopback or link-local, which the network can't use
        if ((cHost >> 24) != 127 && (cHost >> 16) != 0xA9FE)
            addresses.push_back(cAddress);
    }
    freeaddrinfo(pResults);
    return addresses;
}
} // namespace

LanDiscoveryResponder::LanDiscoveryResponder() noexcept
    : m_socket(kNoSocket)
    , m_checkSocket(kNoSocket)
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
    StartNetworkCheck();
}

void LanDiscoveryResponder::StartNetworkCheck() noexcept
{
    const auto cAddresses = NetworkAddresses();
    if (cAddresses.empty())
    {
        spdlog::warn("[Host] Network check: this computer has no network address, only players on it can join");
        return;
    }

    m_checkSocket = static_cast<uintptr_t>(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (m_checkSocket == kNoSocket || !MakeNonBlocking(m_checkSocket))
    {
        spdlog::error("Network check skipped: no socket ({})", LastError());
        return;
    }

    const auto cSocket = static_cast<decltype(socket(0, 0, 0))>(m_checkSocket);
    for (const in_addr& cAddress : cAddresses)
    {
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_addr = cAddress;
        to.sin_port = htons(LanDiscovery::kPort);
        sendto(cSocket, LanDiscovery::kQuery, static_cast<int>(std::strlen(LanDiscovery::kQuery)), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

        char text[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &cAddress, text, sizeof(text));
        m_checkedAddresses += (m_checkedAddresses.empty() ? "" : ", ") + std::string(text);
    }
    m_checkDeadline = std::chrono::steady_clock::now() + kCheckTime;
    m_checking = true;
}

void LanDiscoveryResponder::PollNetworkCheck() noexcept
{
    if (!m_checking)
        return;

    char buffer[512];
    const auto cRead = recv(static_cast<decltype(socket(0, 0, 0))>(m_checkSocket), buffer, sizeof(buffer), 0);
    const bool cAnswered = cRead > 0 && LanDiscovery::Reply::Parse(std::string(buffer, cRead)).has_value();
    if (!cAnswered && std::chrono::steady_clock::now() < m_checkDeadline)
        return;

    m_checking = false;
    CloseSocket(m_checkSocket);
    m_checkSocket = kNoSocket;

    if (cAnswered)
        spdlog::info("[Host] Network check: players on your network can reach this server ({})", m_checkedAddresses);
    else
        spdlog::warn("[Host] Network check: players on your network can't reach this server ({}). A VPN such as NordVPN, or a firewall on this "
                     "computer, is blocking them: turn the VPN off or allow local network traffic in it, and allow the game through the firewall.",
                     m_checkedAddresses);
}

LanDiscoveryResponder::~LanDiscoveryResponder() noexcept
{
    if (m_open)
        CloseSocket(m_socket);
    if (m_checkSocket != kNoSocket)
        CloseSocket(m_checkSocket);
}

void LanDiscoveryResponder::Poll(const LanDiscovery::Reply& acReply) noexcept
{
    if (!m_open)
        return;

    AnswerSearches(acReply);
    PollNetworkCheck();
}

void LanDiscoveryResponder::AnswerSearches(const LanDiscovery::Reply& acReply) noexcept
{
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
