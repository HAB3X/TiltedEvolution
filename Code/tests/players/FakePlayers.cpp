// Fake players: logs players in to a running server and checks what the server does with their messages (host world
// mode, local network discovery, kicking, required mods, quest sharing...), without the game. Start a server with Gameplay:bHostWorld=true, then the two roles in their own processes (the
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
#include <Messages/AssignCharacterRequest.h>
#include <Messages/AssignCharacterResponse.h>
#include <Messages/AssignObjectsRequest.h>
#include <Messages/AssignObjectsResponse.h>
#include <Messages/ActivateRequest.h>
#include <Messages/AuthenticationRequest.h>
#include <Messages/AuthenticationResponse.h>
#include <Messages/ClientMessageFactory.h>
#include <Messages/ClientReferencesMoveRequest.h>
#include <Messages/EnterExteriorCellRequest.h>
#include <Messages/EnterInteriorCellRequest.h>
#include <Messages/HostWorldStateRequest.h>
#include <Messages/NotifyHostWorldRequested.h>
#include <Messages/NotifyHostWorldState.h>
#include <Messages/NotifyChatMessageBroadcast.h>
#include <Messages/NotifyPartyJoined.h>
#include <Messages/NotifyPlayerJoined.h>
#include <Messages/NotifyQuestUpdate.h>
#include <Messages/NotifyPartyTravel.h>
#include <Messages/NotifyQuestDesync.h>
#include <Messages/RequestQuestStages.h>
#include <Messages/PartyCreateRequest.h>
#include <Messages/RequestPartyTravel.h>
#include <Messages/TeleportCommandRequest.h>
#include <Messages/RequestQuestUpdate.h>
#include <Messages/RequestHostWorld.h>
#include <Messages/RequestModeratePlayer.h>
#include <Messages/SendChatMessageRequest.h>
#include <Messages/ServerMessageFactory.h>
#include <Messages/ServerReferencesMoveRequest.h>
#include <Messages/TeleportCommandResponse.h>

#include <WinSock2.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
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
        for (const Plugin& cMod : Plugins)
        {
            Mods::Entry entry;
            entry.Filename = cMod.Name.c_str();
            entry.Id = static_cast<uint16_t>(request.UserMods.ModList.size());
            entry.IsLite = false;
            entry.FileSize = cMod.FileSize;
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
                    (cMod.FileSize != 0 ? RefusedVersions : RefusedMods).push_back(cMod.Filename.c_str());
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
        case kNotifyQuestUpdate: QuestUpdates.push_back(*static_cast<NotifyQuestUpdate*>(pMessage.get())); break;
        case kNotifyPartyTravel: Travel = *static_cast<NotifyPartyTravel*>(pMessage.get()); break;
        case kNotifyQuestDesync: Desyncs.push_back(*static_cast<NotifyQuestDesync*>(pMessage.get())); break;
        case kAssignCharacterResponse:
        {
            const auto* pResponse = static_cast<AssignCharacterResponse*>(pMessage.get());
            AssignedIds[pResponse->Cookie] = pResponse->ServerId;
            break;
        }
        case kAssignObjectsResponse: Objects = *static_cast<AssignObjectsResponse*>(pMessage.get()); break;
        case kServerReferencesMoveRequest:
            MovementBytes += aSize;
            for (const auto& [cId, cMovement] : static_cast<ServerReferencesMoveRequest*>(pMessage.get())->Updates)
                RelayedMovements.push_back(cMovement);
            break;
        default: break;
        }
    }

    std::string m_name, m_password;
    bool LoggedIn = false, Disconnected = false;
    struct Plugin
    {
        std::string Name;
        uint32_t FileSize = 0;
    };
    // The plugins this player says it has loaded
    std::vector<Plugin> Plugins;
    std::optional<AuthenticationResponse::ResponseType> Refusal;
    // In a refusal: the mods to install, and those the player has another version of
    std::vector<std::string> RefusedMods, RefusedVersions;
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
    std::vector<NotifyQuestUpdate> QuestUpdates;
    std::optional<NotifyPartyTravel> Travel;
    std::vector<NotifyQuestDesync> Desyncs;
    // Server ids of the characters this player assigned, by cookie
    std::map<uint32_t, uint32_t> AssignedIds;
    // Movement relayed to this player, and the bytes it took
    std::vector<Movement> RelayedMovements;
    size_t MovementBytes = 0;
    std::optional<AssignObjectsResponse> Objects;
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
        without.Plugins = {{"Skyrim.esm"}};
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
        with.Plugins = {{"Skyrim.esm"}, {"SomeMod.esp"}, {"ExtraMod.esp"}};
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

std::string JoinNames(const std::vector<std::string>& acNames)
{
    std::string list;
    for (const auto& cName : acNames)
        list += (list.empty() ? "" : ", ") + cName;
    return list;
}

// A server requiring mods with their sizes (ModPolicy:bRequireMods), whose list is "Sized.esp|12345" and a plain
// "Plain.esp": another size of Sized.esp is refused as a different version, a missing Plain.esp as missing, and the
// right size with any size of Plain.esp gets in. One player at a time, so one process works.
int RunModVersions(const std::string& acEndpoint, const std::string& acPassword)
{
    using RT = AuthenticationResponse::ResponseType;
    auto login = [&](const char* apName, std::vector<FakePlayer::Plugin> aPlugins)
    {
        auto pPlayer = std::make_unique<FakePlayer>(apName, acPassword);
        pPlayer->Plugins = std::move(aPlugins);
        pPlayer->Connect(acEndpoint);
        RunUntil(*pPlayer, [&] { return pPlayer->LoggedIn || pPlayer->Refusal.has_value(); });
        return pPlayer;
    };
    {
        const auto cpOld = login("FakeOldVersion", {{"Skyrim.esm", 250}, {"Sized.esp", 999}, {"Plain.esp", 10}});
        Check("a player with another version of a mod is refused", cpOld->Refusal == RT::kModsMismatch);
        Check("and told that mod is a different version", cpOld->RefusedVersions == std::vector<std::string>{"Sized.esp"} && cpOld->RefusedMods.empty(),
              "versions: " + JoinNames(cpOld->RefusedVersions) + "; missing: " + JoinNames(cpOld->RefusedMods));
    }
    {
        const auto cpMissing = login("FakeMissingPlain", {{"Sized.esp", 12345}});
        Check("a player missing a plain-name mod is refused", cpMissing->Refusal == RT::kModsMismatch);
        Check("and told it's missing, not a different version", cpMissing->RefusedMods == std::vector<std::string>{"Plain.esp"} && cpMissing->RefusedVersions.empty(),
              "versions: " + JoinNames(cpMissing->RefusedVersions) + "; missing: " + JoinNames(cpMissing->RefusedMods));
    }
    {
        const auto cpSame = login("FakeSameVersion", {{"Sized.esp", 12345}, {"Plain.esp", 777}});
        Check("a player with the same size (and any size of the plain-name mod) gets in", cpSame->LoggedIn);
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

// Quest sharing on a server with Gameplay:sSharedQuestTypes=Main,Misc: the leader sends a stage of a Daedric quest
// (not shared), of an untyped quest (shared with Misc) and of a main quest; the member gets the last two only.
const GameId kDaedricQuest(0, 0x2A49A), kUntypedQuest(0, 0x3C3E9), kMainQuest(0, 0x2610C);

int RunQuestLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeQuestLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("quest leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    // The member joins it on its own (Gameplay:bAutoPartyJoin)
    leader.Send(PartyCreateRequest{});
    Check("quest leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    // The member's party join follows its login
    RunUntil(leader, [] { return false; }, 2s);

    for (const auto& [cId, cType] : {std::pair{kDaedricQuest, SharedQuests::kDaedric}, {kUntypedQuest, SharedQuests::kNone}, {kMainQuest, SharedQuests::kMain}})
    {
        RequestQuestUpdate update;
        update.Id = cId;
        update.Stage = 10;
        update.Status = RequestQuestUpdate::StageUpdate;
        update.ClientQuestType = cType;
        leader.Send(update);
    }
    RunUntil(leader, [] { return false; }, 10s);
    return s_failures ? 1 : 0;
}

int RunQuestMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeQuestMember", acPassword);
    member.Connect(acEndpoint);
    Check("quest member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    Check("the settings say Main and Misc are shared", member.Settings.SharedQuestTypes == (SharedQuests::Bit(SharedQuests::kMain) | SharedQuests::Bit(SharedQuests::kMisc)),
          std::to_string(member.Settings.SharedQuestTypes));
    Check("gets the main quest's stage", RunUntil(member, [&] { return !member.QuestUpdates.empty() && member.QuestUpdates.back().Id == kMainQuest; }, 20s));

    std::string received;
    for (const auto& cUpdate : member.QuestUpdates)
        received += (received.empty() ? "" : ", ") + std::to_string(cUpdate.Id.BaseId);
    Check("gets the untyped quest (shared with Misc) and not the Daedric one", member.QuestUpdates.size() == 2 && member.QuestUpdates[0].Id == kUntypedQuest, received);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Travel together: the leader fast travels to Riverwood and the member is asked to follow, and says yes (a teleport
// to the leader); the member's own trip isn't offered to the leader, only the leader leads.
constexpr char kTravelPlace[] = "Riverwood";

int RunTravelLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeTravelLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("travel leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("travel leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 2s);

    RequestPartyTravel travel;
    travel.Place = kTravelPlace;
    leader.Send(travel);
    RunUntil(leader, [] { return false; }, 10s);
    Check("the member's own trip isn't offered to the leader", !leader.Travel.has_value(), leader.Travel ? leader.Travel->LeaderName.c_str() : "");
    return s_failures ? 1 : 0;
}

int RunTravelMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeTravelMember", acPassword);
    member.Connect(acEndpoint);
    Check("travel member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    Check("is asked to travel with the leader", RunUntil(member, [&] { return member.Travel.has_value(); }, 20s));
    Check("the offer names the leader and the place",
          member.Travel && member.Travel->LeaderName == "FakeTravelLeader" && member.Travel->Place == kTravelPlace && member.Party &&
              member.Travel->LeaderId == member.Party->LeaderPlayerId,
          member.Travel ? std::string(member.Travel->LeaderName.c_str()) + " to " + member.Travel->Place.c_str() : "");

    // A member fast travelling on their own
    RequestPartyTravel ownTrip;
    ownTrip.Place = "Whiterun";
    member.Send(ownTrip);

    // Yes: the teleport to the leader the client sends, the server answers with where the leader is
    TeleportCommandRequest follow;
    follow.TargetPlayer = member.Travel ? member.Travel->LeaderName : "";
    member.Send(follow);
    Check("saying yes gets the leader's whereabouts", RunUntil(member, [&] { return member.Teleport.has_value(); }));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Quest desync: the leader reports its shared quests once. The member first reports the same stages three times and
// hears nothing; then three times with one quest at another stage and one the leader doesn't have, and both are told
// once (a difference counts from its second report in a row, and the same one isn't told twice).
// The server lists the quests sorted by id: kDesyncExtra, then kDesyncStage
const GameId kDesyncSame(0, 0x3372B), kDesyncStage(0, 0x46EF2), kDesyncExtra(0, 0x2A49A);

QuestLog LeaderStages()
{
    QuestLog log;
    log.Entries = {{kDesyncSame, 10}, {kDesyncStage, 20}};
    return log;
}

int RunDesyncLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeDesyncLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("desync leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("desync leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));

    RequestQuestStages report;
    report.Stages = LeaderStages();
    leader.Send(report);
    RunUntil(leader, [] { return false; }, 20s);

    Check("the leader is told once which quests differ from the member's",
          leader.Desyncs.size() == 1 && leader.Desyncs[0].OtherPlayer == "FakeDesyncMember" && !leader.Desyncs[0].OtherIsLeader &&
              leader.Desyncs[0].Quests == Vector<GameId>{kDesyncExtra, kDesyncStage},
          std::to_string(leader.Desyncs.size()) + " notices");
    return s_failures ? 1 : 0;
}

int RunDesyncMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeDesyncMember", acPassword);
    member.Connect(acEndpoint);
    Check("desync member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    // The leader reports once it sees this player join
    RunUntil(member, [] { return false; }, 3s);

    auto reportThrice = [&](const QuestLog& acStages)
    {
        for (int i = 0; i < 3; ++i)
        {
            RequestQuestStages report;
            report.Stages = acStages;
            member.Send(report);
            RunUntil(member, [] { return false; }, 1s);
        }
    };

    reportThrice(LeaderStages());
    Check("the same stages: no notice", member.Desyncs.empty(), std::to_string(member.Desyncs.size()) + " notices");

    QuestLog differing;
    differing.Entries = {{kDesyncExtra, 5}, {kDesyncSame, 10}, {kDesyncStage, 30}};
    reportThrice(differing);
    RunUntil(member, [] { return false; }, 2s);
    std::string detail = std::to_string(member.Desyncs.size()) + " notices";
    if (!member.Desyncs.empty())
        detail += ", from " + std::string(member.Desyncs[0].OtherPlayer.c_str()) + " naming " + std::to_string(member.Desyncs[0].Quests.size()) + " quests";
    Check("different stages: told once, naming the leader and the two quests",
          member.Desyncs.size() == 1 && member.Desyncs[0].OtherPlayer == "FakeDesyncLeader" && member.Desyncs[0].OtherIsLeader &&
              member.Desyncs[0].Quests == Vector<GameId>{kDesyncExtra, kDesyncStage},
          detail);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Movement relay rates: an owner moves NPCs at several distances from a watcher, the watcher counts what the server
// relays. The owner's own character is far away too, players are always relayed in full.
const GameId kRelayWorld{0, 0x3C};
const GameId kRelayCell{0, 0xD74};
constexpr float kWatcherX = 100.f;
constexpr float kNearX = 600.f, kMidX = 4100.f, kFarX = 8100.f, kOwnerX = 9500.f, kOutsideX = 21000.f;

// Assigns a character at aX and waits for its server id; the player's own character is 0:14
std::optional<uint32_t> AssignAt(FakePlayer& aPlayer, uint32_t aCookie, GameId aReference, float aX)
{
    AssignCharacterRequest request;
    request.Cookie = aCookie;
    request.ReferenceId = aReference;
    request.CellId = kRelayCell;
    request.WorldSpaceId = kRelayWorld;
    request.Position = glm::vec3(aX, 0.f, 0.f);
    aPlayer.Send(request);
    if (!RunUntil(aPlayer, [&] { return aPlayer.AssignedIds.count(aCookie) != 0; }))
        return std::nullopt;
    return aPlayer.AssignedIds[aCookie];
}

void EnterRelayCell(FakePlayer& aPlayer, float aX)
{
    EnterExteriorCellRequest enter;
    enter.WorldSpaceId = kRelayWorld;
    enter.CellId = kRelayCell;
    enter.CurrentCoords = GridCellCoords::CalculateGridCellCoords(aX, 0.f);
    aPlayer.Send(enter);
}

int RunCrowdOwner(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer owner("FakeCrowdOwner", acPassword);
    owner.Connect(acEndpoint);
    Check("crowd owner logs in", RunUntil(owner, [&] { return owner.LoggedIn; }));
    const auto cSelf = AssignAt(owner, 1, GameId(0, 0x14), kOwnerX);
    Check("crowd owner gets its character", cSelf.has_value());
    EnterRelayCell(owner, kOwnerX);

    const std::vector<std::pair<uint32_t, float>> cActors{{0x2000, kNearX}, {0x2001, kMidX}, {0x2002, kFarX}, {0x2003, kOutsideX}};
    std::map<uint32_t, float> ids;
    if (cSelf)
        ids[*cSelf] = kOwnerX;
    for (const auto& [cBaseId, cX] : cActors)
    {
        if (const auto cId = AssignAt(owner, cBaseId, GameId(0, cBaseId), cX))
            ids[*cId] = cX;
    }
    Check("its NPCs are assigned", ids.size() == cActors.size() + 1, std::to_string(ids.size()) + " characters");
    Check("sees the watcher join", RunUntil(owner, [&] { return !owner.JoinedNames.empty(); }, 30s));
    RunUntil(owner, [] { return false; }, 2s);

    // Every actor walks a little each snapshot, at the 30 ms players and actors in combat are sent at
    uint32_t snapshots = 0;
    const auto cStart = std::chrono::steady_clock::now();
    auto next = cStart;
    while (std::chrono::steady_clock::now() - cStart < 6s)
    {
        owner.Update();
        if (std::chrono::steady_clock::now() >= next)
        {
            next += 30ms;
            ++snapshots;
            ClientReferencesMoveRequest move;
            move.Tick = owner.GetClock().GetCurrentTick();
            for (const auto& [cId, cX] : ids)
            {
                auto& movement = move.Updates[cId];
                movement.CellId = kRelayCell;
                movement.WorldSpaceId = kRelayWorld;
                movement.Position = glm::vec3(cX, static_cast<float>(snapshots % 50) * 4.f, 0.f);
            }
            owner.Send(move);
        }
        std::this_thread::sleep_for(1ms);
    }
    std::printf("sent %u snapshots of %zu characters\n", snapshots, ids.size());
    RunUntil(owner, [] { return false; }, 5s);
    return s_failures ? 1 : 0;
}

int RunCrowdWatcher(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer watcher("FakeCrowdWatcher", acPassword);
    watcher.Connect(acEndpoint);
    Check("crowd watcher logs in", RunUntil(watcher, [&] { return watcher.LoggedIn; }));
    Check("crowd watcher gets its character", AssignAt(watcher, 1, GameId(0, 0x14), kWatcherX).has_value());
    EnterRelayCell(watcher, kWatcherX);
    RunUntil(watcher, [] { return false; }, 12s);

    auto countAt = [&](float aX)
    {
        return std::count_if(watcher.RelayedMovements.begin(), watcher.RelayedMovements.end(), [aX](const Movement& acMovement) { return std::abs(acMovement.Position.x - aX) < 1.f; });
    };
    const auto cOwner = countAt(kOwnerX), cNear = countAt(kNearX), cMid = countAt(kMidX), cFar = countAt(kFarX), cOutside = countAt(kOutsideX);
    const double cFull = static_cast<double>(std::max<ptrdiff_t>(cOwner, 1));
    const std::string cCounts = "player " + std::to_string(cOwner) + ", near " + std::to_string(cNear) + ", mid " + std::to_string(cMid) + ", far " + std::to_string(cFar) +
                                ", outside " + std::to_string(cOutside) + ", " + std::to_string(watcher.MovementBytes) + " bytes";
    std::printf("relayed: %s\n", cCounts.c_str());
    Check("the far player's movement is relayed in full", cOwner >= 150, cCounts);
    Check("a nearby NPC's movement is relayed in full", cNear >= 0.9 * cFull, cCounts);
    Check("a mid-distance NPC's is relayed about every third snapshot", cMid >= 0.2 * cFull && cMid <= 0.45 * cFull, cCounts);
    Check("a far NPC's is relayed about every sixth snapshot", cFar >= 0.08 * cFull && cFar <= 0.25 * cFull, cCounts);
    Check("an NPC out of range isn't relayed", cOutside == 0, cCounts);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Door state: the first player opens a door, a player entering the cell later is told it is open
const GameId kDoorCell{0, 0x16BDF};
const GameId kDoor{0, 0x16C01};

AssignObjectsRequest ClosedDoorIn(const GameId acCell)
{
    AssignObjectsRequest request;
    ObjectData door;
    door.Id = kDoor;
    door.CellId = acCell;
    door.DoorState = ObjectData::kDoorClosed;
    request.Objects.push_back(door);
    return request;
}

void EnterDoorCell(FakePlayer& aPlayer)
{
    EnterInteriorCellRequest enter;
    enter.CellId = kDoorCell;
    aPlayer.Send(enter);
}

int RunDoorFirst(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer first("FakeDoorFirst", acPassword);
    first.Connect(acEndpoint);
    Check("door opener logs in", RunUntil(first, [&] { return first.LoggedIn; }));
    EnterDoorCell(first);
    first.Send(ClosedDoorIn(kDoorCell));
    Check("is the first to see the door", RunUntil(first, [&] { return first.Objects && first.Objects->Objects.size() == 1 && first.Objects->Objects[0].IsSenderFirst; }));

    ActivateRequest open;
    open.Id = kDoor;
    open.CellId = kDoorCell;
    open.PreActivationOpenState = ObjectData::kDoorClosed;
    first.Send(open);
    RunUntil(first, [] { return false; }, 15s);
    return s_failures ? 1 : 0;
}

int RunDoorLater(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer later("FakeDoorLater", acPassword);
    later.Connect(acEndpoint);
    Check("later player logs in", RunUntil(later, [&] { return later.LoggedIn; }));
    EnterDoorCell(later);
    later.Send(ClosedDoorIn(kDoorCell));
    Check("is told about the door", RunUntil(later, [&] { return later.Objects && later.Objects->Objects.size() == 1; }));
    const auto* pDoor = later.Objects && !later.Objects->Objects.empty() ? &later.Objects->Objects[0] : nullptr;
    Check("the door is open, as the first player left it", pDoor && !pDoor->IsSenderFirst && pDoor->DoorState == ObjectData::kDoorOpen,
          pDoor ? "state " + std::to_string(pDoor->DoorState) : "");
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// FakePlayers.exe host|join [address:port] [password]: start the host first, then the joiner (this build's server)
// FakePlayers.exe soak [address:port] [password]: two at once, for SOAK_SECONDS
// FakePlayers.exe kicker|kicked [address:port] [password]: start the kicker first
// FakePlayers.exe parent <server folder>: starts the server as the game would, and exits after 15 seconds
// FakePlayers.exe mods [address:port] [password]: against a server requiring SomeMod.esp
// FakePlayers.exe modversions [address:port] [password]: against a server requiring Sized.esp|12345 and Plain.esp
// FakePlayers.exe listen184|talk184 [address:port] [password]: the same against an official v1.8.4 server
// FakePlayers.exe questleader|questmember [address:port] [password]: start the leader first (sSharedQuestTypes=Main,Misc)
// FakePlayers.exe travelleader|travelmember [address:port] [password]: start the leader first
// FakePlayers.exe desyncleader|desyncmember [address:port] [password]: start the leader first
// FakePlayers.exe crowdowner|crowdwatcher [address:port] [password]: start the owner first
// FakePlayers.exe doorfirst|doorlater [address:port] [password]: start the first one first
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
    if (cRole == "modversions")
        return RunModVersions(cEndpoint, cPassword);
    if (cRole == "listen184")
        return RunOfficialListener(cEndpoint, cPassword);
    if (cRole == "talk184")
        return RunOfficialTalker(cEndpoint, cPassword);
    if (cRole == "questleader")
        return RunQuestLeader(cEndpoint, cPassword);
    if (cRole == "questmember")
        return RunQuestMember(cEndpoint, cPassword);
    if (cRole == "travelleader")
        return RunTravelLeader(cEndpoint, cPassword);
    if (cRole == "travelmember")
        return RunTravelMember(cEndpoint, cPassword);
    if (cRole == "desyncleader")
        return RunDesyncLeader(cEndpoint, cPassword);
    if (cRole == "desyncmember")
        return RunDesyncMember(cEndpoint, cPassword);
    if (cRole == "crowdowner")
        return RunCrowdOwner(cEndpoint, cPassword);
    if (cRole == "crowdwatcher")
        return RunCrowdWatcher(cEndpoint, cPassword);
    if (cRole == "doorfirst")
        return RunDoorFirst(cEndpoint, cPassword);
    if (cRole == "doorlater")
        return RunDoorLater(cEndpoint, cPassword);

    std::printf("FakePlayers.exe host|join|kicker|kicked|soak|mods|modversions|listen184|talk184|questleader|questmember|travelleader|travelmember|desyncleader|desyncmember|crowdowner|crowdwatcher|doorfirst|doorlater [address:port] [password]\n");
    return 2;
}
