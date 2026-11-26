// SkyrimTogetherRelay: lets friends join a host whose router can't be opened (see encoding/Relay.h). Run it on a
// machine everyone can reach (a VPS, or a computer with these UDP ports forwarded):
//   SkyrimTogetherRelay [--port 10600] [--ports 10601-10699] [--password text] [--max-hosts 16] [--max-peers 16]
// The host sets GameServer:sRelay to this machine's address and port (and sRelayPassword); friends join the address
// the host's invite shows. Plain IPv4 UDP; it logs one line per host and friend that comes and goes.

#include <Relay.h>
#include <UdpSocket.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>

namespace
{
using Clock = std::chrono::steady_clock;
// A host's server sends a keepalive every 5 s
constexpr auto kHostTimeout = std::chrono::seconds(30);
// A game connection sends at least a keepalive every few seconds
constexpr auto kPeerTimeout = std::chrono::seconds(60);

struct Options
{
    uint16_t Port = Relay::kPort;
    uint16_t FirstPort = Relay::kPort + 1;
    uint16_t LastPort = Relay::kPort + 99;
    std::string Password;
    size_t MaxHosts = 16;
    size_t MaxPeers = 16;
};

void Log(const std::string& acLine)
{
    std::printf("%s\n", acLine.c_str());
    std::fflush(stdout);
}

struct Peer
{
    uint32_t Id{};
    Clock::time_point LastSeen;
};

// One host's port and the friends who sent to it
struct Allocation
{
    Udp::Socket Socket;
    uint16_t Port{};
    Clock::time_point LastSeen;
    std::map<Udp::Endpoint, Peer> Peers;
    uint32_t NextPeerId = 1;

    const Udp::Endpoint* FindPeer(uint32_t aId) const
    {
        for (const auto& [cEndpoint, cPeer] : Peers)
        {
            if (cPeer.Id == aId)
                return &cEndpoint;
        }
        return nullptr;
    }
};

class RelayServer
{
public:
    explicit RelayServer(Options aOptions)
        : m_options(std::move(aOptions))
    {
    }

    bool Start()
    {
        if (!m_control.Open(0, m_options.Port))
        {
            Log("Can't open UDP port " + std::to_string(m_options.Port) + " (error " + std::to_string(Udp::Socket::LastError()) + ")");
            return false;
        }
        Log("Relay on UDP port " + std::to_string(m_options.Port) + ", hosts get ports " + std::to_string(m_options.FirstPort) + "-" +
            std::to_string(m_options.LastPort) + (m_options.Password.empty() ? ", no password" : ", with a password"));
        return true;
    }

    void Run()
    {
        char buffer[2048];
        for (;;)
        {
            std::vector<const Udp::Socket*> sockets{&m_control};
            for (const auto& [cHost, cpAllocation] : m_allocations)
                sockets.push_back(&cpAllocation->Socket);
            Udp::Socket::Wait(sockets, 1000);

            Udp::Endpoint from;
            while (const auto cSize = m_control.Receive(buffer, sizeof(buffer), from))
                OnHostMessage(from, buffer, *cSize);

            for (auto& [cHost, pAllocation] : m_allocations)
            {
                while (const auto cSize = pAllocation->Socket.Receive(buffer, sizeof(buffer), from))
                    OnPeerPacket(cHost, *pAllocation, from, buffer, *cSize);
            }

            ForgetQuiet();
        }
    }

private:
    void Send(const Udp::Endpoint& acHost, const Relay::Message& acMessage) const { m_control.SendTo(acHost, acMessage.Serialize()); }

    void Refuse(const Udp::Endpoint& acHost, const std::string& acReason) const
    {
        Relay::Message refused{Relay::Type::Refused};
        refused.Text = acReason;
        Send(acHost, refused);
    }

    void OnHostMessage(const Udp::Endpoint& acHost, const char* apData, size_t aSize)
    {
        const auto cMessage = Relay::Message::Parse(apData, aSize);
        if (!cMessage)
            return;

        const auto cIt = m_allocations.find(acHost);
        switch (cMessage->Kind)
        {
        case Relay::Type::Allocate:
        {
            if (cMessage->Text != m_options.Password)
            {
                Log(acHost.ToString() + " gave a wrong password");
                Refuse(acHost, "wrong relay password");
                return;
            }
            if (cIt == m_allocations.end() && !Allocate(acHost))
                return;
            Allocation& allocation = *m_allocations.at(acHost);
            allocation.LastSeen = Clock::now();
            Relay::Message allocated{Relay::Type::Allocated};
            allocated.Port = allocation.Port;
            Send(acHost, allocated);
            return;
        }
        case Relay::Type::Keepalive:
            if (cIt == m_allocations.end())
            {
                Refuse(acHost, "no port for this host, ask again");
                return;
            }
            cIt->second->LastSeen = Clock::now();
            Send(acHost, Relay::Message{Relay::Type::KeepaliveAck});
            return;
        case Relay::Type::Data:
        {
            if (cIt == m_allocations.end())
                return;
            Allocation& allocation = *cIt->second;
            allocation.LastSeen = Clock::now();
            if (const Udp::Endpoint* pPeer = allocation.FindPeer(cMessage->PeerId))
                allocation.Socket.SendTo(*pPeer, cMessage->Payload);
            return;
        }
        default: return;
        }
    }

    bool Allocate(const Udp::Endpoint& acHost)
    {
        if (m_allocations.size() >= m_options.MaxHosts)
        {
            Log("Refusing " + acHost.ToString() + ": already relaying for " + std::to_string(m_allocations.size()) + " hosts");
            Refuse(acHost, "the relay is full");
            return false;
        }
        auto pAllocation = std::make_unique<Allocation>();
        for (uint32_t port = m_options.FirstPort; port <= m_options.LastPort; ++port)
        {
            if (IsInUse(static_cast<uint16_t>(port)) || !pAllocation->Socket.Open(0, static_cast<uint16_t>(port)))
                continue;
            pAllocation->Port = static_cast<uint16_t>(port);
            break;
        }
        if (!pAllocation->Port)
        {
            Log("Refusing " + acHost.ToString() + ": no free port");
            Refuse(acHost, "the relay has no free port");
            return false;
        }
        Log("Host " + acHost.ToString() + " gets port " + std::to_string(pAllocation->Port));
        m_allocations[acHost] = std::move(pAllocation);
        return true;
    }

    bool IsInUse(uint16_t aPort) const
    {
        for (const auto& [cHost, cpAllocation] : m_allocations)
        {
            if (cpAllocation->Port == aPort)
                return true;
        }
        return false;
    }

    void OnPeerPacket(const Udp::Endpoint& acHost, Allocation& aAllocation, const Udp::Endpoint& acPeer, const char* apData, size_t aSize)
    {
        if (aSize > Relay::kMaxPayload)
            return;
        auto peerIt = aAllocation.Peers.find(acPeer);
        if (peerIt == aAllocation.Peers.end())
        {
            if (aAllocation.Peers.size() >= m_options.MaxPeers)
                return;
            peerIt = aAllocation.Peers.emplace(acPeer, Peer{aAllocation.NextPeerId++, {}}).first;
            Log(acPeer.ToString() + " joins host " + acHost.ToString() + " on port " + std::to_string(aAllocation.Port));
        }
        peerIt->second.LastSeen = Clock::now();

        Relay::Message data{Relay::Type::Data};
        data.PeerId = peerIt->second.Id;
        data.PeerAddress = acPeer.Address;
        data.PeerPort = acPeer.Port;
        data.Payload.assign(apData, aSize);
        Send(acHost, data);
    }

    void ForgetQuiet()
    {
        const auto cNow = Clock::now();
        for (auto it = m_allocations.begin(); it != m_allocations.end();)
        {
            Allocation& allocation = *it->second;
            for (auto peerIt = allocation.Peers.begin(); peerIt != allocation.Peers.end();)
            {
                if (cNow - peerIt->second.LastSeen < kPeerTimeout)
                {
                    ++peerIt;
                    continue;
                }
                Log(peerIt->first.ToString() + " went quiet, forgetting it");
                Relay::Message closed{Relay::Type::PeerClosed};
                closed.PeerId = peerIt->second.Id;
                Send(it->first, closed);
                peerIt = allocation.Peers.erase(peerIt);
            }

            if (cNow - allocation.LastSeen < kHostTimeout)
            {
                ++it;
                continue;
            }
            Log("Host " + it->first.ToString() + " went quiet, freeing port " + std::to_string(allocation.Port));
            it = m_allocations.erase(it);
        }
    }

    Options m_options;
    Udp::Socket m_control;
    std::map<Udp::Endpoint, std::unique_ptr<Allocation>> m_allocations;
};

// "10601-10699"
bool ParseRange(const char* apText, uint16_t& aFirst, uint16_t& aLast)
{
    char* pEnd = nullptr;
    const unsigned long cFirst = std::strtoul(apText, &pEnd, 10);
    if (*pEnd != '-')
        return false;
    const unsigned long cLast = std::strtoul(pEnd + 1, &pEnd, 10);
    if (*pEnd != '\0' || cFirst == 0 || cLast > 65535 || cFirst > cLast)
        return false;
    aFirst = static_cast<uint16_t>(cFirst);
    aLast = static_cast<uint16_t>(cLast);
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; i += 2)
    {
        const std::string cArg = argv[i];
        const char* pValue = i + 1 < argc ? argv[i + 1] : nullptr;
        bool valid = pValue != nullptr;
        if (valid && cArg == "--port")
        {
            const unsigned long cPort = std::strtoul(pValue, nullptr, 10);
            valid = cPort > 0 && cPort <= 65535;
            options.Port = static_cast<uint16_t>(cPort);
        }
        else if (valid && cArg == "--ports")
            valid = ParseRange(pValue, options.FirstPort, options.LastPort);
        else if (valid && cArg == "--password")
            options.Password = pValue;
        else if (valid && cArg == "--max-hosts")
            options.MaxHosts = std::strtoul(pValue, nullptr, 10);
        else if (valid && cArg == "--max-peers")
            options.MaxPeers = std::strtoul(pValue, nullptr, 10);
        else
            valid = false;

        if (!valid)
        {
            std::printf("SkyrimTogetherRelay [--port 10600] [--ports 10601-10699] [--password text] [--max-hosts 16] [--max-peers 16]\n");
            return 2;
        }
    }

    RelayServer relay(options);
    if (!relay.Start())
        return 1;
    relay.Run();
    return 0;
}
