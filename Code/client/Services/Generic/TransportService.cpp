
#include <Services/TransportService.h>
#include <WireProtocol.h>
#include <Services/OverlayService.h>
#include <Services/SendBuffer.h>

#include <Events/ConnectedEvent.h>
#include <Events/ConnectionErrorEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/UpdateEvent.h>

#include <Games/References.h>
#include <Games/TES.h>
#include <Forms/TESWorldSpace.h>
#include <Forms/TESObjectCELL.h>

#include <TimeManager.h>

#include <Forms/TESNPC.h>
#include <TiltedOnlinePCH.h>
#include <Systems/FrameProfiler.h>
#include <World.h>

#include <Messages/AuthenticationRequest.h>
#include <Messages/ServerMessageFactory.h>
#include <Messages/NotifySettingsChange.h>
#include <Packet.hpp>
#include <steam/isteamnetworkingutils.h>

#include <ScriptExtender.h>
#include <Services/DiscordService.h>
#include <BSGraphics/BSGraphicsRenderer.h>

// #include <imgui_internal.h>

static constexpr wchar_t kMO2DllName[] = L"usvfs_x64.dll";

using TiltedPhoques::Packet;

TransportService::TransportService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&TransportService::HandleUpdate>(this);
    m_settingsChangeConnection = m_dispatcher.sink<NotifySettingsChange>().connect<&TransportService::HandleNotifySettingsChange>(this);
    m_connectedConnection = m_dispatcher.sink<ConnectedEvent>().connect<&TransportService::HandleConnected>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&TransportService::HandleDisconnected>(this);

    m_connected = false;

    auto handlerGenerator = [this](auto& x)
    {
        using T = typename std::remove_reference_t<decltype(x)>::Type;

        m_messageHandlers[T::Opcode] = [this](UniquePtr<ServerMessage>& apMessage)
        {
            const auto pRealMessage = TiltedPhoques::CastUnique<T>(std::move(apMessage));
            m_dispatcher.trigger(*pRealMessage);
        };

        return false;
    };

    ServerMessageFactory::Visit(handlerGenerator);

    // Override authentication response
    m_messageHandlers[AuthenticationResponse::Opcode] = [this](UniquePtr<ServerMessage>& apMessage)
    {
        const auto pRealMessage = TiltedPhoques::CastUnique<AuthenticationResponse>(std::move(apMessage));
        HandleAuthenticationResponse(*pRealMessage);
    };
}

bool TransportService::Send(const ClientMessage& acMessage, const TiltedPhoques::EPacketFlags aPacketFlags) const noexcept
{
    static thread_local ScratchAllocator s_allocator(1 << 18);

    struct ScopedReset
    {
        ~ScopedReset() { s_allocator.Reset(); }
    } allocatorGuard;

    if (!IsConnected())
        return false;

    // Features of this build the server's protocol lacks just don't reach it
    if (!Wire::ToWire(acMessage.GetOpcode()))
    {
        spdlog::debug("Not sent, the server's protocol has no message {}", static_cast<int>(acMessage.GetOpcode()));
        return false;
    }

    {
        SendBuffer::PacketBuffer buffer(acMessage, s_allocator, 1 << 16);
        if (buffer.IsComplete())
        {
            TiltedPhoques::PacketView packet(reinterpret_cast<char*>(buffer.GetData()), static_cast<uint32_t>(buffer.Size()));
            Client::Send(&packet, aPacketFlags);
            return true;
        }
    }

    SendBuffer::PacketBuffer buffer(acMessage, *Allocator::GetDefault(), k_cbMaxSteamNetworkingSocketsMessageSizeSend);
    if (!buffer.IsComplete())
    {
        spdlog::error("Message with opcode {} could not be serialized within the {} byte send limit", static_cast<uint32_t>(acMessage.GetOpcode()),
                      k_cbMaxSteamNetworkingSocketsMessageSizeSend);
        return false;
    }

    TiltedPhoques::PacketView packet(reinterpret_cast<char*>(buffer.GetData()), static_cast<uint32_t>(buffer.Size()));
    Client::Send(&packet, aPacketFlags);
    return true;
}

void TransportService::OnConsume(const void* apData, uint32_t aSize)
{
    ServerMessageFactory factory;
    TiltedPhoques::ViewBuffer buf((uint8_t*)apData, aSize);
    Buffer::Reader reader(&buf);

    auto pMessage = factory.Extract(reader);
    if (!pMessage)
    {
        spdlog::error("Couldn't parse packet from server");
        return;
    }

    m_messageHandlers[pMessage->GetOpcode()](pMessage);
}

namespace
{
// BUILD_COMMIT stays the same across our builds, the launcher passes the id of its package instead.
String GetBuildVersion()
{
    // The version an official server checks for, when speaking its protocol
    if (Wire::IsOfficial184())
        return Wire::kOfficial184Version;

    char build[128]{};
    return GetEnvironmentVariableA("TP_BUILD", build, static_cast<DWORD>(std::size(build))) ? String(build) : String(BUILD_COMMIT);
}

// Launcher sessions have no overlay, and without this a refused player would stand around without a word.
void ShowRefusal(const AuthenticationResponse::ResponseType aType)
{
    using AR = AuthenticationResponse::ResponseType;

    const wchar_t* pText = nullptr;
    switch (aType)
    {
    case AR::kWrongVersion: pText = L"Der Server hat die Verbindung abgelehnt:\nHost und Mitspieler nutzen verschiedene Versionen von Fallout Together Reborn."; break;
    case AR::kModsMismatch: pText = L"Der Server hat die Verbindung abgelehnt:\nDie Mods passen nicht zu denen des Hosts."; break;
    case AR::kClientModsDisallowed: pText = L"Der Server hat die Verbindung abgelehnt:\nDer Host erlaubt keine Client-Mods."; break;
    case AR::kWrongPassword: pText = L"Der Server hat die Verbindung abgelehnt:\nDas Passwort ist falsch."; break;
    case AR::kServerFull: pText = L"Der Server hat die Verbindung abgelehnt:\nDer Server ist voll."; break;
    default: return;
    }

    char address[8]{};
    if (!GetEnvironmentVariableA("TP_AUTOCONNECT", address, static_cast<DWORD>(std::size(address))))
        return;

    HWND window = reinterpret_cast<HWND>(BSGraphics::GetMainWindow()->hWnd);
    MessageBoxW(window, pText, L"Fallout Together Reborn", MB_OK | MB_ICONWARNING | MB_TOPMOST);
    PostMessageW(window, WM_CLOSE, 0, 0);
}
} // namespace

void TransportService::OnConnected()
{
    if (PlayerCharacter::Get())
    {
        SendAuthenticationRequest();
        return;
    }

    m_authenticationPending = true;
}

void TransportService::SendAuthenticationRequest()
{
    AuthenticationRequest request{};
    request.Version = GetBuildVersion();
    request.SKSEActive = IsScriptExtenderLoaded();
    request.MO2Active = GetModuleHandleW(kMO2DllName);

    request.Token = m_serverPassword;
    m_lastPassword = m_serverPassword;
    m_serverPassword = "";

    PlayerCharacter* pPlayer = PlayerCharacter::Get();

    // null if discord is not active
    // TODO: think about user opt out
    request.DiscordId = m_world.ctx().at<DiscordService>().GetUser().id;
    auto* pNpc = Cast<TESNPC>(pPlayer->baseForm);
    if (pNpc)
    {
        request.Username = pNpc->fullName.value.AsAscii();
    }
    else
    {
        request.Username = "Some dragon boi";
    }

    auto* const cpModManager = ModManager::Get();

    for (auto* pMod : cpModManager->mods)
    {
        if (!pMod->IsLoaded())
            continue;

        auto& entry = request.UserMods.ModList.emplace_back();
        entry.Id = pMod->GetId();
        entry.IsLite = pMod->IsLite();
        entry.Filename = pMod->filename;
    }

    auto& modSystem = m_world.GetModSystem();
    if (pPlayer->GetWorldSpace())
        modSystem.GetServerModId(pPlayer->GetWorldSpace()->formID, request.WorldSpaceId);

    if (TESObjectCELL* pCell = pPlayer->GetParentCellEx())
        modSystem.GetServerModId(pCell->formID, request.CellId);

    request.Level = pPlayer->GetLevel();

    auto* pGameTime = TimeData::Get();
    request.PlayerTime.TimeScale = pGameTime->TimeScale->f;
    request.PlayerTime.Time = pGameTime->GameHour->f;
    request.PlayerTime.Year = pGameTime->GameYear->f;
    request.PlayerTime.Month = pGameTime->GameMonth->f;
    request.PlayerTime.Day = pGameTime->GameDay->f;

    Send(request);
}

bool TransportService::Connect(const std::string& acEndpoint) noexcept
{
    // A new connection starts in this build's protocol
    Wire::Set(WireProtocol::Current);
    m_reconnectAsOfficial = false;
    m_endpoint = acEndpoint;
    return Client::Connect(acEndpoint);
}

void TransportService::OnDisconnected(EDisconnectReason aReason)
{
    m_authenticationPending = false;
    m_connected = false;

    // The official server's kick after refusing our version: the overlay keeps "connecting" while we come back
    if (m_reconnectAsOfficial)
    {
        m_reconnectAsOfficial = false;
        Wire::Set(WireProtocol::Official184);
        m_serverPassword = m_lastPassword;
        spdlog::info("Reconnecting to {} as official Skyrim Together {}", m_endpoint, Wire::kOfficial184Version);
        m_world.GetRunner().Queue([this]() { Client::Connect(m_endpoint); });
        return;
    }

    spdlog::warn("Disconnected from server {}", aReason);

    m_dispatcher.trigger(DisconnectedEvent(aReason == kTimeout || aReason == kLocalProblem));
}

void TransportService::OnUpdate()
{
    if (!m_authenticationPending)
        return;

    if (!PlayerCharacter::Get())
        return;

    m_authenticationPending = false;
    SendAuthenticationRequest();
}

double TransportService::GetMessageReceiveTime() const noexcept
{
    return static_cast<double>(GetCurrentMessageReceiveTime()) / 1000.0;
}

void TransportService::HandleUpdate(const UpdateEvent& acEvent) noexcept
{
    TP_PROFILE_SCOPE("TransportService::HandleUpdate");
    // Taken before polling, so that every message that arrived before this time gets handled by this update
    m_lastPollTime = static_cast<double>(SteamNetworkingUtils()->GetLocalTimestamp()) / 1000.0;

    Update();
}

void TransportService::HandleConnected(const ConnectedEvent& acEvent) noexcept
{
    m_localPlayerId = acEvent.PlayerId;
}

void TransportService::HandleDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    m_localPlayerId = NULL;
}

void TransportService::HandleAuthenticationResponse(const AuthenticationResponse& acMessage) noexcept
{
    using AR = AuthenticationResponse::ResponseType;
    if (acMessage.Type == AR::kAccepted)
    {
        m_connected = true;

        if (Wire::IsOfficial184())
            m_world.GetOverlayService().SendSystemMessage(fmt::format(
                "This server runs official Skyrim Together {}. You're playing with it, without this version's own features (host world, party saves, voice chat, invites).",
                Wire::kOfficial184Version));

        m_world.SetServerSettings(acMessage.Settings);

        m_dispatcher.trigger(acMessage.UserMods);
        m_dispatcher.trigger(acMessage.Settings);
        m_dispatcher.trigger(ConnectedEvent(acMessage.PlayerId));
        return; // quit the function here.
    }

    // error finding

    if (acMessage.Type == AR::kWrongVersion && acMessage.Version == Wire::kOfficial184Version && !Wire::IsOfficial184())
    {
        spdlog::info("Server {} is official Skyrim Together {}, joining it in its protocol", m_endpoint, Wire::kOfficial184Version);
        m_reconnectAsOfficial = true;
        return;
    }

    spdlog::warn("Server refused the connection, type {}", static_cast<int>(acMessage.Type));
    ShowRefusal(acMessage.Type);

    TiltedPhoques::String ErrorInfo;

    ErrorInfo = "{";

    switch (acMessage.Type)
    {
    case AR::kWrongVersion:
        ErrorInfo += "\"error\": \"wrong_version\", \"data\": {";
        ErrorInfo += fmt::format("\"expectedVersion\": \"{}\", \"version\": \"{}\"", acMessage.Version, BUILD_COMMIT);
        ErrorInfo += "}";
        break;
    case AR::kModsMismatch:
    {
        ErrorInfo += "\"error\": \"mods_mismatch\", \"data\": {\"mods\": [";
        bool first = true;
        for (const auto& m : acMessage.UserMods.ModList)
        {
            if (!first)
                ErrorInfo += ",";
            ErrorInfo += fmt::format("[\"{}\",\"{}\"]", m.Filename.c_str(), m.Id);
            first = false;
        }
        ErrorInfo += "]}";
        break;
    }
    case AR::kClientModsDisallowed:
    {
        ErrorInfo += "\"error\": \"client_mods_disallowed\", \"data\": { \"mods\": [";
        if (acMessage.SKSEActive)
            ErrorInfo += "\"SKSE\"";
        if (acMessage.MO2Active)
            if (acMessage.SKSEActive)
                ErrorInfo += ",";
        ErrorInfo += "\"MO2\"";
        ErrorInfo += "]}";
        break;
    }
    case AR::kWrongPassword:
    {
        ErrorInfo += "\"error\": \"wrong_password\"";
        break;
    }
    case AR::kServerFull:
    {
        ErrorInfo += "\"error\": \"server_full\"";
        break;
    }
    default: ErrorInfo += "\"error\": \"no_reason\""; break;
    }

    ErrorInfo += "}";

    ConnectionErrorEvent errorEvent;
    if (!ErrorInfo.empty())
    {
        spdlog::error(ErrorInfo.c_str());
        errorEvent.ErrorDetail = std::move(ErrorInfo);
    }

    m_dispatcher.trigger(errorEvent);
}

void TransportService::HandleNotifySettingsChange(const NotifySettingsChange& acMessage) noexcept
{
    m_world.SetServerSettings(acMessage.Settings);
    m_dispatcher.trigger(acMessage.Settings);
}
