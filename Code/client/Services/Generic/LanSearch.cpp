#include <TiltedOnlinePCH.h>

#include <Services/LanSearch.h>

#include <Services/OverlayService.h>

#include <LanDiscovery.h>
#include <OverlayApp.hpp>
#include <World.h>

#include <iphlpapi.h>

#include <atomic>
#include <thread>

// windows.h already brings in Winsock 1 (winsock.h), which has everything used here

namespace
{
// How long servers have to answer
constexpr int kAnswerMs = 1500;

std::atomic<bool> s_searching{false};

struct FoundServer
{
    std::string Address;
    LanDiscovery::Reply Reply;
};

std::string ToJson(const Vector<FoundServer>& acServers)
{
    std::string json = "[";
    for (const FoundServer& server : acServers)
    {
        if (json.size() > 1)
            json += ',';
        json += fmt::format(R"({{"address":{},"port":{},"name":{},"players":{},"maxPlayers":{},"hasPassword":{},"hostWorld":{}}})", OverlayService::ToJsonString(server.Address), server.Reply.Port,
                            OverlayService::ToJsonString(server.Reply.Name), server.Reply.Players, server.Reply.MaxPlayers, server.Reply.HasPassword, server.Reply.HostWorld);
    }
    return json + "]";
}

// Where to ask: each network's broadcast address (macOS, which Wine runs on, refuses 255.255.255.255), the whole
// network for Windows, and this computer for a server started here
Vector<u_long> Targets()
{
    Vector<u_long> targets;
    ULONG size = 0;
    if (GetAdaptersInfo(nullptr, &size) == ERROR_BUFFER_OVERFLOW)
    {
        Vector<uint8_t> buffer(size);
        auto* pAdapters = reinterpret_cast<IP_ADAPTER_INFO*>(buffer.data());
        if (GetAdaptersInfo(pAdapters, &size) == NO_ERROR)
        {
            for (const IP_ADAPTER_INFO* pAdapter = pAdapters; pAdapter; pAdapter = pAdapter->Next)
            {
                for (const IP_ADDR_STRING* pIp = &pAdapter->IpAddressList; pIp; pIp = pIp->Next)
                {
                    const u_long cAddress = inet_addr(pIp->IpAddress.String);
                    const u_long cMask = inet_addr(pIp->IpMask.String);
                    if (cAddress != 0 && cAddress != INADDR_NONE && cMask != 0)
                        targets.push_back(ntohl(cAddress | ~cMask));
                }
            }
        }
    }
    if (targets.empty())
        spdlog::warn("[LanSearch]: no network addresses, asking the whole network only");

    targets.push_back(INADDR_BROADCAST);
    targets.push_back(INADDR_LOOPBACK);
    return targets;
}

Vector<FoundServer> Search()
{
    Vector<FoundServer> found;

    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    const SOCKET cSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (cSocket == INVALID_SOCKET)
    {
        spdlog::error("[LanSearch]: no socket ({})", WSAGetLastError());
        return found;
    }

    BOOL broadcast = TRUE;
    setsockopt(cSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

    for (const u_long cTarget : Targets())
    {
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = htonl(cTarget);
        to.sin_port = htons(LanDiscovery::kPort);
        if (sendto(cSocket, LanDiscovery::kQuery, static_cast<int>(std::strlen(LanDiscovery::kQuery)), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to)) < 0)
            spdlog::debug("[LanSearch]: couldn't ask {:X} ({})", cTarget, WSAGetLastError());
    }

    const ULONGLONG cDeadline = GetTickCount64() + kAnswerMs;
    for (ULONGLONG now; (now = GetTickCount64()) < cDeadline;)
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(cSocket, &readable);
        const ULONGLONG cLeft = cDeadline - now;
        timeval timeout{static_cast<long>(cLeft / 1000), static_cast<long>(cLeft % 1000 * 1000)};
        if (select(0, &readable, nullptr, nullptr, &timeout) <= 0)
            break;

        char buffer[512];
        sockaddr_in from{};
        int fromLength = sizeof(from);
        const int cRead = recvfrom(cSocket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (cRead <= 0)
            continue;

        auto reply = LanDiscovery::Reply::Parse(std::string(buffer, cRead));
        if (!reply)
            continue;

        // A server here answers both the broadcast and this computer: list it once
        const std::string cAddress = inet_ntoa(from.sin_addr);
        const bool cSeen = std::any_of(found.begin(), found.end(), [&](const FoundServer& s) { return s.Reply.Port == reply->Port && s.Reply.Name == reply->Name; });
        if (!cSeen)
            found.push_back({cAddress, std::move(*reply)});
    }

    closesocket(cSocket);
    return found;
}
} // namespace

void LanSearch::Start() noexcept
{
    if (s_searching.exchange(true))
        return;

    std::thread(
        []()
        {
            const std::string cJson = ToJson(Search());
            spdlog::info("[LanSearch]: found {}", cJson);
            s_searching = false;

            World::Get().GetRunner().Queue(
                [cJson]()
                {
                    auto pArguments = CefListValue::Create();
                    pArguments->SetString(0, cJson);
                    World::Get().GetOverlayService().GetOverlayApp()->ExecuteAsync("lanServers", pArguments);
                });
        })
        .detach();
}
