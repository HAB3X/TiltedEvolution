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
#include <WireProtocol.h>
#include <Messages/AuthenticationRequest.h>
#include <Messages/AuthenticationResponse.h>
#include <Messages/ClientMessageFactory.h>
#include <Messages/HostWorldStateRequest.h>
#include <Messages/NotifyHostWorldRequested.h>
#include <Messages/NotifyHostWorldState.h>
#include <Messages/NotifyChatMessageBroadcast.h>
#include <Messages/NotifyPartyJoined.h>
#include <Messages/NotifyPlayerJoined.h>
#include <Messages/RequestHostWorld.h>
#include <Messages/RequestModeratePlayer.h>
#include <Messages/SendChatMessageRequest.h>
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
        request.Version = Wire::IsOfficial184() ? Wire::kOfficial184Version : BUILD_COMMIT;
        request.Token = m_password.c_str();
        request.Username = m_name.c_str();
        request.Level = 1;
        for (const std::string& cMod : PluginNames)
        {
            Mods::Entry entry;
            entry.Filename = cMod.c_str();
            entry.Id = static_cast<uint16_t>(request.UserMods.ModList.size());
            request.UserMods.ModList.push_back(entry);
        }
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
        case kAuthenticationResponse:
        {
            const auto* pResponse = static_cast<AuthenticationResponse*>(pMessage.get());
            LoggedIn = pResponse->Type == AuthenticationResponse::ResponseType::kAccepted;
            Refusal = pResponse->Type;
            for (const auto& cMod : pResponse->UserMods.ModList)
            {
                if (!LoggedIn)
                    RefusedMods.push_back(cMod.Filename.c_str());
            }
            Settings = pResponse->Settings;
            ServerVersion = pResponse->Version.c_str();
            break;
        }
        case kNotifyPlayerJoined:
            JoinedNames.push_back(static_cast<NotifyPlayerJoined*>(pMessage.get())->Username.c_str());
            JoinedIds.push_back(static_cast<NotifyPlayerJoined*>(pMessage.get())->PlayerId);
            break;
        case kNotifyChatMessageBroadcast:
            Chat = *static_cast<NotifyChatMessageBroadcast*>(pMessage.get());
            if (Chat->PlayerName.c_str() == m_name)
                ++OwnChatsBack;
            break;
        case kNotifyPartyJoined: Party = *static_cast<NotifyPartyJoined*>(pMessage.get()); break;
        case kNotifyHostWorldRequested: WorldRequest = *static_cast<NotifyHostWorldRequested*>(pMessage.get()); break;
        case kNotifyHostWorldState: World = *static_cast<NotifyHostWorldState*>(pMessage.get()); break;
        case kTeleportCommandResponse: Teleport = *static_cast<TeleportCommandResponse*>(pMessage.get()); break;
        default: break;
        }
    }

    std::string m_name, m_password;
    bool LoggedIn = false, Disconnected = false;
    // The plugins this player says it has loaded
    std::vector<std::string> PluginNames;
    std::optional<AuthenticationResponse::ResponseType> Refusal;
    std::vector<std::string> RefusedMods;
    ServerSettings Settings{};
    std::string ServerVersion;
    std::vector<std::string> JoinedNames;
    std::vector<uint32_t> JoinedIds;
    std::optional<NotifyChatMessageBroadcast> Chat;
    uint32_t OwnChatsBack = 0;
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

// An official v1.8.4 server (official, or our mac release's): this build speaking its protocol. The listener waits for
// the talker to join and chat, which crosses the message numbers official v1.8.4 has shifted, both ways.
constexpr char kOfficialChat[] = "hello in v1.8.4";

int RunOfficialListener(const std::string& acEndpoint, const std::string& acPassword)
{
    Wire::Set(WireProtocol::Official184);
    FakePlayer listener("FakeListener", acPassword);
    listener.Connect(acEndpoint);
    Check("logs in to the official server as v1.8.4", RunUntil(listener, [&] { return listener.LoggedIn; }), listener.ServerVersion);
    Check("reads its settings (console allowed, no voice or host world)", listener.Settings.ConsoleCommandsAllowed && !listener.Settings.VoipEnabled && !listener.Settings.HostWorld);
    Check("sees the talker join", RunUntil(listener, [&] { return !listener.JoinedNames.empty(); }, 30s), listener.JoinedNames.empty() ? "" : listener.JoinedNames.back());
    Check("gets the talker's chat", RunUntil(listener, [&] { return listener.Chat.has_value(); }, 15s));
    Check("chat arrives intact", listener.Chat && listener.Chat->ChatMessage == kOfficialChat && listener.Chat->PlayerName == "FakeTalker",
          listener.Chat ? std::string(listener.Chat->PlayerName.c_str()) + ": " + listener.Chat->ChatMessage.c_str() : "");
    return s_failures ? 1 : 0;
}

int RunOfficialTalker(const std::string& acEndpoint, const std::string& acPassword)
{
    Wire::Set(WireProtocol::Official184);
    FakePlayer talker("FakeTalker", acPassword);
    talker.Connect(acEndpoint);
    Check("talker logs in as v1.8.4", RunUntil(talker, [&] { return talker.LoggedIn; }));

    SendChatMessageRequest chat;
    chat.MessageType = kGlobalChat;
    chat.ChatMessage = kOfficialChat;
    talker.Send(chat);
    Check("talker gets its own chat back", RunUntil(talker, [&] { return talker.Chat.has_value(); }));
    RunUntil(talker, [] { return false; }, 3s);

    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// A server requiring the host's mods (ModPolicy:bRequireMods) whose list is SomeMod.esp and a Creation Club plugin: a
// player without SomeMod.esp is told it's missing, one with it gets in. One player at a time, so one process works.
int RunRequiredMods(const std::string& acEndpoint, const std::string& acPassword)
{
    {
        FakePlayer without("FakeNoMods", acPassword);
        without.PluginNames = {"Skyrim.esm"};
        without.Connect(acEndpoint);
        Check("a player without the host's mods is refused", RunUntil(without, [&] { return without.Refusal.has_value(); }) &&
                                                                  without.Refusal == AuthenticationResponse::ResponseType::kModsMismatch);
        std::string listed;
        for (const auto& cMod : without.RefusedMods)
            listed += (listed.empty() ? "" : ", ") + cMod;
        Check("and told exactly which mod is missing (not Creation Club content)", without.RefusedMods == std::vector<std::string>{"SomeMod.esp"}, listed);
    }
    {
        FakePlayer with("FakeWithMods", acPassword);
        with.PluginNames = {"Skyrim.esm", "SomeMod.esp", "ExtraMod.esp"};
        with.Connect(acEndpoint);
        Check("a player with them (and an extra mod) gets in", RunUntil(with, [&] { return with.LoggedIn; }));
        // The server also recommends NiceToHave.esp, which this player lacks
        Check("and is told about the recommended mod they lack", RunUntil(with, [&] { return with.Chat.has_value(); }, 5s) &&
                                                                     std::string(with.Chat->ChatMessage.c_str()).find("NiceToHave.esp") != std::string::npos,
              with.Chat ? with.Chat->ChatMessage.c_str() : "no message");
    }
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Like the game hosting: starts the server in a folder as a child that should stop with this process
// (GameServer:uStopWithProcess), lets it run for a while, then exits. The caller checks the server stopped by itself.
int RunParent(const std::string& acServerFolder)
{
    SetEnvironmentVariableA("USTOPWITHPROCESS", std::to_string(GetCurrentProcessId()).c_str());

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    const std::string cLog = acServerFolder + "\\server.log";
    HANDLE log = CreateFileA(cLog.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = log;
    std::string commandLine = "\"" + acServerFolder + "\\SkyrimTogetherServer.exe\"";
    PROCESS_INFORMATION process{};
    Check("starts the server", CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, acServerFolder.c_str(), &startup, &process));
    std::this_thread::sleep_for(15s);
    std::printf("parent exits\n");
    return s_failures ? 1 : 0;
}

// The host kicking from the player list (RequestModeratePlayer): the host waits for a player and kicks them, the player
// checks the server disconnects them. Both play on the server's computer, which is what lets the host kick.
int RunKicker(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer host("FakeKicker", acPassword);
    host.Connect(acEndpoint);
    Check("kicker logs in", RunUntil(host, [&] { return host.LoggedIn; }));
    Check("sees a player join", RunUntil(host, [&] { return !host.JoinedIds.empty(); }, 30s));
    if (!host.JoinedIds.empty())
    {
        RequestModeratePlayer kick;
        kick.PlayerId = host.JoinedIds.back();
        host.Send(kick);
    }
    RunUntil(host, [] { return false; }, 5s);
    return s_failures ? 1 : 0;
}

int RunKicked(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer player("FakeKicked", acPassword);
    player.Connect(acEndpoint);
    Check("player logs in", RunUntil(player, [&] { return player.LoggedIn; }));
    Check("the host's kick disconnects them", RunUntil(player, [&] { return player.Disconnected; }, 15s));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Soak: stays connected for SOAK_SECONDS (default 300), chatting every two seconds, and checks it never drops and the
// server echoes nearly every message. Run two at once so the server has two players to sync.
int RunSoak(const std::string& acEndpoint, const std::string& acPassword)
{
    char seconds[16]{};
    const auto cSeconds = std::chrono::seconds(GetEnvironmentVariableA("SOAK_SECONDS", seconds, sizeof(seconds)) ? std::atoi(seconds) : 300);
    const std::string cName = "FakeSoak" + std::to_string(GetCurrentProcessId());

    FakePlayer player(cName, acPassword);
    player.Connect(acEndpoint);
    Check("soak player logs in", RunUntil(player, [&] { return player.LoggedIn; }));

    uint32_t sent = 0;
    const auto cEnd = std::chrono::steady_clock::now() + cSeconds;
    while (std::chrono::steady_clock::now() < cEnd && !player.Disconnected)
    {
        SendChatMessageRequest chat;
        chat.MessageType = kGlobalChat;
        chat.ChatMessage = ("soak " + std::to_string(sent)).c_str();
        player.Send(chat);
        ++sent;
        RunUntil(player, [&] { return player.Disconnected; }, 2s);
    }
    RunUntil(player, [] { return false; }, 3s);

    Check("stays connected the whole time", !player.Disconnected, std::to_string(cSeconds.count()) + " s");
    Check("the server echoes at least 99% of its chat", player.OwnChatsBack * 100 >= sent * 99, std::to_string(player.OwnChatsBack) + " of " + std::to_string(sent));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// FakePlayers.exe host|join [address:port] [password]: start the host first, then the joiner (this build's server)
// FakePlayers.exe soak [address:port] [password]: two at once, for SOAK_SECONDS
// FakePlayers.exe kicker|kicked [address:port] [password]: start the kicker first
// FakePlayers.exe parent <server folder>: starts the server as the game would, and exits after 15 seconds
// FakePlayers.exe mods [address:port] [password]: against a server requiring SomeMod.esp
// FakePlayers.exe listen184|talk184 [address:port] [password]: the same against an official v1.8.4 server
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
    if (cRole == "soak")
        return RunSoak(cEndpoint, cPassword);
    if (cRole == "kicker")
        return RunKicker(cEndpoint, cPassword);
    if (cRole == "kicked")
        return RunKicked(cEndpoint, cPassword);
    if (cRole == "parent")
        return RunParent(cEndpoint);
    if (cRole == "mods")
        return RunRequiredMods(cEndpoint, cPassword);
    if (cRole == "listen184")
        return RunOfficialListener(cEndpoint, cPassword);
    if (cRole == "talk184")
        return RunOfficialTalker(cEndpoint, cPassword);

    std::printf("FakePlayers.exe host|join|mods|listen184|talk184 [address:port] [password]\n");
    return 2;
}
