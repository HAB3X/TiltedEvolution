#include <PortMapper.h>

#include <httplib.h>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <optional>

namespace
{
constexpr char kSsdpAddress[] = "239.255.255.250";
constexpr uint16_t kSsdpPort = 1900;
constexpr int kSearchSeconds = 3;
constexpr char kDescription[] = "Skyrim Together";

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kNoSocket = INVALID_SOCKET;
void CloseSocket(Socket aSocket) { closesocket(aSocket); }
#else
using Socket = int;
constexpr Socket kNoSocket = -1;
void CloseSocket(Socket aSocket) { close(aSocket); }
#endif

// A private (RFC 1918) or carrier-grade NAT (RFC 6598, 100.64.0.0/10) IPv4 address, as a router reports its
// external address when it sits behind another NAT
bool IsBehindAnotherNetwork(const std::string& acAddress)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    char end = 0;
    if (std::sscanf(acAddress.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &end) != 4)
        return false;
    return a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127);
}

std::string Lower(std::string aText)
{
    std::transform(aText.begin(), aText.end(), aText.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return aText;
}

// The value of the first <aTag>…</aTag> at or after aFrom
std::optional<std::string> XmlValue(const std::string& acXml, const std::string& acTag, size_t aFrom = 0)
{
    const std::string cOpen = "<" + acTag + ">", cClose = "</" + acTag + ">";
    const size_t cStart = acXml.find(cOpen, aFrom);
    if (cStart == std::string::npos)
        return std::nullopt;
    const size_t cValue = cStart + cOpen.size();
    const size_t cEnd = acXml.find(cClose, cValue);
    if (cEnd == std::string::npos)
        return std::nullopt;
    return acXml.substr(cValue, cEnd - cValue);
}

struct Url
{
    std::string Host;
    int Port = 80;
    std::string Path = "/";
};

// http://host[:port][/path], the only form routers use
std::optional<Url> ParseUrl(const std::string& acUrl)
{
    constexpr char kScheme[] = "http://";
    if (Lower(acUrl.substr(0, 7)) != kScheme)
        return std::nullopt;
    Url url;
    const size_t cHostStart = 7;
    const size_t cPathStart = acUrl.find('/', cHostStart);
    const std::string cHostPort = acUrl.substr(cHostStart, cPathStart == std::string::npos ? std::string::npos : cPathStart - cHostStart);
    if (cPathStart != std::string::npos)
        url.Path = acUrl.substr(cPathStart);
    const size_t cColon = cHostPort.find(':');
    url.Host = cHostPort.substr(0, cColon);
    if (cColon != std::string::npos)
        url.Port = std::atoi(cHostPort.c_str() + cColon + 1);
    if (url.Host.empty() || url.Port <= 0)
        return std::nullopt;
    return url;
}

// Locations of the gateways that answer an SSDP search
std::vector<std::string> SearchGateways()
{
    std::vector<std::string> locations;
    const Socket cSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (cSocket == kNoSocket)
        return locations;

    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(kSsdpPort);
    inet_pton(AF_INET, kSsdpAddress, &to.sin_addr);
    const std::string cSearch = std::string("M-SEARCH * HTTP/1.1\r\nHOST: ") + kSsdpAddress + ":1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
                                "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    for (int i = 0; i < 2; ++i)
        sendto(cSocket, cSearch.data(), static_cast<int>(cSearch.size()), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    const auto cDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(kSearchSeconds);
    for (auto now = std::chrono::steady_clock::now(); now < cDeadline; now = std::chrono::steady_clock::now())
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(cSocket, &readable);
        const auto cLeft = std::chrono::duration_cast<std::chrono::microseconds>(cDeadline - now).count();
        timeval timeout{static_cast<long>(cLeft / 1000000), static_cast<long>(cLeft % 1000000)};
        if (select(static_cast<int>(cSocket) + 1, &readable, nullptr, nullptr, &timeout) <= 0)
            break;

        char buffer[2048];
        const auto cRead = recv(cSocket, buffer, sizeof(buffer) - 1, 0);
        if (cRead <= 0)
            continue;
        const std::string cAnswer(buffer, cRead);
        const std::string cLower = Lower(cAnswer);
        const size_t cHeader = cLower.find("\nlocation:");
        if (cHeader == std::string::npos)
            continue;
        size_t start = cHeader + 10;
        while (start < cAnswer.size() && cAnswer[start] == ' ')
            ++start;
        const std::string cLocation = cAnswer.substr(start, cAnswer.find_first_of("\r\n", start) - start);
        if (std::find(locations.begin(), locations.end(), cLocation) == locations.end())
            locations.push_back(cLocation);
    }
    CloseSocket(cSocket);
    return locations;
}

// The address of this computer that the router sees
std::string LocalAddressTowards(const std::string& acHost, int aPort)
{
    const Socket cSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (cSocket == kNoSocket)
        return {};
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(static_cast<uint16_t>(aPort));
    std::string address;
    if (inet_pton(AF_INET, acHost.c_str(), &to.sin_addr) == 1 && connect(cSocket, reinterpret_cast<sockaddr*>(&to), sizeof(to)) == 0)
    {
        sockaddr_in local{};
#ifdef _WIN32
        int length = sizeof(local);
#else
        socklen_t length = sizeof(local);
#endif
        char text[INET_ADDRSTRLEN]{};
        if (getsockname(cSocket, reinterpret_cast<sockaddr*>(&local), &length) == 0 && inet_ntop(AF_INET, &local.sin_addr, text, sizeof(text)))
            address = text;
    }
    CloseSocket(cSocket);
    return address;
}
} // namespace

PortMapper::PortMapper(uint16_t aPort, std::function<void()> aOnUnreachable) noexcept
    : m_port(aPort)
{
    m_thread = std::thread(
        [this, onUnreachable = std::move(aOnUnreachable)]()
        {
            if (!Run() && onUnreachable)
                onUnreachable();
        });
}

PortMapper::~PortMapper() noexcept
{
    if (m_thread.joinable())
        m_thread.join();

    if (!m_mapped)
        return;

    std::string response;
    const std::string cArguments = "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(m_port) + "</NewExternalPort><NewProtocol>UDP</NewProtocol>";
    if (Soap(m_router, "DeletePortMapping", cArguments, response))
        spdlog::info("Closed UDP port {} on the router again", m_port);
    else
        spdlog::warn("Couldn't close UDP port {} on the router, it stays open until the router restarts", m_port);
}

bool PortMapper::FindRouter(Router& aRouter, std::string& aError) const noexcept
{
    const auto cLocations = SearchGateways();
    if (cLocations.empty())
    {
        aError = "no router answered (UPnP may be off on it)";
        return false;
    }

    for (const std::string& cLocation : cLocations)
    {
        const auto cUrl = ParseUrl(cLocation);
        if (!cUrl)
            continue;

        httplib::Client client(cUrl->Host, cUrl->Port);
        client.set_connection_timeout(3);
        client.set_read_timeout(3);
        const auto cDescription = client.Get(cUrl->Path);
        if (!cDescription || cDescription->status != 200)
            continue;

        // The router's connection service: WANIPConnection, or WANPPPConnection on DSL
        const std::string& cXml = cDescription->body;
        for (const char* pService : {"WANIPConnection:2", "WANIPConnection:1", "WANPPPConnection:1"})
        {
            const size_t cAt = cXml.find(std::string("urn:schemas-upnp-org:service:") + pService);
            if (cAt == std::string::npos)
                continue;
            const auto cControl = XmlValue(cXml, "controlURL", cAt);
            if (!cControl)
                continue;

            aRouter.ServiceType = std::string("urn:schemas-upnp-org:service:") + pService;
            aRouter.Host = cUrl->Host;
            aRouter.Port = cUrl->Port;
            // Absolute on some routers, relative to the description's host on most
            aRouter.ControlPath = *cControl;
            if (const auto cAbsolute = ParseUrl(*cControl))
            {
                aRouter.Host = cAbsolute->Host;
                aRouter.Port = cAbsolute->Port;
                aRouter.ControlPath = cAbsolute->Path;
            }
            else if (aRouter.ControlPath.empty() || aRouter.ControlPath[0] != '/')
                aRouter.ControlPath = "/" + aRouter.ControlPath;

            aRouter.LocalAddress = LocalAddressTowards(aRouter.Host, aRouter.Port);
            if (aRouter.LocalAddress.empty())
            {
                aError = "couldn't tell this computer's address on the network";
                return false;
            }
            return true;
        }
    }

    aError = "the router doesn't offer port opening";
    return false;
}

bool PortMapper::Soap(const Router& acRouter, const std::string& acAction, const std::string& acArguments, std::string& aResponse) const noexcept
{
    const std::string cBody = "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                              "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" +
                              acAction + " xmlns:u=\"" + acRouter.ServiceType + "\">" + acArguments + "</u:" + acAction + "></s:Body></s:Envelope>";

    httplib::Client client(acRouter.Host, acRouter.Port);
    client.set_connection_timeout(3);
    client.set_read_timeout(3);
    const httplib::Headers cHeaders{{"SOAPAction", "\"" + acRouter.ServiceType + "#" + acAction + "\""}};
    const auto cResult = client.Post(acRouter.ControlPath, cHeaders, cBody, "text/xml; charset=\"utf-8\"");
    if (!cResult)
    {
        aResponse = "no answer";
        return false;
    }
    aResponse = cResult->body;
    return cResult->status == 200;
}

bool PortMapper::Run() noexcept
{
    std::string error;
    if (!FindRouter(m_router, error))
    {
        spdlog::warn("[Host] Router port not opened: {}. Friends outside your network need UDP port {} forwarded to this computer on your router.", error, m_port);
        return false;
    }

    // A day, so the router closes it by itself if the server is killed before it can; then without expiry for the
    // routers that only take permanent mappings
    std::string response;
    bool mapped = false;
    for (const char* pLease : {"86400", "0"})
    {
        const std::string cArguments = "<NewRemoteHost></NewRemoteHost><NewExternalPort>" + std::to_string(m_port) + "</NewExternalPort><NewProtocol>UDP</NewProtocol>"
                                       "<NewInternalPort>" + std::to_string(m_port) + "</NewInternalPort><NewInternalClient>" + m_router.LocalAddress +
                                       "</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>" + kDescription +
                                       "</NewPortMappingDescription><NewLeaseDuration>" + pLease + "</NewLeaseDuration>";
        if ((mapped = Soap(m_router, "AddPortMapping", cArguments, response)))
            break;
    }
    if (!mapped)
    {
        const std::string cCode = XmlValue(response, "errorDescription").value_or(XmlValue(response, "errorCode").value_or(response.substr(0, 80)));
        spdlog::warn("[Host] Router port not opened: the router said \"{}\". Friends outside your network need UDP port {} forwarded to {} on your router.", cCode, m_port,
                     m_router.LocalAddress);
        return false;
    }
    m_mapped = true;

    if (Soap(m_router, "GetExternalIPAddress", "", response))
    {
        if (const auto cAddress = XmlValue(response, "NewExternalIPAddress"); cAddress && !cAddress->empty())
        {
            if (IsBehindAnotherNetwork(*cAddress))
            {
                spdlog::warn("[Host] UDP port {} opened on the router, but the router's own address {} is private: your internet provider or another router "
                             "shares one public address between customers (carrier-grade NAT), so friends outside can't reach it.",
                             m_port, *cAddress);
                return false;
            }
            spdlog::info("[Host] Public address: {}:{} (UDP port {} opened on the router)", *cAddress, m_port, m_port);
            return true;
        }
    }
    spdlog::info("[Host] UDP port {} opened on the router; it didn't say this network's public address", m_port);
    return true;
}
