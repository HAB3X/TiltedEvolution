#include <RelayClient.h>

#include <Relay.h>
#include <UdpSocket.h>

#include <chrono>
#include <memory>

namespace
{
using Clock = std::chrono::steady_clock;
constexpr auto kAllocateEvery = std::chrono::seconds(2);
constexpr auto kKeepaliveEvery = std::chrono::seconds(5);
// No answer to keepalives for this long: the relay restarted or the way to it broke, ask for a port again
constexpr auto kRelayTimeout = std::chrono::seconds(20);
// The relay forgets a friend after 60 s; this side waits a little longer
constexpr auto kPeerTimeout = std::chrono::seconds(90);
constexpr uint32_t kLoopback = 0x7F000001;

struct PeerLink
{
    Udp::Socket Socket;
    std::string PublicAddress;
    Clock::time_point LastSeen;
};
} // namespace

RelayClient::RelayClient(std::string aRelay, std::string aPassword, uint16_t aGamePort) noexcept
    : m_relay(std::move(aRelay))
    , m_password(std::move(aPassword))
    , m_gamePort(aGamePort)
{
}

RelayClient::~RelayClient() noexcept
{
    m_stop = true;
    if (m_thread.joinable())
        m_thread.join();
}

void RelayClient::Start() noexcept
{
    if (m_started.exchange(true))
        return;
    m_thread = std::thread([this]() { Run(); });
}

std::optional<std::string> RelayClient::PublicAddressOf(uint16_t aLoopbackPort) const noexcept
{
    std::scoped_lock lock(m_addressesLock);
    const auto cIt = m_publicAddresses.find(aLoopbackPort);
    return cIt == m_publicAddresses.end() ? std::nullopt : std::optional(cIt->second);
}

void RelayClient::Run() noexcept
{
    const auto cRelay = Udp::Resolve(m_relay, Relay::kPort);
    if (!cRelay)
    {
        spdlog::error("[Host] The relay {} (GameServer:sRelay) isn't an address this computer can find: friends outside your network can't join.", m_relay);
        return;
    }
    Udp::Socket control;
    if (!control.Open())
    {
        spdlog::error("[Host] Couldn't open a network socket for the relay (error {}).", Udp::Socket::LastError());
        return;
    }
    spdlog::info("[Host] Your router can't be opened, so friends will join through the relay {}.", cRelay->ToString());

    const Udp::Endpoint cGame{kLoopback, m_gamePort};
    std::map<uint32_t, std::unique_ptr<PeerLink>> peers;
    uint16_t port = 0;
    Clock::time_point lastAsked{}, lastKeepalive{}, lastHeard = Clock::now();
    int unanswered = 0;
    char buffer[2048];

    auto forget = [&](uint32_t aId)
    {
        const auto cIt = peers.find(aId);
        if (cIt == peers.end())
            return;
        std::scoped_lock lock(m_addressesLock);
        m_publicAddresses.erase(cIt->second->Socket.LocalPort());
        peers.erase(cIt);
    };

    while (!m_stop)
    {
        const auto cNow = Clock::now();
        if (!port && cNow - lastAsked >= kAllocateEvery)
        {
            Relay::Message allocate(Relay::Type::Allocate);
            allocate.Text = m_password;
            control.SendTo(*cRelay, allocate.Serialize());
            lastAsked = cNow;
            if (++unanswered == 5)
                spdlog::warn("[Host] The relay {} doesn't answer: is SkyrimTogetherRelay running there, and its UDP port open?", cRelay->ToString());
        }
        if (port && cNow - lastKeepalive >= kKeepaliveEvery)
        {
            control.SendTo(*cRelay, Relay::Message(Relay::Type::Keepalive).Serialize());
            lastKeepalive = cNow;
        }
        if (port && cNow - lastHeard >= kRelayTimeout)
        {
            spdlog::warn("[Host] Lost the relay {}, asking it for a port again", cRelay->ToString());
            port = 0;
        }

        std::vector<const Udp::Socket*> sockets{&control};
        for (const auto& [cId, cpPeer] : peers)
            sockets.push_back(&cpPeer->Socket);
        Udp::Socket::Wait(sockets, 250);

        Udp::Endpoint from;
        while (const auto cSize = control.Receive(buffer, sizeof(buffer), from))
        {
            const auto cMessage = from == *cRelay ? Relay::Message::Parse(buffer, *cSize) : std::nullopt;
            if (!cMessage)
                continue;
            lastHeard = Clock::now();
            switch (cMessage->Kind)
            {
            case Relay::Type::Allocated:
                if (port != cMessage->Port)
                {
                    port = cMessage->Port;
                    unanswered = 0;
                    // LocalServerService reads this line for the invite
                    spdlog::info("[Host] Public address: {}:{} (through the relay)", cRelay->AddressString(), port);
                }
                break;
            case Relay::Type::Refused:
                if (port || cMessage->Text.rfind("no port", 0) == 0)
                {
                    port = 0;
                    break;
                }
                spdlog::error("[Host] The relay {} refused: {}.", cRelay->ToString(), cMessage->Text);
                return;
            case Relay::Type::Data:
            {
                auto& pPeer = peers[cMessage->PeerId];
                if (!pPeer)
                {
                    pPeer = std::make_unique<PeerLink>();
                    if (!pPeer->Socket.Open(kLoopback, 0))
                    {
                        spdlog::error("Relay: couldn't open a socket for a friend (error {})", Udp::Socket::LastError());
                        peers.erase(cMessage->PeerId);
                        break;
                    }
                    pPeer->PublicAddress = Udp::Endpoint{cMessage->PeerAddress, 0}.AddressString();
                    spdlog::info("Relay: {}:{} joins through the relay", pPeer->PublicAddress, cMessage->PeerPort);
                    std::scoped_lock lock(m_addressesLock);
                    m_publicAddresses[pPeer->Socket.LocalPort()] = pPeer->PublicAddress;
                }
                pPeer->LastSeen = Clock::now();
                pPeer->Socket.SendTo(cGame, cMessage->Payload);
                break;
            }
            case Relay::Type::PeerClosed: forget(cMessage->PeerId); break;
            default: break;
            }
        }

        std::vector<uint32_t> quiet;
        for (auto& [cId, pPeer] : peers)
        {
            while (const auto cSize = pPeer->Socket.Receive(buffer, sizeof(buffer), from))
            {
                if (!(from == cGame) || *cSize > Relay::kMaxPayload)
                    continue;
                Relay::Message data(Relay::Type::Data);
                data.PeerId = cId;
                data.Payload.assign(buffer, *cSize);
                control.SendTo(*cRelay, data.Serialize());
                pPeer->LastSeen = Clock::now();
            }
            if (Clock::now() - pPeer->LastSeen > kPeerTimeout)
                quiet.push_back(cId);
        }
        for (const uint32_t cId : quiet)
            forget(cId);
    }
}
