// Fake players: logs two players in to a running server and checks what host world mode and local network discovery
// do, without the game. Start a server with Gameplay:bHostWorld=true, then the two roles in their own processes (the
// network library hands every connection's events to the client that updates, so two clients can't share one):
//   FakePlayers.exe host [address:port] [password]   (in the background)
//   FakePlayers.exe join [address:port] [password]
// Each prints one line per check and exits with 0 when all of its checks pass.

#include <chrono>

#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <TiltedCore/ViewBuffer.hpp>
#include <Client.hpp>
#include <Packet.hpp>

#include <BuildInfo.h>
#include <LanDiscovery.h>
#include <Messages/AuthenticationRequest.h>
#include <Messages/AuthenticationResponse.h>
#include <Messages/ClientMessageFactory.h>
#include <Messages/HostWorldStateRequest.h>
#include <Messages/NotifyHostWorldRequested.h>
#include <Messages/NotifyHostWorldState.h>
#include <Messages/NotifyPartyJoined.h>
#include <Messages/RequestHostWorld.h>
#include <Messages/ServerMessageFactory.h>
#include <Messages/TeleportCommandResponse.h>

#include <WinSock2.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <thread>

using namespace TiltedPhoques;
using namespace std::chrono_literals;

namespace
{
int s_failures = 0;

void Check(const char* apName, bool aPassed, const std::string& acDetail = {})
{
    std::printf("%s %s%s%s\n", aPassed ? "PASS" : "FAIL", apName, acDetail.empty() ? "" : ": ", acDetail.c_str());
    if (!aPassed)
        ++s_failures;
}

struct FakePlayer final : Client
{
    explicit FakePlayer(std::string aName, std::string aPassword)
        : m_name(std::move(aName))
        , m_password(std::move(aPassword))
    {
    }

    void Send(const ClientMessage& acMessage)
    {
        Buffer buffer(1 << 16);
        Buffer::Writer writer(&buffer);
        writer.WriteBits(0, 8); // the packet layer's header
        acMessage.Serialize(writer);
        PacketView packet(reinterpret_cast<char*>(buffer.GetWriteData()), static_cast<uint32_t>(writer.Size()));
        Client::Send(&packet);
    }

    void OnConnected() override
    {
        std::printf("%s connected\n", m_name.c_str());
        AuthenticationRequest request;
        request.Version = BUILD_COMMIT;
        request.Token = m_password.c_str();
        request.Username = m_name.c_str();
        request.Level = 1;
        Send(request);
    }

    void OnDisconnected(EDisconnectReason aReason) override
    {
        std::printf("%s disconnected (reason %d)\n", m_name.c_str(), static_cast<int>(aReason));
        Disconnected = true;
    }
    void OnUpdate() override {}

    void OnConsume(const void* apData, uint32_t aSize) override
    {
        ViewBuffer view((uint8_t*)apData, aSize);
        Buffer::Reader reader(&view);
        auto pMessage = ServerMessageFactory().Extract(reader);
        if (!pMessage)
            return;

        switch (pMessage->GetOpcode())
        {
        case kAuthenticationResponse: LoggedIn = static_cast<AuthenticationResponse*>(pMessage.get())->Type == AuthenticationResponse::ResponseType::kAccepted; break;
        case kNotifyPartyJoined: Party = *static_cast<NotifyPartyJoined*>(pMessage.get()); break;
        case kNotifyHostWorldRequested: WorldRequest = *static_cast<NotifyHostWorldRequested*>(pMessage.get()); break;
        case kNotifyHostWorldState: World = *static_cast<NotifyHostWorldState*>(pMessage.get()); break;
        case kTeleportCommandResponse: Teleport = *static_cast<TeleportCommandResponse*>(pMessage.get()); break;
        default: break;
        }
    }

    std::string m_name, m_password;
    bool LoggedIn = false, Disconnected = false;
    std::optional<NotifyPartyJoined> Party;
    std::optional<NotifyHostWorldRequested> WorldRequest;
    std::optional<NotifyHostWorldState> World;
    std::optional<TeleportCommandResponse> Teleport;
};

// Runs the player until the condition holds or the time is up
bool RunUntil(FakePlayer& aPlayer, const std::function<bool()>& acDone, std::chrono::seconds aTime = 10s)
{
    const auto cDeadline = std::chrono::steady_clock::now() + aTime;
    while (std::chrono::steady_clock::now() < cDeadline)
    {
        aPlayer.Update();
        if (acDone())
            return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

// What the fake host says its world is
HostWorldState FakeWorld()
{
    HostWorldState state;
    state.RunningQuests.Entries.push_back({GameId(0, 0x3372B), 10});
    state.CompletedQuests.Entries.push_back({GameId(0, 0x46EF2), 200});
    state.ClearedLocations.push_back(GameId(0, 0x18A5D));
    state.DeadUniqueActors.push_back(GameId(0, 0x1A69C));
    return state;
}

std::optional<LanDiscovery::Reply> AskLocalNetwork()
{
    const SOCKET cSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    to.sin_port = htons(LanDiscovery::kPort);
    sendto(cSocket, LanDiscovery::kQuery, static_cast<int>(std::strlen(LanDiscovery::kQuery)), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));

    DWORD timeoutMs = 2000;
    setsockopt(cSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    char buffer[512];
    const int cRead = recv(cSocket, buffer, sizeof(buffer), 0);
    closesocket(cSocket);
    return cRead > 0 ? LanDiscovery::Reply::Parse(std::string(buffer, cRead)) : std::nullopt;
}
} // namespace

// The host: leads the party and answers the joiner's request with FakeWorld, then stays for the joiner's checks
int RunHost(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer host("FakeHost", acPassword);
    host.Connect(acEndpoint);
    Check("host logs in", RunUntil(host, [&] { return host.LoggedIn && host.Party.has_value(); }));
    Check("host leads the party", host.Party && host.Party->IsLeader);
    Check("host is asked for its world", RunUntil(host, [&] { return host.WorldRequest.has_value(); }, 30s));

    HostWorldStateRequest world;
    world.PlayerId = host.WorldRequest ? host.WorldRequest->PlayerId : 0;
    world.State = FakeWorld();
    host.Send(world);
    RunUntil(host, [] { return false; }, 15s);
    return s_failures ? 1 : 0;
}

// The joiner: run while a host is connected
int RunJoiner(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer joiner("FakeJoiner", acPassword);
    joiner.Connect(acEndpoint);
    Check("joiner logs in to the host's party", RunUntil(joiner, [&] { return joiner.LoggedIn && joiner.Party.has_value(); }));
    Check("joiner doesn't lead", joiner.Party && !joiner.Party->IsLeader && joiner.Party->PlayerIds.size() == 2);

    joiner.Send(RequestHostWorld{});
    Check("joiner gets the host's world", RunUntil(joiner, [&] { return joiner.World.has_value() && joiner.Teleport.has_value(); }));
    Check("the world is the host's, unchanged", joiner.World && joiner.World->State == FakeWorld());
    Check("it names the host", joiner.World && joiner.World->HostName == "FakeHost", joiner.World ? joiner.World->HostName.c_str() : "");

    // A player who isn't the host can't hand anyone a world
    joiner.World.reset();
    HostWorldStateRequest forged;
    forged.PlayerId = joiner.Party ? joiner.Party->PlayerIds.back() : 0;
    forged.State = FakeWorld();
    joiner.Send(forged);
    RunUntil(joiner, [] { return false; }, 3s);
    Check("only the host can send a world", !joiner.World.has_value());

    const auto cLan = AskLocalNetwork();
    Check("server answers a local network search", cLan.has_value());
    Check("it counts both players", cLan && cLan->Players == 2, cLan ? std::to_string(cLan->Players) : "no answer");
    Check("it says host world is on", cLan && cLan->HostWorld);

    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// FakePlayers.exe host|join [address:port] [password]: start the host first, then the joiner
int main(int argc, char** argv)
{
    const std::string cRole = argc > 1 ? argv[1] : "";
    const std::string cEndpoint = argc > 2 ? argv[2] : "127.0.0.1:10578";
    const std::string cPassword = argc > 3 ? argv[3] : "";

    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);

    if (cRole == "host")
        return RunHost(cEndpoint, cPassword);
    if (cRole == "join")
        return RunJoiner(cEndpoint, cPassword);

    std::printf("FakePlayers.exe host|join [address:port] [password]\n");
    return 2;
}
