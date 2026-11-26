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
#include <Messages/AssignObjectsRequest.h>
#include <Messages/AssignObjectsResponse.h>
#include <Messages/AuthenticationResponse.h>
#include <Messages/ClientMessageFactory.h>
#include <Messages/ClientReferencesMoveRequest.h>
#include <Messages/EnterExteriorCellRequest.h>
#include <Messages/EnterInteriorCellRequest.h>
#include <Messages/HostWorldStateRequest.h>
#include <Messages/NotifyHostWorldRequested.h>
#include <Messages/NotifyHostWorldState.h>
#include <Messages/NotifyChatMessageBroadcast.h>
#include <Messages/NotifyPartyInfo.h>
#include <Messages/NotifyPartyInvite.h>
#include <Messages/NotifyLootRefused.h>
#include <Messages/NotifyOwnershipTransfer.h>
#include <Messages/RequestCrimeAuthority.h>
#include <Messages/RequestRevive.h>
#include <Messages/NotifyRevive.h>
#include <Messages/RequestDeathStateChange.h>
#include <Messages/RequestTrade.h>
#include <Messages/NotifyTrade.h>
#include <Messages/RequestOwnershipClaim.h>
#include <Messages/CharacterSpawnRequest.h>
#include <Messages/NotifyInventoryChanges.h>
#include <Messages/NotifyPartyJoined.h>
#include <Messages/PartyAcceptInviteRequest.h>
#include <Messages/PartyCreateRequest.h>
#include <Messages/PartyInviteRequest.h>
#include <Messages/NotifyPlayerJoined.h>
#include <Messages/NotifyQuestUpdate.h>
#include <Messages/NotifyPartyTravel.h>
#include <Messages/NotifyQuestDesync.h>
#include <Messages/NotifyPlayerHealthUpdate.h>
#include <Messages/RequestPlayerHealthUpdate.h>
#include <Messages/RequestPartyPing.h>
#include <Messages/NotifyPartyPing.h>
#include <Messages/RequestPartyDoor.h>
#include <Messages/NotifyPartyDoor.h>
#include <Messages/NotifyPartySummon.h>
#include <Messages/NotifyPartyTeleportRefused.h>
#include <Messages/NotifyTeleport.h>
#include <Messages/RequestPartySummon.h>
#include <Messages/RequestPartySummonAnswer.h>
#include <Messages/TeleportRequest.h>
#include <Messages/RequestRespawnAtPartner.h>
#include <PartyTeleport.h>
#include <Messages/RequestQuestStages.h>
#include <Messages/RequestPartyQuestSharing.h>
#include <Messages/NotifyPartyQuestSharing.h>
#include <Messages/RequestQuestShare.h>
#include <Messages/NotifyQuestShareOffer.h>
#include <Messages/RequestQuestShareAnswer.h>
#include <Messages/NotifyQuestCatchUp.h>
#include <Messages/PartyCreateRequest.h>
#include <Messages/RequestPartyTravel.h>
#include <Messages/TeleportCommandRequest.h>
#include <Messages/RequestQuestUpdate.h>
#include <Messages/RequestHostWorld.h>
#include <Messages/RequestInventoryChanges.h>
#include <Messages/RequestModeratePlayer.h>
#include <Messages/NotifyVote.h>
#include <Messages/RequestVote.h>
#include <Messages/RequestVoteAnswer.h>
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
        request.Version = !Version.empty() ? Version.c_str() : Wire::IsOfficial184() ? Wire::kOfficial184Version : BUILD_COMMIT;
        request.Token = m_password.c_str();
        request.Username = m_name.c_str();
        request.Level = 1;
        request.Returning = Returning;
        for (const Plugin& cMod : Plugins)
        {
            Mods::Entry entry;
            entry.Filename = cMod.Name.c_str();
            entry.Id = static_cast<uint16_t>(request.UserMods.ModList.size());
            entry.IsLite = false;
            entry.FileSize = cMod.FileSize;
            entry.Checksum = cMod.Checksum;
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
            PlayerId = pResponse->PlayerId;
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
        case kNotifyPartyInfo: PartyInfo = *static_cast<NotifyPartyInfo*>(pMessage.get()); break;
        case kNotifyPartyInvite: Invite = *static_cast<NotifyPartyInvite*>(pMessage.get()); break;
        case kNotifyHostWorldRequested: WorldRequest = *static_cast<NotifyHostWorldRequested*>(pMessage.get()); break;
        case kNotifyHostWorldState: World = *static_cast<NotifyHostWorldState*>(pMessage.get()); break;
        case kTeleportCommandResponse: Teleport = *static_cast<TeleportCommandResponse*>(pMessage.get()); break;
        case kNotifyQuestUpdate: QuestUpdates.push_back(*static_cast<NotifyQuestUpdate*>(pMessage.get())); break;
        case kNotifyPartyTravel: Travel = *static_cast<NotifyPartyTravel*>(pMessage.get()); break;
        case kNotifyQuestDesync: Desyncs.push_back(*static_cast<NotifyQuestDesync*>(pMessage.get())); break;
        case kNotifyPlayerHealthUpdate: Health = *static_cast<NotifyPlayerHealthUpdate*>(pMessage.get()); break;
        case kNotifyPartyPing: Pings.push_back(*static_cast<NotifyPartyPing*>(pMessage.get())); break;
        case kNotifyPartyDoor: Doors.push_back(*static_cast<NotifyPartyDoor*>(pMessage.get())); break;
        case kNotifyPartySummon: Summons.push_back(*static_cast<NotifyPartySummon*>(pMessage.get())); break;
        case kNotifyPartyTeleportRefused: Refusals.push_back(*static_cast<NotifyPartyTeleportRefused*>(pMessage.get())); break;
        case kNotifyTeleport: ++Teleports; break;
        case kNotifyVote: Votes.push_back(*static_cast<NotifyVote*>(pMessage.get())); break;
        case kAssignObjectsResponse: Objects = *static_cast<AssignObjectsResponse*>(pMessage.get()); break;
        case kAssignCharacterResponse:
        {
            const auto* pResponse = static_cast<AssignCharacterResponse*>(pMessage.get());
            AssignedIds[pResponse->Cookie] = pResponse->ServerId;
            AssignedEpochs[pResponse->Cookie] = pResponse->OwnershipEpoch;
            AssignedInventories[pResponse->Cookie] = pResponse->CurrentInventory;
            break;
        }
        case kNotifyLootRefused: LootRefusals.push_back(*static_cast<NotifyLootRefused*>(pMessage.get())); break;
        case kNotifyOwnershipTransfer: OwnershipTransfers.push_back(*static_cast<NotifyOwnershipTransfer*>(pMessage.get())); break;
        case kNotifyRevive: Revives.push_back(*static_cast<NotifyRevive*>(pMessage.get())); break;
        case kNotifyTrade: Trades.push_back(*static_cast<NotifyTrade*>(pMessage.get())); break;
        case kCharacterSpawnRequest:
        {
            const auto* pSpawn = static_cast<CharacterSpawnRequest*>(pMessage.get());
            Spawns[pSpawn->ServerId] = *pSpawn;
            break;
        }
        case kNotifyInventoryChanges: InventoryChanges.push_back(*static_cast<NotifyInventoryChanges*>(pMessage.get())); break;
        case kServerReferencesMoveRequest:
            MovementBytes += aSize;
            for (const auto& [cId, cMovement] : static_cast<ServerReferencesMoveRequest*>(pMessage.get())->Updates)
                RelayedMovements.push_back(cMovement);
            break;
        case kNotifyPartyQuestSharing: QuestSharing.push_back(*static_cast<NotifyPartyQuestSharing*>(pMessage.get())); break;
        case kNotifyQuestShareOffer: ShareOffers.push_back(*static_cast<NotifyQuestShareOffer*>(pMessage.get())); break;
        case kNotifyQuestCatchUp: CatchUps.push_back(*static_cast<NotifyQuestCatchUp*>(pMessage.get())); break;
        default: break;
        }
    }

    std::string m_name, m_password;
    bool LoggedIn = false, Disconnected = false;
    // Says it comes back after its game stopped (AuthenticationRequest::Returning)
    bool Returning = false;
    // The version it says it has, when not this build's
    std::string Version;
    struct Plugin
    {
        std::string Name;
        uint32_t FileSize = 0;
        uint32_t Checksum = 0;
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
    std::optional<NotifyPartyInfo> PartyInfo;
    std::optional<NotifyPartyInvite> Invite;
    std::optional<NotifyHostWorldRequested> WorldRequest;
    std::optional<NotifyHostWorldState> World;
    std::optional<TeleportCommandResponse> Teleport;
    std::vector<NotifyQuestUpdate> QuestUpdates;
    std::optional<NotifyPartyTravel> Travel;
    std::vector<NotifyQuestDesync> Desyncs;
    std::optional<NotifyPlayerHealthUpdate> Health;
    std::vector<NotifyPartyPing> Pings;
    std::vector<NotifyPartyDoor> Doors;
    std::vector<NotifyPartySummon> Summons;
    std::vector<NotifyPartyTeleportRefused> Refusals;
    uint32_t Teleports = 0;
    uint32_t PlayerId = 0;
    // Every vote state the server sent, in order
    std::vector<NotifyVote> Votes;
    std::optional<AssignObjectsResponse> Objects;

    // The last vote state of this kind and state, if any came since aFrom
    const NotifyVote* FindVote(VoteKind aKind, VoteState aState, size_t aFrom = 0) const
    {
        for (size_t i = Votes.size(); i > aFrom; --i)
        {
            if (Votes[i - 1].Kind == aKind && Votes[i - 1].State == aState)
                return &Votes[i - 1];
        }
        return nullptr;
    }
    // Server ids of the characters this player assigned, by cookie
    std::map<uint32_t, uint32_t> AssignedIds;
    std::map<uint32_t, uint32_t> AssignedEpochs;
    std::map<uint32_t, Inventory> AssignedInventories;
    // Movement relayed to this player, and the bytes it took
    std::vector<Movement> RelayedMovements;
    size_t MovementBytes = 0;
    std::vector<NotifyLootRefused> LootRefusals;
    std::vector<NotifyOwnershipTransfer> OwnershipTransfers;
    std::vector<NotifyRevive> Revives;
    std::vector<NotifyTrade> Trades;

    // The last transfer of this actor to this player, if any came since aFrom
    const NotifyOwnershipTransfer* FindTransfer(uint32_t aServerId, uint32_t aOwnerPlayerId, size_t aFrom = 0) const
    {
        for (size_t i = OwnershipTransfers.size(); i > aFrom; --i)
        {
            if (OwnershipTransfers[i - 1].ServerId == aServerId && OwnershipTransfers[i - 1].OwnerPlayerId == aOwnerPlayerId)
                return &OwnershipTransfers[i - 1];
        }
        return nullptr;
    }
    // Characters the server spawned for this player, by server id
    std::map<uint32_t, CharacterSpawnRequest> Spawns;
    std::vector<NotifyInventoryChanges> InventoryChanges;
    std::vector<NotifyPartyQuestSharing> QuestSharing;
    std::vector<NotifyQuestShareOffer> ShareOffers;
    std::vector<NotifyQuestCatchUp> CatchUps;

    bool HasQuestUpdate(const GameId& acId, uint16_t aStage) const
    {
        return std::any_of(QuestUpdates.begin(), QuestUpdates.end(), [&](const NotifyQuestUpdate& acUpdate) { return acUpdate.Id == acId && acUpdate.Stage == aStage; });
    }
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

// A server requiring "Exact.esp|100|0000abcd" (ModPolicy:bRequireMods): the same size with other contents is refused as
// a different version, the same contents get in, and a player whose game couldn't read the file (checksum 0) is
// compared by size only. One player at a time, so one process works.
int RunModChecksums(const std::string& acEndpoint, const std::string& acPassword)
{
    using RT = AuthenticationResponse::ResponseType;
    auto login = [&](const char* apName, uint32_t aChecksum)
    {
        auto pPlayer = std::make_unique<FakePlayer>(apName, acPassword);
        pPlayer->Plugins = {{"Exact.esp", 100, aChecksum}};
        pPlayer->Connect(acEndpoint);
        RunUntil(*pPlayer, [&] { return pPlayer->LoggedIn || pPlayer->Refusal.has_value(); });
        return pPlayer;
    };
    {
        const auto cpOther = login("FakeOtherContents", 0x1234);
        Check("the same size with other contents is refused", cpOther->Refusal == RT::kModsMismatch);
        Check("as a different version of that mod", cpOther->RefusedVersions == std::vector<std::string>{"Exact.esp"}, "versions: " + JoinNames(cpOther->RefusedVersions));
    }
    Check("the same contents get in", login("FakeSameContents", 0xABCD)->LoggedIn);
    Check("a plugin the game couldn't read is compared by size", login("FakeUnreadable", 0)->LoggedIn);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Votes (VoteService) between three players: the host on the server's computer (votehost, at 127.0.0.1) and two who
// join from another address (votevoter and votetarget, at this computer's network address, so they aren't the host).
// The voter can't kick the host; the host starts a kick vote against the target, the voter agrees and the target is
// kicked; the host can't start another kick vote right away; then a ready check passes when both say ready. Start
// votehost, then votetarget, both in the background, then votevoter.
uint32_t IdOf(const FakePlayer& acPlayer, const std::string& acName)
{
    for (size_t i = 0; i < acPlayer.JoinedNames.size(); ++i)
    {
        if (acPlayer.JoinedNames[i] == acName)
            return acPlayer.JoinedIds[i];
    }
    return 0;
}

int RunVoteHost(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer host("FakeVoteHost", acPassword);
    host.Connect(acEndpoint);
    Check("host logs in", RunUntil(host, [&] { return host.LoggedIn; }));
    Check("sees the voter and the target join", RunUntil(host, [&] { return IdOf(host, "FakeVoter") && IdOf(host, "FakeTarget"); }, 40s));
    // The voter tries to kick the host first
    RunUntil(host, [] { return false; }, 3s);

    RequestVote kick;
    kick.Kind = VoteKind::Kick;
    kick.TargetId = IdOf(host, "FakeTarget");
    host.Send(kick);
    const NotifyVote* pOpen = nullptr;
    Check("a kick vote opens with the host's yes, the target not counted",
          RunUntil(host, [&] { return (pOpen = host.FindVote(VoteKind::Kick, VoteState::Open)) != nullptr; }) && pOpen->Yes == 1 && pOpen->Eligible == 2);
    Check("it passes when the voter agrees", RunUntil(host, [&] { return host.FindVote(VoteKind::Kick, VoteState::Passed) != nullptr; }, 30s));

    const size_t cBefore = host.Votes.size();
    kick.TargetId = IdOf(host, "FakeVoter");
    host.Send(kick);
    Check("the host can't start another kick vote right away",
          RunUntil(host, [&] { return host.FindVote(VoteKind::Kick, VoteState::RefusedCooldown, cBefore) != nullptr; }));

    RequestVote ready;
    ready.Kind = VoteKind::Ready;
    host.Send(ready);
    Check("a ready check passes once everyone is ready", RunUntil(host, [&] { return host.FindVote(VoteKind::Ready, VoteState::Passed) != nullptr; }, 30s));
    RunUntil(host, [] { return false; }, 3s);
    return s_failures ? 1 : 0;
}

int RunVoteTarget(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer target("FakeTarget", acPassword);
    target.Connect(acEndpoint);
    Check("target logs in", RunUntil(target, [&] { return target.LoggedIn; }));
    Check("hears of the vote against it", RunUntil(target, [&] { return !target.Votes.empty() && target.Votes.back().TargetId == target.PlayerId; }, 60s));
    // Its own no doesn't count
    RequestVoteAnswer no;
    no.VoteId = target.Votes.back().VoteId;
    target.Send(no);
    Check("the vote kicks it", RunUntil(target, [&] { return target.Disconnected; }, 30s));
    return s_failures ? 1 : 0;
}

int RunVoteVoter(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer voter("FakeVoter", acPassword);
    voter.Connect(acEndpoint);
    Check("voter logs in", RunUntil(voter, [&] { return voter.LoggedIn; }));

    RequestVote kickHost;
    kickHost.Kind = VoteKind::Kick;
    kickHost.TargetId = IdOf(voter, "FakeVoteHost");
    voter.Send(kickHost);
    Check("can't start a vote against the host", RunUntil(voter, [&] { return voter.FindVote(VoteKind::Kick, VoteState::RefusedHost) != nullptr; }));

    const NotifyVote* pOpen = nullptr;
    Check("sees the kick vote", RunUntil(voter, [&] { return (pOpen = voter.FindVote(VoteKind::Kick, VoteState::Open)) != nullptr; }, 30s));
    RequestVoteAnswer yes;
    yes.VoteId = pOpen ? pOpen->VoteId : 0;
    yes.Yes = true;
    voter.Send(yes);
    const NotifyVote* pPassed = nullptr;
    Check("the vote passes with two yes of two", RunUntil(voter, [&] { return (pPassed = voter.FindVote(VoteKind::Kick, VoteState::Passed)) != nullptr; }) &&
                                                 pPassed->Yes == 2 && pPassed->Eligible == 2);

    Check("sees the ready check", RunUntil(voter, [&] { return (pOpen = voter.FindVote(VoteKind::Ready, VoteState::Open)) != nullptr; }, 30s));
    yes.VoteId = pOpen ? pOpen->VoteId : 0;
    voter.Send(yes);
    Check("everyone is ready", RunUntil(voter, [&] { return voter.FindVote(VoteKind::Ready, VoteState::Passed) != nullptr; }));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// A host world server keeping its world between runs (WorldSaveService): worldstore puts 7 gold in a chest and waits for
// the server's periodic save (run the server with Gameplay:uKeepWorldSeconds=2); after the server restarts in the same
// folder, worldload finds the chest as the server kept it, not as its own empty copy.
ObjectData FakeChest(int32_t aGold)
{
    ObjectData chest;
    chest.Id = GameId(0, 0x812);
    chest.CellId = GameId(0, 0x3C);
    chest.IsContainer = true;
    if (aGold)
    {
        Inventory::Entry gold;
        gold.BaseId = GameId(0, 0xF);
        gold.Count = aGold;
        chest.CurrentInventory.Entries.push_back(gold);
    }
    return chest;
}

int RunWorldStore(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer player("FakeStorer", acPassword);
    player.Connect(acEndpoint);
    Check("logs in to the host world", RunUntil(player, [&] { return player.LoggedIn; }));
    AssignObjectsRequest objects;
    objects.Objects.push_back(FakeChest(7));
    player.Send(objects);
    Check("the server takes the chest as it is", RunUntil(player, [&] { return player.Objects.has_value(); }) && player.Objects->Objects.size() == 1 &&
                                                       player.Objects->Objects[0].IsSenderFirst);
    // The periodic save
    RunUntil(player, [] { return false; }, 6s);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

int RunWorldLoad(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer player("FakeLoader", acPassword);
    player.Connect(acEndpoint);
    Check("logs in to the restarted host world", RunUntil(player, [&] { return player.LoggedIn; }));
    AssignObjectsRequest objects;
    objects.Objects.push_back(FakeChest(0));
    player.Send(objects);
    Check("the chest comes back as the server kept it, with the 7 gold", RunUntil(player, [&] { return player.Objects.has_value(); }) &&
                                                                          player.Objects->Objects.size() == 1 && !player.Objects->Objects[0].IsSenderFirst &&
                                                                          player.Objects->Objects[0].CurrentInventory == FakeChest(7).CurrentInventory);
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

// A lost connection put back (ReturnService), on a server without auto party join. The leader stands in a cell and
// leads a party with the member, then its game "crashes" (the process ends without closing the connection). The member
// sees the party carry on without it; the leader comes back as a restarted game (Returning) and is put back in its
// party, leading it, and moved to where the server last saw it. Start lostleader, then lostmember, both in the
// background; once the server has timed the leader out, run lostreturn.
const GameId kLostCell{0, 0x3C};
const glm::vec3 kLostPosition{100.f, 200.f, 300.f};
constexpr char kLostName[] = "FakeLost";

int RunLostLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader(kLostName, acPassword);
    leader.Connect(acEndpoint);
    Check("leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));

    EnterInteriorCellRequest cell;
    cell.CellId = kLostCell;
    leader.Send(cell);
    AssignCharacterRequest character;
    character.ReferenceId = GameId(0, 0x14);
    character.CellId = kLostCell;
    character.Position = kLostPosition;
    leader.Send(character);

    leader.Send(PartyCreateRequest{});
    Check("leader leads a party", RunUntil(leader, [&] { return leader.Party && leader.Party->IsLeader; }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    PartyInviteRequest invite;
    invite.PlayerId = leader.JoinedIds.empty() ? 0 : leader.JoinedIds.back();
    leader.Send(invite);
    Check("the member joins the party", RunUntil(leader, [&] { return leader.PartyInfo && leader.PartyInfo->PlayerIds.size() == 2; }, 15s));
    std::fflush(stdout);

    // The game crashing: no goodbye to the server, which times the connection out
    TerminateProcess(GetCurrentProcess(), s_failures ? 1 : 0);
    return 1;
}

int RunLostMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeStays", acPassword);
    member.Connect(acEndpoint);
    Check("member logs in", RunUntil(member, [&] { return member.LoggedIn; }));
    Check("is invited", RunUntil(member, [&] { return member.Invite.has_value(); }, 30s));
    PartyAcceptInviteRequest accept;
    accept.InviterId = member.Invite ? member.Invite->InviterId : 0;
    member.Send(accept);
    Check("joins the leader's party", RunUntil(member, [&] { return member.Party && !member.Party->IsLeader; }));
    const uint32_t cLeaderId = member.Party ? member.Party->LeaderPlayerId : 0;

    Check("the party carries on without the lost leader", RunUntil(member, [&] { return member.PartyInfo && member.PartyInfo->PlayerIds.size() == 1 && member.PartyInfo->IsLeader; }, 40s));
    Check("the leader comes back into the party and leads it again",
          RunUntil(member, [&] { return member.PartyInfo && member.PartyInfo->PlayerIds.size() == 2 && !member.PartyInfo->IsLeader && member.PartyInfo->LeaderPlayerId != cLeaderId; }, 60s));
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

int RunLostReturn(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader(kLostName, acPassword);
    leader.Returning = true;
    leader.Connect(acEndpoint);
    Check("the lost leader logs in again", RunUntil(leader, [&] { return leader.LoggedIn; }));
    Check("and is back in its party, leading it", RunUntil(leader, [&] { return leader.Party && leader.Party->IsLeader && leader.Party->PlayerIds.size() == 2; }));
    Check("and is moved back to where the server last saw it", RunUntil(leader, [&] { return leader.Teleport.has_value(); }, 5s) && leader.Teleport->CellId == kLostCell &&
              glm::distance(glm::vec3(leader.Teleport->Position), kLostPosition) < 1.f);
    RunUntil(leader, [] { return false; }, 5s);
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
    Check("the leader has nothing to repair with", !leader.Desyncs.empty() && leader.Desyncs[0].LeaderStages.Entries.empty());
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
    // Only the quest the leader has a stage of can be repaired: the one only the member has isn't there
    QuestLog repairWith;
    repairWith.Entries = {{kDesyncStage, 20}};
    Check("with the leader's stage to repair with", !member.Desyncs.empty() && member.Desyncs[0].LeaderStages == repairWith,
          member.Desyncs.empty() ? "" : std::to_string(member.Desyncs[0].LeaderStages.Entries.size()) + " stages");
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Party quest sharing on a server with Gameplay:sSharedQuestTypes=Main: the party starts with the host's kinds; its
// leader switches it to Daedric only, so the member gets the Daedric stage and not the main one. The leader offers a
// side quest, the member accepts and gets its stage; from then on the two relay that quest to each other, while an
// untyped quest nobody offered isn't relayed. The member, not leading, can't change the party's sharing.
const GameId kSharedSideQuest(0, 0x3B5A6);

RequestQuestUpdate QuestStage(const GameId& acId, uint16_t aStage, uint8_t aType)
{
    RequestQuestUpdate update;
    update.Id = acId;
    update.Stage = aStage;
    update.Status = RequestQuestUpdate::StageUpdate;
    update.ClientQuestType = aType;
    return update;
}

std::string DescribeSharing(const std::vector<NotifyPartyQuestSharing>& acNotices)
{
    std::string text;
    for (const auto& cNotice : acNotices)
        text += (text.empty() ? "" : ", ") + std::string(cNotice.Enabled ? "on " : "off ") + std::to_string(cNotice.Types);
    return text.empty() ? "none" : text;
}

int RunShareLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeShareLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("share leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    const bool cToldSharing = RunUntil(leader, [&] { return leader.QuestSharing.size() == 1; });
    Check("the new party shares the host's kinds", cToldSharing && leader.QuestSharing[0].Enabled && leader.QuestSharing[0].Types == SharedQuests::Bit(SharedQuests::kMain),
          DescribeSharing(leader.QuestSharing));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 2s);

    RequestPartyQuestSharing sharing;
    sharing.Enabled = true;
    sharing.Types = SharedQuests::Bit(SharedQuests::kDaedric);
    leader.Send(sharing);
    const bool cToldChange = RunUntil(leader, [&] { return leader.QuestSharing.size() == 2; });
    Check("the leader switches the party to Daedric quests", cToldChange && leader.QuestSharing[1].Types == SharedQuests::Bit(SharedQuests::kDaedric),
          DescribeSharing(leader.QuestSharing));

    leader.Send(QuestStage(kMainQuest, 10, SharedQuests::kMain));
    leader.Send(QuestStage(kDaedricQuest, 10, SharedQuests::kDaedric));

    RequestQuestShare share;
    share.Id = kSharedSideQuest;
    share.Stage = 20;
    share.ClientQuestType = SharedQuests::kSide;
    leader.Send(share);

    Check("gets the member's stage of the quest they took", RunUntil(leader, [&] { return leader.HasQuestUpdate(kSharedSideQuest, 30); }, 30s));
    leader.Send(QuestStage(kUntypedQuest, 10, SharedQuests::kNone));
    leader.Send(QuestStage(kSharedSideQuest, 50, SharedQuests::kSide));
    RunUntil(leader, [] { return false; }, 5s);
    Check("the member can't change the party's sharing", leader.QuestSharing.size() == 2, DescribeSharing(leader.QuestSharing));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

int RunShareMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeShareMember", acPassword);
    member.Connect(acEndpoint);
    Check("share member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    const bool cToldSharing = RunUntil(member, [&] { return !member.QuestSharing.empty(); });
    Check("is told the party's sharing on joining", cToldSharing && member.QuestSharing[0].Types == SharedQuests::Bit(SharedQuests::kMain),
          DescribeSharing(member.QuestSharing));
    const bool cToldChange =
        RunUntil(member, [&] { return member.QuestSharing.size() == 2 && member.QuestSharing[1].Types == SharedQuests::Bit(SharedQuests::kDaedric); }, 20s);
    Check("is told when the leader switches it to Daedric", cToldChange, DescribeSharing(member.QuestSharing));
    Check("gets the Daedric stage", RunUntil(member, [&] { return member.HasQuestUpdate(kDaedricQuest, 10); }));
    Check("the main quest isn't shared anymore", !member.HasQuestUpdate(kMainQuest, 10));

    const bool cOffered = RunUntil(member, [&] { return !member.ShareOffers.empty(); });
    Check("is offered the leader's side quest", cOffered && member.ShareOffers[0].Id == kSharedSideQuest && member.ShareOffers[0].PlayerName == "FakeShareLeader",
          cOffered ? member.ShareOffers[0].PlayerName.c_str() : "");
    RequestQuestShareAnswer answer;
    answer.Id = kSharedSideQuest;
    answer.Accept = true;
    member.Send(answer);
    Check("accepting puts it on the quest's current stage", RunUntil(member, [&] { return member.HasQuestUpdate(kSharedSideQuest, 20); }));

    RequestPartyQuestSharing sharing;
    sharing.Enabled = false;
    member.Send(sharing);
    member.Send(QuestStage(kSharedSideQuest, 30, SharedQuests::kSide));
    Check("gets the leader's later stage of it", RunUntil(member, [&] { return member.HasQuestUpdate(kSharedSideQuest, 50); }, 20s));
    Check("an untyped quest nobody offered isn't relayed", !member.HasQuestUpdate(kUntypedQuest, 10));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Catching up: the leader reports its quests; a member joining with one of them at another stage and one the leader
// doesn't have is offered the leader's stage of the first, once.
int RunCatchUpLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeCatchUpLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("catch-up leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("catch-up leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    RequestQuestStages report;
    report.Stages = LeaderStages();
    leader.Send(report);
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 10s);
    Check("the leader isn't offered anything", leader.CatchUps.empty());
    return s_failures ? 1 : 0;
}

int RunCatchUpMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeCatchUpMember", acPassword);
    member.Connect(acEndpoint);
    Check("catch-up member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    QuestLog own;
    own.Entries = {{kDesyncExtra, 5}, {kDesyncSame, 10}, {kDesyncStage, 30}};
    for (int i = 0; i < 3; ++i)
    {
        RequestQuestStages report;
        report.Stages = own;
        member.Send(report);
        RunUntil(member, [] { return false; }, 1s);
    }
    std::string detail = std::to_string(member.CatchUps.size()) + " offers";
    if (!member.CatchUps.empty())
        detail += ", from " + std::string(member.CatchUps[0].LeaderName.c_str()) + " with " + std::to_string(member.CatchUps[0].Quests.Entries.size()) + " quests";
    Check("is offered the leader's stage of the quest that differs, once",
          member.CatchUps.size() == 1 && member.CatchUps[0].LeaderName == "FakeCatchUpLeader" &&
              member.CatchUps[0].Quests.Entries == Vector<QuestLog::Entry>{{kDesyncStage, 20}},
          detail);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Party bar: each member's health update carries their magicka, stamina, combat and downed state to the other
RequestPlayerHealthUpdate BarUpdate(float aHealth, PartyVitals aVitals)
{
    RequestPlayerHealthUpdate update;
    update.Percentage = aHealth;
    update.Vitals = aVitals;
    return update;
}

bool IsBarUpdate(const std::optional<NotifyPlayerHealthUpdate>& acHealth, uint32_t aPlayerId, const RequestPlayerHealthUpdate& acSent)
{
    return acHealth && acHealth->PlayerId == aPlayerId && acHealth->Percentage == acSent.Percentage && acHealth->Vitals == acSent.Vitals;
}

std::string DescribeBar(const std::optional<NotifyPlayerHealthUpdate>& acHealth)
{
    if (!acHealth)
        return "nothing";
    return "player " + std::to_string(acHealth->PlayerId) + ": " + std::to_string(acHealth->Percentage) + "% health, " + std::to_string(acHealth->Vitals.Magicka) +
           "% magicka, " + std::to_string(acHealth->Vitals.Stamina) + "% stamina, combat " + std::to_string(acHealth->Vitals.InCombat) + ", downed " +
           std::to_string(acHealth->Vitals.Downed);
}

int RunBarLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeBarLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("bar leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("bar leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 2s);

    leader.Send(BarUpdate(90.f, PartyVitals{.Magicka = 40, .Stamina = 75, .InCombat = true, .Downed = false}));
    const RequestPlayerHealthUpdate cExpected = BarUpdate(12.5f, PartyVitals{.Magicka = 3, .Stamina = 100, .InCombat = true, .Downed = true});
    const bool cGotBar = RunUntil(leader, [&] { return IsBarUpdate(leader.Health, leader.JoinedIds.front(), cExpected); }, 20s);
    Check("gets the member's bar: health, magicka, stamina, in combat and downed", cGotBar, DescribeBar(leader.Health));
    RunUntil(leader, [] { return false; }, 3s);
    return s_failures ? 1 : 0;
}

int RunBarMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeBarMember", acPassword);
    member.Connect(acEndpoint);
    Check("bar member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    const RequestPlayerHealthUpdate cExpected = BarUpdate(90.f, PartyVitals{.Magicka = 40, .Stamina = 75, .InCombat = true, .Downed = false});
    const bool cGotBar = RunUntil(member, [&] { return member.Party && IsBarUpdate(member.Health, member.Party->LeaderPlayerId, cExpected); }, 20s);
    Check("gets the leader's bar", cGotBar, DescribeBar(member.Health));

    member.Send(BarUpdate(12.5f, PartyVitals{.Magicka = 3, .Stamina = 100, .InCombat = true, .Downed = true}));
    RunUntil(member, [] { return false; }, 3s);
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

// Pings: the member pings a target, the leader gets it with the member's name; a player without a party can't ping
RequestPartyPing FakePing()
{
    RequestPartyPing ping;
    ping.WorldSpaceId = GameId(0, 0x3C);
    ping.CellId = GameId(0, 0x9732);
    ping.Position = glm::vec3{1200.f, -340.f, 56.f};
    ping.Target = "Bandit Chief";
    return ping;
}

int RunPingLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakePingLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("ping leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("ping leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));

    const bool cPinged = RunUntil(leader, [&] { return !leader.Pings.empty(); }, 20s);
    const RequestPartyPing cSent = FakePing();
    std::string detail = std::to_string(leader.Pings.size()) + " pings";
    if (!leader.Pings.empty())
        detail += ", the first from " + std::string(leader.Pings[0].PlayerName.c_str()) + " at " + leader.Pings[0].Target.c_str();
    Check("gets the member's ping: who, where and what", cPinged && leader.Pings[0].PlayerId == leader.JoinedIds.front() && leader.Pings[0].PlayerName == "FakePingMember" &&
                                                              leader.Pings[0].WorldSpaceId == cSent.WorldSpaceId && leader.Pings[0].CellId == cSent.CellId &&
                                                              leader.Pings[0].Position == cSent.Position && leader.Pings[0].Target == cSent.Target,
          detail);
    RunUntil(leader, [] { return false; }, 3s);
    return s_failures ? 1 : 0;
}

int RunPingMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakePingMember", acPassword);
    member.Connect(acEndpoint);
    Check("ping member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    member.Send(FakePing());
    RunUntil(member, [] { return false; }, 3s);
    Check("the pinger doesn't get its own ping back", member.Pings.empty(), std::to_string(member.Pings.size()) + " pings");
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

// Party doors: the leader goes through a door and the member is offered to follow; the member's own door is offered
// to the leader only when the server lets any member lead (doormember) and not by default (doorleader / doorfollower)
RequestPartyDoor FakeDoor(const char* apPlace)
{
    RequestPartyDoor door;
    door.CellId = GameId(0, 0x165A7);
    door.Position = glm::vec3{12.f, 340.f, -56.f};
    door.Place = apPlace;
    return door;
}

int RunDoorLeader(const std::string& acEndpoint, const std::string& acPassword, bool aAnyMember)
{
    FakePlayer leader("FakeDoorLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("door leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("door leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 2s);

    leader.Send(FakeDoor("Bleak Falls Barrow"));
    RunUntil(leader, [] { return false; }, 8s);
    std::string detail = std::to_string(leader.Doors.size()) + " doors";
    if (!leader.Doors.empty())
        detail += ", the first from " + std::string(leader.Doors[0].PlayerName.c_str()) + " to " + leader.Doors[0].Place.c_str();
    if (aAnyMember)
        Check("any member may lead: the member's door is offered to the leader",
              leader.Doors.size() == 1 && leader.Doors[0].PlayerName == "FakeDoorFollower" && leader.Doors[0].Place == "Riverwood", detail);
    else
        Check("only the leader leads: the member's door isn't offered", leader.Doors.empty(), detail);
    return s_failures ? 1 : 0;
}

int RunDoorFollower(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeDoorFollower", acPassword);
    member.Connect(acEndpoint);
    Check("door member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    member.Send(FakeDoor("Riverwood"));
    const bool cOffered = RunUntil(member, [&] { return !member.Doors.empty(); }, 20s);
    const RequestPartyDoor cExpected = FakeDoor("Bleak Falls Barrow");
    std::string detail = std::to_string(member.Doors.size()) + " doors";
    if (!member.Doors.empty())
        detail += ", the first from " + std::string(member.Doors[0].PlayerName.c_str()) + " to " + member.Doors[0].Place.c_str();
    Check("is offered to follow the leader through the door, to where it leads",
          cOffered && member.Party && member.Doors[0].PlayerId == member.Party->LeaderPlayerId && member.Doors[0].PlayerName == "FakeDoorLeader" &&
              member.Doors[0].CellId == cExpected.CellId && member.Doors[0].WorldSpaceId == cExpected.WorldSpaceId &&
              member.Doors[0].Position == cExpected.Position && member.Doors[0].Place == cExpected.Place,
          detail);
    RunUntil(member, [] { return false; }, 2s);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Go to and summon (server with Gameplay:uPartyTeleportCooldown=5): the member can't go to the leader in combat, can
// out of it, then not again within the cooldown; the leader's summons is declined, then accepted
std::string DescribeRefusals(const FakePlayer& acPlayer)
{
    std::string text = std::to_string(acPlayer.Refusals.size()) + " refusals";
    for (const auto& cRefusal : acPlayer.Refusals)
        text += " [" + std::to_string(cRefusal.Reason) + " " + cRefusal.PlayerName.c_str() + " " + std::to_string(cRefusal.SecondsLeft) + "s]";
    return text;
}

RequestPlayerHealthUpdate CombatUpdate(bool aInCombat)
{
    return BarUpdate(100.f, PartyVitals{.Magicka = 100, .Stamina = 100, .InCombat = aInCombat, .Downed = false});
}

int RunMoveLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeMoveLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("move leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("move leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    // The member's go-to checks first
    RunUntil(leader, [] { return false; }, 10s);

    RequestPartySummon summon;
    summon.PlayerId = leader.JoinedIds.front();
    leader.Send(summon);
    const bool cDeclined = RunUntil(leader, [&] { return !leader.Refusals.empty(); }, 15s);
    Check("the member declines the first summons", cDeclined && leader.Refusals[0].Reason == PartyTeleport::kDeclined && leader.Refusals[0].PlayerName == "FakeMoveMember",
          DescribeRefusals(leader));

    leader.Send(summon);
    RunUntil(leader, [] { return false; }, 8s);
    Check("the accepted summons isn't refused", leader.Refusals.size() == 1, DescribeRefusals(leader));
    return s_failures ? 1 : 0;
}

int RunMoveMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeMoveMember", acPassword);
    member.Connect(acEndpoint);
    Check("move member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    TeleportRequest goTo;
    goTo.PlayerId = member.Party ? member.Party->LeaderPlayerId : 0;

    member.Send(CombatUpdate(true));
    RunUntil(member, [] { return false; }, 1s);
    member.Send(goTo);
    RunUntil(member, [&] { return !member.Refusals.empty() || member.Teleports; }, 5s);
    Check("can't go to the leader in combat", member.Teleports == 0 && member.Refusals.size() == 1 && member.Refusals[0].Reason == PartyTeleport::kInCombat,
          DescribeRefusals(member));

    member.Send(CombatUpdate(false));
    RunUntil(member, [] { return false; }, 1s);
    member.Send(goTo);
    RunUntil(member, [&] { return member.Refusals.size() > 1 || member.Teleports; }, 5s);
    Check("goes to the leader out of combat", member.Teleports == 1 && member.Refusals.size() == 1, DescribeRefusals(member));

    member.Send(goTo);
    RunUntil(member, [&] { return member.Refusals.size() > 1 || member.Teleports > 1; }, 5s);
    Check("not again within the cooldown, told how long", member.Teleports == 1 && member.Refusals.size() == 2 &&
                                                              member.Refusals[1].Reason == PartyTeleport::kCooldown && member.Refusals[1].SecondsLeft > 0 &&
                                                              member.Refusals[1].SecondsLeft <= 5,
          DescribeRefusals(member));

    TeleportRequest nobody;
    nobody.PlayerId = 999999;
    member.Send(nobody);
    RunUntil(member, [&] { return member.Refusals.size() > 2; }, 5s);
    Check("going to no one is refused", member.Refusals.size() == 3 && member.Refusals[2].Reason == PartyTeleport::kUnavailable, DescribeRefusals(member));

    const bool cSummoned = RunUntil(member, [&] { return !member.Summons.empty(); }, 20s);
    Check("is summoned by the leader", cSummoned && member.Summons[0].PlayerName == "FakeMoveLeader" && member.Party && member.Summons[0].PlayerId == member.Party->LeaderPlayerId);
    RequestPartySummonAnswer answer;
    answer.PlayerId = cSummoned ? member.Summons[0].PlayerId : 0;
    answer.Accept = false;
    member.Send(answer);

    const bool cSummonedAgain = RunUntil(member, [&] { return member.Summons.size() > 1; }, 20s);
    Check("is summoned again", cSummonedAgain);
    answer.Accept = true;
    member.Send(answer);
    RunUntil(member, [&] { return member.Teleports > 1; }, 5s);
    Check("saying yes moves the member to the leader", member.Teleports == 2, std::to_string(member.Teleports) + " teleports");
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Respawning at a partner: fake players have no character to stand next to, so the server leaves the member at the
// game's respawn point (the choice itself is PartyTeleport::RespawnPartner, tested in TPTests)
int RunRespawnLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeRespawnLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("respawn leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("respawn leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 10s);
    return s_failures ? 1 : 0;
}

int RunRespawnMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeRespawnMember", acPassword);
    member.Connect(acEndpoint);
    Check("respawn member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    member.Send(RequestRespawnAtPartner{});
    RunUntil(member, [&] { return member.Teleport.has_value(); }, 5s);
    Check("no partner with a character: stays at the respawn point", !member.Teleport.has_value());
    Check("still connected", !member.Disconnected);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// A player with another Plus release: refused as a wrong version, and told which version the server runs. The server
// also logs it for the host's chat ([Host] line), which the caller checks in its log.
int RunOldVersion(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer player("FakeOldBuild", acPassword);
    player.Version = "v1.8.4-plus.0";
    player.Connect(acEndpoint);
    Check("a player with another Plus release is refused as a wrong version",
          RunUntil(player, [&] { return player.Refusal.has_value(); }) && player.Refusal == AuthenticationResponse::ResponseType::kWrongVersion);
    Check("and told the server's version", player.ServerVersion == BUILD_COMMIT, player.ServerVersion);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Loot race: a chest with one sword, both players take it within a second, the later one is told to give it back
const GameId kLootChest{0, 0x16C02};
const GameId kLootSword{0, 0x12EB7};

// Enters the loot cell and reports the chest; returns its server id
std::optional<uint32_t> SeeLootChest(FakePlayer& aPlayer)
{
    EnterDoorCell(aPlayer);
    AssignObjectsRequest request;
    ObjectData chest;
    chest.Id = kLootChest;
    chest.CellId = kDoorCell;
    chest.IsContainer = true;
    Inventory::Entry sword{};
    sword.BaseId = kLootSword;
    sword.Count = 1;
    chest.CurrentInventory.Entries.push_back(sword);
    request.Objects.push_back(chest);
    aPlayer.Send(request);
    if (!RunUntil(aPlayer, [&] { return aPlayer.Objects && aPlayer.Objects->Objects.size() == 1; }))
        return std::nullopt;
    return aPlayer.Objects->Objects[0].ServerId;
}

void TakeSword(FakePlayer& aPlayer, uint32_t aChestId)
{
    RequestInventoryChanges take;
    take.ServerId = aChestId;
    take.Item.BaseId = kLootSword;
    take.Item.Count = -1;
    aPlayer.Send(take);
}

int RunLootFirst(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer first("FakeLootFirst", acPassword);
    first.Connect(acEndpoint);
    Check("first looter logs in", RunUntil(first, [&] { return first.LoggedIn; }));
    const auto cChest = SeeLootChest(first);
    Check("first looter sees the chest", cChest.has_value());
    Check("sees the second looter join", RunUntil(first, [&] { return !first.JoinedNames.empty(); }, 30s));
    if (cChest)
        TakeSword(first, *cChest);
    RunUntil(first, [] { return false; }, 10s);
    Check("the first to take the sword keeps it", first.LootRefusals.empty(), std::to_string(first.LootRefusals.size()) + " refusals");
    return s_failures ? 1 : 0;
}

int RunLootSecond(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer second("FakeLootSecond", acPassword);
    second.Connect(acEndpoint);
    Check("second looter logs in", RunUntil(second, [&] { return second.LoggedIn; }));
    const auto cChest = SeeLootChest(second);
    Check("second looter sees the chest", cChest.has_value());
    if (cChest)
        TakeSword(second, *cChest);
    Check("the second gives the sword back", RunUntil(second, [&] { return !second.LootRefusals.empty(); }, 5s));
    Check("one sword, from that chest", second.LootRefusals.size() == 1 && second.LootRefusals[0].Item.BaseId == kLootSword && second.LootRefusals[0].Item.Count == 1 &&
                                            cChest && second.LootRefusals[0].ServerId == *cChest);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Per player loot (Gameplay:bPerPlayerLoot=true): both players take the chest's one sword, neither gives it back or
// hears of the other's take, and the chest stays empty for the one who took it
bool ChestHasSword(const FakePlayer& acPlayer)
{
    if (!acPlayer.Objects || acPlayer.Objects->Objects.size() != 1)
        return false;
    for (const Inventory::Entry& cEntry : acPlayer.Objects->Objects[0].CurrentInventory.Entries)
    {
        if (cEntry.BaseId == kLootSword && cEntry.Count > 0)
            return true;
    }
    return false;
}

int RunOwnLootFirst(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer first("FakeOwnLootFirst", acPassword);
    first.Connect(acEndpoint);
    Check("first looter logs in", RunUntil(first, [&] { return first.LoggedIn; }));
    const auto cChest = SeeLootChest(first);
    Check("first looter sees the chest", cChest.has_value());
    Check("sees the second looter join", RunUntil(first, [&] { return !first.JoinedNames.empty(); }, 30s));
    if (cChest)
        TakeSword(first, *cChest);
    RunUntil(first, [] { return false; }, 10s);
    Check("the first keeps their sword", first.LootRefusals.empty(), std::to_string(first.LootRefusals.size()) + " refusals");
    Check("isn't told of the other's take", first.InventoryChanges.empty(), std::to_string(first.InventoryChanges.size()) + " changes");

    first.Objects.reset();
    SeeLootChest(first);
    Check("their chest is empty when they look again", first.Objects && !ChestHasSword(first));
    return s_failures ? 1 : 0;
}

int RunOwnLootSecond(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer second("FakeOwnLootSecond", acPassword);
    second.Connect(acEndpoint);
    Check("second looter logs in", RunUntil(second, [&] { return second.LoggedIn; }));
    RunUntil(second, [] { return false; }, 2s);
    const auto cChest = SeeLootChest(second);
    Check("second looter sees the chest", cChest.has_value());
    Check("with a sword of their own in it", ChestHasSword(second));
    if (cChest)
        TakeSword(second, *cChest);
    RunUntil(second, [] { return false; }, 5s);
    Check("the second keeps their sword too", second.LootRefusals.empty(), std::to_string(second.LootRefusals.size()) + " refusals");
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Loot race over a body: the owner of a dead bandit with one sword takes it, a player next to it takes it as soon as
// the owner's take reaches them, and is told to give it back
constexpr uint32_t kBodyCookie = 0x3000;
constexpr float kBodyX = 700.f;

int RunCorpseOwner(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer owner("FakeCorpseOwner", acPassword);
    owner.Connect(acEndpoint);
    Check("body owner logs in", RunUntil(owner, [&] { return owner.LoggedIn; }));
    Check("body owner gets its character", AssignAt(owner, 1, GameId(0, 0x14), kBodyX).has_value());
    EnterRelayCell(owner, kBodyX);
    Check("sees the looter join", RunUntil(owner, [&] { return !owner.JoinedNames.empty(); }, 30s));
    // The looter enters the cell and is sent the body
    RunUntil(owner, [] { return false; }, 3s);

    AssignCharacterRequest body;
    body.Cookie = kBodyCookie;
    body.ReferenceId = GameId(0, kBodyCookie);
    body.CellId = kRelayCell;
    body.WorldSpaceId = kRelayWorld;
    body.Position = glm::vec3(kBodyX, 0.f, 0.f);
    body.CurrentActorData.IsDead = true;
    Inventory::Entry sword{};
    sword.BaseId = kLootSword;
    sword.Count = 1;
    body.CurrentActorData.InitialInventory.Entries.push_back(sword);
    owner.Send(body);
    Check("the body is assigned", RunUntil(owner, [&] { return owner.AssignedIds.count(kBodyCookie) != 0; }));
    // The looter's game shows the body before the owner loots it
    RunUntil(owner, [] { return false; }, 2s);

    if (owner.AssignedIds.count(kBodyCookie))
    {
        RequestInventoryChanges take;
        take.ServerId = owner.AssignedIds[kBodyCookie];
        take.OwnershipEpoch = owner.AssignedEpochs[kBodyCookie];
        take.Item = sword;
        take.Item.Count = -1;
        take.UpdateClients = true;
        owner.Send(take);
    }
    RunUntil(owner, [] { return false; }, 10s);
    Check("the owner keeps the sword", owner.LootRefusals.empty(), std::to_string(owner.LootRefusals.size()) + " refusals");
    return s_failures ? 1 : 0;
}

int RunCorpseLooter(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer looter("FakeCorpseLooter", acPassword);
    looter.Connect(acEndpoint);
    Check("looter logs in", RunUntil(looter, [&] { return looter.LoggedIn; }));
    Check("looter gets its character", AssignAt(looter, 1, GameId(0, 0x14), kBodyX + 50.f).has_value());
    EnterRelayCell(looter, kBodyX + 50.f);
    const CharacterSpawnRequest* pBody = nullptr;
    auto findBody = [&]
    {
        for (const auto& [cId, cSpawn] : looter.Spawns)
        {
            if (cSpawn.IsDead && !cSpawn.IsPlayer)
                pBody = &cSpawn;
        }
        return pBody != nullptr;
    };
    Check("is sent the dead bandit", RunUntil(looter, findBody, 20s));
    Check("its sword is in the owner's take", RunUntil(looter, [&] { return !looter.InventoryChanges.empty(); }, 10s));
    if (pBody)
    {
        RequestInventoryChanges take;
        take.ServerId = pBody->ServerId;
        take.OwnershipEpoch = pBody->OwnershipEpoch;
        take.Item.BaseId = kLootSword;
        take.Item.Count = -1;
        take.UpdateClients = true;
        looter.Send(take);
    }
    Check("the looter gives the sword back", RunUntil(looter, [&] { return !looter.LootRefusals.empty(); }, 5s));
    Check("one sword, from that body", looter.LootRefusals.size() == 1 && looter.LootRefusals[0].Item.BaseId == kLootSword && looter.LootRefusals[0].Item.Count == 1 &&
                                           pBody && looter.LootRefusals[0].ServerId == pBody->ServerId);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Crime authority: the leader runs a guard; the member, once they have a bounty with the guard's crime faction, gets
// the guard (their game knows the bounty), the leader can't take it back meanwhile, and paying it hands it back
constexpr uint32_t kGuardCookie = 0x3000;
constexpr float kGuardX = 650.f;
const GameId kWhiterunCrime{0, 0x267EA};

int RunCrimeLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeCrimeLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("crime leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("crime leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("crime leader gets its character", AssignAt(leader, 1, GameId(0, 0x14), kGuardX).has_value());
    EnterRelayCell(leader, kGuardX);
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    // The member enters the cell, then is sent the guard
    RunUntil(leader, [] { return false; }, 3s);
    const auto cGuard = AssignAt(leader, kGuardCookie, GameId(0, kGuardCookie), kGuardX + 100.f);
    Check("runs the guard", cGuard.has_value() && leader.Party);
    if (!cGuard || leader.JoinedIds.empty())
        return 1;
    const uint32_t cMember = leader.JoinedIds.front();

    const bool cTaken = RunUntil(leader, [&] { return leader.FindTransfer(*cGuard, cMember) != nullptr; }, 30s);
    Check("the member's bounty takes the guard to the member's game", cTaken);
    if (!cTaken)
        return 1;

    RequestOwnershipClaim claim;
    claim.ServerId = *cGuard;
    claim.ExpectedOwnershipEpoch = leader.FindTransfer(*cGuard, cMember)->OwnershipEpoch;
    const size_t cBeforeClaim = leader.OwnershipTransfers.size();
    leader.Send(claim);
    RunUntil(leader, [] { return false; }, 2s);
    Check("the leader can't claim the guard while the member's bounty lasts", leader.FindTransfer(*cGuard, leader.PlayerId, cBeforeClaim) == nullptr);

    const bool cBack = RunUntil(leader, [&] { return leader.FindTransfer(*cGuard, leader.PlayerId, cBeforeClaim) != nullptr; }, 20s);
    Check("the guard comes back to the leader once the member's bounty is paid", cBack);
    RunUntil(leader, [] { return false; }, 2s);
    return s_failures ? 1 : 0;
}

int RunCrimeMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeCrimeMember", acPassword);
    member.Connect(acEndpoint);
    Check("crime member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    Check("crime member gets its character", AssignAt(member, 1, GameId(0, 0x14), kGuardX + 50.f).has_value());
    EnterRelayCell(member, kGuardX + 50.f);

    const CharacterSpawnRequest* pGuard = nullptr;
    const bool cSeen = RunUntil(
        member,
        [&]
        {
            for (const auto& [cId, cSpawn] : member.Spawns)
            {
                if (!cSpawn.IsPlayer)
                    pGuard = &cSpawn;
            }
            return pGuard != nullptr;
        },
        30s);
    Check("is sent the leader's guard", cSeen);
    if (!cSeen)
        return 1;
    const CharacterSpawnRequest cGuard = *pGuard;

    RequestCrimeAuthority innocent;
    innocent.Faction = kWhiterunCrime;
    innocent.Bounty = 0;
    innocent.Guards.push_back({cGuard.ServerId, cGuard.OwnershipEpoch});
    member.Send(innocent);
    RunUntil(member, [] { return false; }, 2s);
    Check("without a bounty, a bystander doesn't get the guard", member.FindTransfer(cGuard.ServerId, member.PlayerId) == nullptr);

    RequestCrimeAuthority wanted = innocent;
    wanted.Bounty = 40;
    member.Send(wanted);
    const bool cGot = RunUntil(member, [&] { return member.FindTransfer(cGuard.ServerId, member.PlayerId) != nullptr; });
    Check("with a bounty, the guard is run by this player's game", cGot);

    const size_t cAfterGrant = member.OwnershipTransfers.size();
    RunUntil(member, [] { return false; }, 5s);
    Check("keeps the guard while the bounty lasts", cGot && member.OwnershipTransfers.size() == cAfterGrant,
          std::to_string(member.OwnershipTransfers.size() - cAfterGrant) + " transfers since");

    RequestCrimeAuthority paid = innocent;
    paid.Guards.clear();
    member.Send(paid);
    const bool cHandedBack = RunUntil(member, [&] { return member.Party && member.FindTransfer(cGuard.ServerId, member.Party->LeaderPlayerId, cAfterGrant) != nullptr; });
    Check("paying the bounty hands the guard back to the leader", cHandedBack);
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Reviving: the leader goes down and the member revives them; a revive of a player who is up does nothing; with both
// down, the server tells both nobody is left to revive them
RequestPlayerHealthUpdate DownedUpdate(bool aDowned)
{
    return BarUpdate(aDowned ? 0.f : 60.f, PartyVitals{.Magicka = 50, .Stamina = 50, .InCombat = true, .Downed = aDowned});
}

bool HasRevive(const FakePlayer& acPlayer, NotifyRevive::Outcome aOutcome, size_t aFrom = 0)
{
    return std::any_of(acPlayer.Revives.begin() + static_cast<ptrdiff_t>(std::min(aFrom, acPlayer.Revives.size())), acPlayer.Revives.end(),
                       [aOutcome](const NotifyRevive& acRevive) { return acRevive.Result == aOutcome; });
}

int RunReviveLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeReviveLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("revive leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    Check("the server revives in parties by default", leader.Settings.ReviveEnabled);
    leader.Send(PartyCreateRequest{});
    Check("revive leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    Check("revive leader gets its character", AssignAt(leader, 1, GameId(0, 0x14), kGuardX).has_value());
    EnterRelayCell(leader, kGuardX);
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    RunUntil(leader, [] { return false; }, 3s);

    leader.Send(DownedUpdate(true));
    const bool cRevived = RunUntil(leader, [&] { return HasRevive(leader, NotifyRevive::Outcome::kRevived); }, 20s);
    Check("goes down and the member revives them", cRevived && leader.Revives.front().ReviverName == "FakeReviveMember",
          leader.Revives.empty() ? "nothing" : std::string("by ") + leader.Revives.front().ReviverName.c_str());

    leader.Send(DownedUpdate(false));
    const size_t cAfterUp = leader.Revives.size();
    RunUntil(leader, [] { return false; }, 4s);
    Check("a revive while they are up does nothing", leader.Revives.size() == cAfterUp, std::to_string(leader.Revives.size() - cAfterUp) + " notices");

    leader.Send(DownedUpdate(true));
    const bool cPartyDown = RunUntil(leader, [&] { return HasRevive(leader, NotifyRevive::Outcome::kPartyDown, cAfterUp); }, 20s);
    Check("once the member is down too, nobody is left to revive them", cPartyDown);
    RunUntil(leader, [] { return false; }, 2s);
    return s_failures ? 1 : 0;
}

int RunReviveMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeReviveMember", acPassword);
    member.Connect(acEndpoint);
    Check("revive member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    Check("revive member gets its character", AssignAt(member, 1, GameId(0, 0x14), kGuardX + 50.f).has_value());
    EnterRelayCell(member, kGuardX + 50.f);
    member.Send(DownedUpdate(false));

    const auto leaderDowned = [&](bool aDowned) { return member.Party && member.Health && member.Health->PlayerId == member.Party->LeaderPlayerId && member.Health->Vitals.Downed == aDowned; };
    Check("sees the leader go down", RunUntil(member, [&] { return leaderDowned(true); }, 30s));
    RequestRevive revive;
    revive.PlayerId = member.Party ? member.Party->LeaderPlayerId : 0;
    member.Send(revive);

    Check("sees the leader back up", RunUntil(member, [&] { return leaderDowned(false); }, 20s));
    member.Send(revive);

    Check("sees the leader go down again", RunUntil(member, [&] { return leaderDowned(true); }, 20s));
    member.Send(DownedUpdate(true));
    Check("goes down too: nobody is left to revive them", RunUntil(member, [&] { return HasRevive(member, NotifyRevive::Outcome::kPartyDown); }));
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Per player loot on bodies: the owner kills an NPC, its game adds a death item (everyone's), then the owner loots the
// sword and the shield; the looter isn't told of that, takes its own sword at once without giving it back, and its
// copy still has the shield the owner took
constexpr uint32_t kOwnBodyCookie = 0x5000;
const GameId kFireSalts{0, 0x3AD5E};
const GameId kLootShield{0, 0x12EB6};

bool HasInventoryChange(const FakePlayer& acPlayer, uint32_t aServerId, const GameId& acItem)
{
    return std::any_of(acPlayer.InventoryChanges.begin(), acPlayer.InventoryChanges.end(),
                       [&](const NotifyInventoryChanges& acChange) { return acChange.ServerId == aServerId && acChange.Item.BaseId == acItem; });
}

bool InventoryHas(const Inventory& acInventory, const GameId& acItem)
{
    return std::any_of(acInventory.Entries.begin(), acInventory.Entries.end(), [&](const Inventory::Entry& acEntry) { return acEntry.BaseId == acItem && acEntry.Count > 0; });
}

int RunOwnBodyOwner(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer owner("FakeOwnBodyOwner", acPassword);
    owner.Connect(acEndpoint);
    Check("body owner logs in", RunUntil(owner, [&] { return owner.LoggedIn; }));
    Check("body owner gets its character", AssignAt(owner, 1, GameId(0, 0x14), kBodyX).has_value());
    EnterRelayCell(owner, kBodyX);
    Check("sees the looter join", RunUntil(owner, [&] { return !owner.JoinedNames.empty(); }, 30s));
    RunUntil(owner, [] { return false; }, 3s);

    AssignCharacterRequest npc;
    npc.Cookie = kOwnBodyCookie;
    npc.ReferenceId = GameId(0, kOwnBodyCookie);
    npc.CellId = kRelayCell;
    npc.WorldSpaceId = kRelayWorld;
    npc.Position = glm::vec3(kBodyX, 0.f, 0.f);
    Inventory::Entry sword{};
    sword.BaseId = kLootSword;
    sword.Count = 1;
    npc.CurrentActorData.InitialInventory.Entries.push_back(sword);
    Inventory::Entry shield = sword;
    shield.BaseId = kLootShield;
    npc.CurrentActorData.InitialInventory.Entries.push_back(shield);
    owner.Send(npc);
    Check("the NPC is assigned", RunUntil(owner, [&] { return owner.AssignedIds.count(kOwnBodyCookie) != 0; }));
    if (!owner.AssignedIds.count(kOwnBodyCookie))
        return 1;
    const uint32_t cId = owner.AssignedIds[kOwnBodyCookie];
    const uint32_t cEpoch = owner.AssignedEpochs[kOwnBodyCookie];
    // The looter's game shows it first
    RunUntil(owner, [] { return false; }, 2s);

    RequestDeathStateChange death;
    death.Id = cId;
    death.OwnershipEpoch = cEpoch;
    death.IsDead = true;
    owner.Send(death);

    RequestInventoryChanges deathItem;
    deathItem.ServerId = cId;
    deathItem.OwnershipEpoch = cEpoch;
    deathItem.Item.BaseId = kFireSalts;
    deathItem.Item.Count = 1;
    deathItem.UpdateClients = true;
    owner.Send(deathItem);

    RequestInventoryChanges take = deathItem;
    take.Item.BaseId = kLootSword;
    take.Item.Count = -1;
    owner.Send(take);
    take.Item.BaseId = kLootShield;
    owner.Send(take);
    RunUntil(owner, [] { return false; }, 12s);
    Check("the owner keeps its sword", owner.LootRefusals.empty(), std::to_string(owner.LootRefusals.size()) + " refusals");
    return s_failures ? 1 : 0;
}

int RunOwnBodyLooter(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer looter("FakeOwnBodyLooter", acPassword);
    looter.Connect(acEndpoint);
    Check("body looter logs in", RunUntil(looter, [&] { return looter.LoggedIn; }));
    Check("body looter gets its character", AssignAt(looter, 1, GameId(0, 0x14), kBodyX + 50.f).has_value());
    EnterRelayCell(looter, kBodyX + 50.f);

    std::optional<CharacterSpawnRequest> npc;
    Check("is sent the owner's NPC", RunUntil(
                                         looter,
                                         [&]
                                         {
                                             for (const auto& [cId, cSpawn] : looter.Spawns)
                                             {
                                                 if (!cSpawn.IsPlayer)
                                                     npc = cSpawn;
                                             }
                                             return npc.has_value();
                                         },
                                         30s));
    if (!npc)
        return 1;

    Check("the body's death item reaches every player", RunUntil(looter, [&] { return HasInventoryChange(looter, npc->ServerId, kFireSalts); }, 20s));

    // Right away, while the owner's take of the same sword would make it give its back without per player loot
    RequestInventoryChanges take;
    take.ServerId = npc->ServerId;
    take.OwnershipEpoch = npc->OwnershipEpoch;
    take.Item.BaseId = kLootSword;
    take.Item.Count = -1;
    take.UpdateClients = true;
    looter.Send(take);
    RunUntil(looter, [] { return false; }, 3s);
    Check("takes its own sword without giving it back", looter.LootRefusals.empty(), std::to_string(looter.LootRefusals.size()) + " refusals");
    Check("the owner's looting isn't sent to others", !HasInventoryChange(looter, npc->ServerId, kLootSword) && !HasInventoryChange(looter, npc->ServerId, kLootShield));

    AssignCharacterRequest again;
    again.Cookie = kOwnBodyCookie;
    again.ReferenceId = GameId(0, kOwnBodyCookie);
    again.CellId = kRelayCell;
    again.WorldSpaceId = kRelayWorld;
    again.Position = glm::vec3(kBodyX, 0.f, 0.f);
    looter.Send(again);
    const bool cAnswered = RunUntil(looter, [&] { return looter.AssignedInventories.count(kOwnBodyCookie) != 0; });
    const Inventory* pCopy = cAnswered ? &looter.AssignedInventories[kOwnBodyCookie] : nullptr;
    Check("its own copy has the death item and the shield the owner took, not the sword it took", pCopy && InventoryHas(*pCopy, kFireSalts) && InventoryHas(*pCopy, kLootShield) && !InventoryHas(*pCopy, kLootSword),
          pCopy ? std::to_string(pCopy->Entries.size()) + " entries" : "no answer");
    std::printf("%s\n", s_failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return s_failures ? 1 : 0;
}

// Player trading: the leader offers more gold than it has, which is refused once both confirm; then 60 gold for the
// member's sword, both confirm, and the server moves the items as inventory changes of both characters
const GameId kGold{0, 0xF};

std::optional<uint32_t> AssignWith(FakePlayer& aPlayer, const GameId& acItem, int32_t aCount, float aX)
{
    AssignCharacterRequest request;
    request.Cookie = 1;
    request.ReferenceId = GameId(0, 0x14);
    request.CellId = kRelayCell;
    request.WorldSpaceId = kRelayWorld;
    request.Position = glm::vec3(aX, 0.f, 0.f);
    Inventory::Entry item{};
    item.BaseId = acItem;
    item.Count = aCount;
    request.CurrentActorData.InitialInventory.Entries.push_back(item);
    aPlayer.Send(request);
    if (!RunUntil(aPlayer, [&] { return aPlayer.AssignedIds.count(1) != 0; }))
        return std::nullopt;
    return aPlayer.AssignedIds[1];
}

RequestTrade TradeOffer(uint32_t aPartner, const GameId& acItem, int32_t aCount)
{
    RequestTrade request;
    request.PartnerId = aPartner;
    request.Act = RequestTrade::Action::kOffer;
    if (aCount > 0)
    {
        Inventory::Entry item{};
        item.BaseId = acItem;
        item.Count = aCount;
        request.Offer.Entries.push_back(item);
    }
    return request;
}

bool HasTrade(const FakePlayer& acPlayer, NotifyTrade::Event aEvent, size_t aFrom = 0)
{
    return std::any_of(acPlayer.Trades.begin() + static_cast<ptrdiff_t>(std::min(aFrom, acPlayer.Trades.size())), acPlayer.Trades.end(),
                       [aEvent](const NotifyTrade& acTrade) { return acTrade.What == aEvent; });
}

bool GotChange(const FakePlayer& acPlayer, uint32_t aServerId, const GameId& acItem, int32_t aCount)
{
    return std::any_of(acPlayer.InventoryChanges.begin(), acPlayer.InventoryChanges.end(),
                       [&](const NotifyInventoryChanges& acChange) { return acChange.ServerId == aServerId && acChange.Item.BaseId == acItem && acChange.Item.Count == aCount; });
}

int RunTradeLeader(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer leader("FakeTradeLeader", acPassword);
    leader.Connect(acEndpoint);
    Check("trade leader logs in", RunUntil(leader, [&] { return leader.LoggedIn; }));
    leader.Send(PartyCreateRequest{});
    Check("trade leader creates the party", RunUntil(leader, [&] { return leader.Party.has_value(); }));
    const auto cSelf = AssignWith(leader, kGold, 100, kBodyX);
    Check("trade leader gets its character with 100 gold", cSelf.has_value());
    EnterRelayCell(leader, kBodyX);
    Check("sees the member join", RunUntil(leader, [&] { return !leader.JoinedIds.empty(); }, 30s));
    if (!cSelf || leader.JoinedIds.empty())
        return 1;
    const uint32_t cMember = leader.JoinedIds.front();
    RunUntil(leader, [] { return false; }, 3s);

    // Confirmed before the member offers anything: the member's offer has to unconfirm it
    RequestTrade confirm;
    confirm.PartnerId = cMember;
    confirm.Act = RequestTrade::Action::kConfirm;
    leader.Send(TradeOffer(cMember, kGold, 200));
    leader.Send(confirm);
    const auto withSword = [&]() -> const NotifyTrade*
    {
        for (const NotifyTrade& cTrade : leader.Trades)
        {
            if (!cTrade.TheirOffer.Entries.empty())
                return &cTrade;
        }
        return nullptr;
    };
    const bool cSword = RunUntil(leader, [&] { return withSword() != nullptr; }, 20s);
    Check("the member offers its sword", cSword && withSword()->TheirOffer.Entries[0].BaseId == kLootSword);
    Check("the member's offer unconfirms the leader", cSword && !withSword()->MyConfirmed && !withSword()->TheirConfirmed);
    RunUntil(leader, [] { return false; }, 1s);
    leader.Send(confirm);
    Check("200 gold it doesn't have is refused once both confirm", RunUntil(leader, [&] { return HasTrade(leader, NotifyTrade::Event::kMissingItems); }));
    Check("nothing moved", !GotChange(leader, *cSelf, kGold, -200) && !GotChange(leader, *cSelf, kLootSword, 1));

    const size_t cBefore = leader.Trades.size();
    leader.Send(TradeOffer(cMember, kGold, 60));
    Check("its new offer is sent to both", RunUntil(leader, [&] { return leader.Trades.size() > cBefore; }) && leader.Trades[cBefore].MyOffer.Entries.size() == 1 &&
                                                  leader.Trades[cBefore].MyOffer.Entries[0].Count == 60);
    leader.Send(confirm);
    Check("both confirm and the trade completes", RunUntil(leader, [&] { return HasTrade(leader, NotifyTrade::Event::kCompleted, cBefore); }));
    Check("its character gets the sword and gives 60 gold", RunUntil(leader, [&] { return GotChange(leader, *cSelf, kLootSword, 1) && GotChange(leader, *cSelf, kGold, -60); }));
    RunUntil(leader, [] { return false; }, 3s);
    return s_failures ? 1 : 0;
}

int RunTradeMember(const std::string& acEndpoint, const std::string& acPassword)
{
    FakePlayer member("FakeTradeMember", acPassword);
    member.Connect(acEndpoint);
    Check("trade member logs in to the party", RunUntil(member, [&] { return member.LoggedIn && member.Party.has_value(); }));
    const auto cSelf = AssignWith(member, kLootSword, 1, kBodyX + 50.f);
    Check("trade member gets its character with a sword", cSelf.has_value());
    EnterRelayCell(member, kBodyX + 50.f);
    if (!cSelf)
        return 1;

    // Offers its sword when asked, confirms whenever the leader has
    bool offered = false;
    size_t seen = 0;
    const bool cDone = RunUntil(
        member,
        [&]
        {
            for (; seen < member.Trades.size(); ++seen)
            {
                const NotifyTrade& cTrade = member.Trades[seen];
                if (cTrade.What == NotifyTrade::Event::kCompleted)
                    return true;
                if (cTrade.What != NotifyTrade::Event::kUpdated && cTrade.What != NotifyTrade::Event::kMissingItems)
                    continue;
                if (!offered)
                {
                    member.Send(TradeOffer(cTrade.PartnerId, kLootSword, 1));
                    offered = true;
                }
                else if (cTrade.TheirConfirmed && !cTrade.MyConfirmed)
                {
                    RequestTrade confirm;
                    confirm.PartnerId = cTrade.PartnerId;
                    confirm.Act = RequestTrade::Action::kConfirm;
                    member.Send(confirm);
                }
            }
            return false;
        },
        40s);
    Check("the trade completes", cDone);
    Check("its character gets 60 gold and gives the sword", RunUntil(member, [&] { return GotChange(member, *cSelf, kGold, 60) && GotChange(member, *cSelf, kLootSword, -1); }));
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
// FakePlayers.exe barleader|barmember [address:port] [password]: start the leader first
// FakePlayers.exe pingleader|pingmember [address:port] [password]: start the leader first
// FakePlayers.exe doorleader|doorfollower [address:port] [password]: start the leader first; doorleaderany with
//   Gameplay:bPartyDoorsAnyMember=true
// FakePlayers.exe respawnleader|respawnmember [address:port] [password]: start the leader first
// FakePlayers.exe moveleader|movemember [address:port] [password]: start the leader first (uPartyTeleportCooldown=5)
// FakePlayers.exe lostleader|lostmember|lostreturn [address:port] [password]: see RunLostLeader
// FakePlayers.exe oldversion [address:port] [password]: see RunOldVersion
// FakePlayers.exe modchecksums [address:port] [password]: against a server requiring Exact.esp|100|0000abcd
// FakePlayers.exe votehost|votetarget|votevoter [address:port] [password]: see RunVoteHost
// FakePlayers.exe worldstore|worldload [address:port] [password]: see RunWorldStore
// FakePlayers.exe crowdowner|crowdwatcher [address:port] [password]: start the owner first
// FakePlayers.exe doorfirst|doorlater [address:port] [password]: start the first one first
// FakePlayers.exe lootfirst|lootsecond [address:port] [password]: start the first one first
// FakePlayers.exe corpseowner|corpselooter [address:port] [password]: start the owner first
// FakePlayers.exe ownlootfirst|ownlootsecond [address:port] [password]: start the first one first (bPerPlayerLoot=true)
// FakePlayers.exe shareleader|sharemember [address:port] [password]: start the leader first (sSharedQuestTypes=Main)
// FakePlayers.exe catchupleader|catchupmember [address:port] [password]: start the leader first
// FakePlayers.exe crimeleader|crimemember [address:port] [password]: start the leader first
// FakePlayers.exe reviveleader|revivemember [address:port] [password]: start the leader first
// FakePlayers.exe ownbodyowner|ownbodylooter [address:port] [password]: start the owner first (bPerPlayerLoot=true)
// FakePlayers.exe tradeleader|trademember [address:port] [password]: start the leader first
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
    if (cRole == "barleader")
        return RunBarLeader(cEndpoint, cPassword);
    if (cRole == "barmember")
        return RunBarMember(cEndpoint, cPassword);
    if (cRole == "pingleader")
        return RunPingLeader(cEndpoint, cPassword);
    if (cRole == "pingmember")
        return RunPingMember(cEndpoint, cPassword);
    if (cRole == "doorleader")
        return RunDoorLeader(cEndpoint, cPassword, false);
    if (cRole == "doorleaderany")
        return RunDoorLeader(cEndpoint, cPassword, true);
    if (cRole == "doorfollower")
        return RunDoorFollower(cEndpoint, cPassword);
    if (cRole == "respawnleader")
        return RunRespawnLeader(cEndpoint, cPassword);
    if (cRole == "respawnmember")
        return RunRespawnMember(cEndpoint, cPassword);
    if (cRole == "moveleader")
        return RunMoveLeader(cEndpoint, cPassword);
    if (cRole == "movemember")
        return RunMoveMember(cEndpoint, cPassword);
    if (cRole == "worldstore")
        return RunWorldStore(cEndpoint, cPassword);
    if (cRole == "worldload")
        return RunWorldLoad(cEndpoint, cPassword);
    if (cRole == "votehost")
        return RunVoteHost(cEndpoint, cPassword);
    if (cRole == "votetarget")
        return RunVoteTarget(cEndpoint, cPassword);
    if (cRole == "votevoter")
        return RunVoteVoter(cEndpoint, cPassword);
    if (cRole == "modchecksums")
        return RunModChecksums(cEndpoint, cPassword);
    if (cRole == "oldversion")
        return RunOldVersion(cEndpoint, cPassword);
    if (cRole == "lostleader")
        return RunLostLeader(cEndpoint, cPassword);
    if (cRole == "lostmember")
        return RunLostMember(cEndpoint, cPassword);
    if (cRole == "lostreturn")
        return RunLostReturn(cEndpoint, cPassword);
    if (cRole == "crowdowner")
        return RunCrowdOwner(cEndpoint, cPassword);
    if (cRole == "crowdwatcher")
        return RunCrowdWatcher(cEndpoint, cPassword);
    if (cRole == "doorfirst")
        return RunDoorFirst(cEndpoint, cPassword);
    if (cRole == "doorlater")
        return RunDoorLater(cEndpoint, cPassword);
    if (cRole == "lootfirst")
        return RunLootFirst(cEndpoint, cPassword);
    if (cRole == "lootsecond")
        return RunLootSecond(cEndpoint, cPassword);
    if (cRole == "corpseowner")
        return RunCorpseOwner(cEndpoint, cPassword);
    if (cRole == "corpselooter")
        return RunCorpseLooter(cEndpoint, cPassword);
    if (cRole == "ownlootfirst")
        return RunOwnLootFirst(cEndpoint, cPassword);
    if (cRole == "ownlootsecond")
        return RunOwnLootSecond(cEndpoint, cPassword);
    if (cRole == "shareleader")
        return RunShareLeader(cEndpoint, cPassword);
    if (cRole == "sharemember")
        return RunShareMember(cEndpoint, cPassword);
    if (cRole == "catchupleader")
        return RunCatchUpLeader(cEndpoint, cPassword);
    if (cRole == "tradeleader")
        return RunTradeLeader(cEndpoint, cPassword);
    if (cRole == "trademember")
        return RunTradeMember(cEndpoint, cPassword);
    if (cRole == "ownbodyowner")
        return RunOwnBodyOwner(cEndpoint, cPassword);
    if (cRole == "ownbodylooter")
        return RunOwnBodyLooter(cEndpoint, cPassword);
    if (cRole == "reviveleader")
        return RunReviveLeader(cEndpoint, cPassword);
    if (cRole == "revivemember")
        return RunReviveMember(cEndpoint, cPassword);
    if (cRole == "crimeleader")
        return RunCrimeLeader(cEndpoint, cPassword);
    if (cRole == "crimemember")
        return RunCrimeMember(cEndpoint, cPassword);
    if (cRole == "catchupmember")
        return RunCatchUpMember(cEndpoint, cPassword);
    std::printf("FakePlayers.exe host|join|soak|kicker|kicked|parent|mods|modversions|listen184|talk184|questleader|questmember|travelleader|travelmember|desyncleader|desyncmember|barleader|barmember|pingleader|pingmember|doorleader|doorleaderany|doorfollower|respawnleader|respawnmember|moveleader|movemember|worldstore|worldload|votehost|votetarget|votevoter|modchecksums|oldversion|lostleader|lostmember|lostreturn|crowdowner|crowdwatcher|doorfirst|doorlater|lootfirst|lootsecond|shareleader|sharemember|catchupleader|catchupmember|crimeleader|crimemember|reviveleader|revivemember|ownbodyowner|ownbodylooter|tradeleader|trademember|corpseowner|corpselooter|ownlootfirst|ownlootsecond [address:port] [password]\n");
    return 2;
}
