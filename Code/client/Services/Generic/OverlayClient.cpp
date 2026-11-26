#include <TiltedOnlinePCH.h>

#include <OverlayRenderHandler.hpp>
#include <DInputHook.hpp>

#include <Services/OverlayClient.h>
#include <Services/TransportService.h>
#include <Services/VoiceService.h>
#include <Services/LocalServerService.h>
#include <Services/LanSearch.h>
#include <Services/PartySaves.h>
#include <Services/NameTagService.h>
#include <Services/HostWorldService.h>
#include <Services/TravelTogetherService.h>
#include <Services/QuestSharingService.h>
#include <Services/QuestDesyncService.h>
#include <Services/MapService.h>

#include <Messages/SendChatMessageRequest.h>
#include <Messages/TeleportRequest.h>
#include <Messages/RequestModeratePlayer.h>
#include <Messages/RequestVote.h>
#include <Messages/RequestVoteAnswer.h>

#include <Events/SetTimeCommandEvent.h>
#include <Events/WaveCommandEvent.h>

#include <World.h>

OverlayClient::OverlayClient(TransportService& aTransport, TiltedPhoques::OverlayRenderHandler* apHandler)
    : TiltedPhoques::OverlayClient(apHandler)
    , m_transport(aTransport)
{
}

OverlayClient::~OverlayClient() noexcept
{
}

bool OverlayClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
    if (message->GetName() == "ui-event")
    {
        auto pArguments = message->GetArgumentList();

        auto eventName = pArguments->GetString(0).ToString();
        auto eventArgs = pArguments->GetList(1);

        spdlog::info(eventName);
        spdlog::info(eventArgs->GetString(0).ToString());
        spdlog::info(std::to_string(eventArgs->GetInt(1)));
        spdlog::info(eventArgs->GetString(2).ToString());

#ifndef PUBLIC_BUILD
        LOG(INFO) << "event=ui_event name=" << eventName;
#endif

        if (eventName == "connect")
            ProcessConnectMessage(eventArgs);
        else if (eventName == "answerHostWorld")
        {
            const bool join = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([join]() { World::Get().ctx().at<HostWorldService>().Answer(join); });
        }
        else if (eventName == "answerPartyTravel")
        {
            const bool travel = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([travel]() { World::Get().ctx().at<TravelTogetherService>().Answer(travel); });
        }
        else if (eventName == "setPartyBar")
        {
            const bool enabled = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([enabled]() { World::Get().GetOverlayService().SetPartyBarEnabled(enabled); });
        }
        else if (eventName == "setPerfOverlay")
        {
            const bool enabled = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([enabled]() { World::Get().GetOverlayService().SetPerfOverlayEnabled(enabled); });
        }
        else if (eventName == "setPingMarkers")
        {
            const bool enabled = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([enabled]() { World::Get().ctx().at<MapService>().SetPingMarkersEnabled(enabled); });
        }
        else if (eventName == "summonPlayer")
        {
            const uint32_t playerId = eventArgs->GetInt(0);
            World::Get().GetRunner().Queue([playerId]() { World::Get().ctx().at<TravelTogetherService>().Summon(playerId); });
        }
        else if (eventName == "answerPartySummon")
        {
            const uint32_t summonerId = eventArgs->GetInt(0);
            const bool accept = eventArgs->GetBool(1);
            World::Get().GetRunner().Queue([summonerId, accept]() { World::Get().ctx().at<TravelTogetherService>().AnswerSummon(summonerId, accept); });
        }
        else if (eventName == "setPartyQuestSharing")
        {
            const bool enabled = eventArgs->GetBool(0);
            const uint32_t types = static_cast<uint32_t>(eventArgs->GetInt(1));
            World::Get().GetRunner().Queue([enabled, types]() { World::Get().ctx().at<QuestSharingService>().SetSharing(enabled, types); });
        }
        else if (eventName == "listShareableQuests")
            World::Get().GetRunner().Queue([]() { World::Get().ctx().at<QuestSharingService>().ListShareableQuests(); });
        else if (eventName == "shareQuest")
        {
            const uint32_t questId = static_cast<uint32_t>(eventArgs->GetInt(0));
            World::Get().GetRunner().Queue([questId]() { World::Get().ctx().at<QuestSharingService>().Share(questId); });
        }
        else if (eventName == "answerQuestShare")
        {
            const uint32_t questId = static_cast<uint32_t>(eventArgs->GetInt(0));
            const bool accept = eventArgs->GetBool(1);
            World::Get().GetRunner().Queue([questId, accept]() { World::Get().ctx().at<QuestSharingService>().Answer(questId, accept); });
        }
        else if (eventName == "answerQuestCatchUp")
        {
            const bool take = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([take]() { World::Get().ctx().at<QuestDesyncService>().AnswerCatchUp(take); });
        }
        else if (eventName == "answerPartyDoor")
        {
            const bool follow = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([follow]() { World::Get().ctx().at<TravelTogetherService>().AnswerDoor(follow); });
        }
        else if (eventName == "setNameTags")
        {
            const bool enabled = eventArgs->GetBool(0);
            World::Get().GetRunner().Queue([enabled]() { World::Get().ctx().at<NameTagService>().SetEnabled(enabled); });
        }
        else if (eventName == "moderatePlayer")
        {
            RequestModeratePlayer request;
            request.PlayerId = static_cast<uint32_t>(eventArgs->GetInt(0));
            request.Ban = eventArgs->GetBool(1);
            World::Get().GetRunner().Queue([request]() { World::Get().GetTransport().Send(request); });
        }
        else if (eventName == "startVote")
        {
            RequestVote request;
            request.Kind = eventArgs->GetInt(0) == 0 ? VoteKind::Kick : VoteKind::Ready;
            request.TargetId = static_cast<uint32_t>(eventArgs->GetInt(1));
            World::Get().GetRunner().Queue([request]() { World::Get().GetTransport().Send(request); });
        }
        else if (eventName == "answerVote")
        {
            RequestVoteAnswer request;
            request.VoteId = static_cast<uint32_t>(eventArgs->GetInt(0));
            request.Yes = eventArgs->GetBool(1);
            World::Get().GetRunner().Queue([request]() { World::Get().GetTransport().Send(request); });
        }
        else if (eventName == "searchLanServers")
            LanSearch::Start();
        else if (eventName == "hostServer")
        {
            HostSettings settings;
            settings.Password = eventArgs->GetString(0).ToString();
            settings.HostWorld = eventArgs->GetBool(1);
            settings.RequireMods = eventArgs->GetBool(2);
            settings.Pvp = eventArgs->GetBool(3);
            settings.PauseInMenus = eventArgs->GetBool(4);
            settings.Classic = eventArgs->GetBool(5);
            settings.SharedQuestTypes = eventArgs->GetString(6).ToString();
            settings.Relay = eventArgs->GetString(7).ToString();
            settings.RelayPassword = eventArgs->GetString(8).ToString();
            settings.PerPlayerLoot = eventArgs->GetBool(9);
            World::Get().GetRunner().Queue([settings]() { World::Get().ctx().at<LocalServerService>().Host(settings); });
        }
        else if (eventName == "disconnect")
            ProcessDisconnectMessage();
        else if (eventName == "revealPlayers")
            ProcessRevealPlayersMessage();
        else if (eventName == "sendMessage")
            ProcessChatMessage(eventArgs);
        else if (eventName == "setTime")
            ProcessSetTimeCommand(eventArgs);
        else if (eventName == "wave")
            ProcessWaveCommand();
        else if (eventName == "launchParty")
            World::Get().GetPartyService().CreateParty();
        else if (eventName == "leaveParty")
            World::Get().GetPartyService().LeaveParty();
        else if (eventName == "createPartyInvite")
        {
            uint32_t aPlayerId = eventArgs->GetInt(0);
            World::Get().GetPartyService().CreateInvite(aPlayerId);
        }
        else if (eventName == "acceptPartyInvite")
        {
            uint32_t aInviterId = eventArgs->GetInt(0);
            // push to main thread because the party service has to check validity of invite thread safely
            World::Get().GetRunner().Queue([aInviterId]() { World::Get().GetPartyService().AcceptInvite(aInviterId); });
        }
        else if (eventName == "kickPartyMember")
        {
            uint32_t aPlayerId = eventArgs->GetInt(0);
            World::Get().GetPartyService().KickPartyMember(aPlayerId);
        }
        else if (eventName == "changePartyLeader")
        {
            uint32_t aPlayerId = eventArgs->GetInt(0);
            World::Get().GetPartyService().ChangePartyLeader(aPlayerId);
        }
        else if (eventName == "teleportToPlayer")
            ProcessTeleportMessage(eventArgs);
        else if (eventName == "toggleDebugUI")
            ProcessToggleDebugUI();
        else if (eventName == "openPlayGuide")
            ProcessOpenPlayGuide();
        else if (eventName == "openUrl")
            ProcessOpenUrl(eventArgs);
        else if (eventName == "listPartySaves")
            World::Get().GetRunner().Queue([]() { PartySaves::Send(); });
        else if (eventName == "loadPartySave")
        {
            const std::string name = eventArgs->GetString(0).ToString();
            // Out of the way of the game's loading screen
            SetUIVisible(false);
            World::Get().GetRunner().Queue([name]() { PartySaves::Load(name); });
        }
        else if (eventName == "listHostMods")
            World::Get().GetRunner().Queue([]() { World::Get().ctx().at<LocalServerService>().SendHostMods(); });
        else if (eventName == "voiceRefresh")
            World::Get().GetRunner().Queue([]() { World::Get().ctx().at<VoiceService>().RefreshUi(); });
        else if (eventName == "setVoiceSettings")
            ProcessSetVoiceSettings(eventArgs);
        else if (eventName == "setVoicePlayerVolume")
        {
            const uint32_t playerId = eventArgs->GetInt(0);
            const float volume = static_cast<float>(eventArgs->GetInt(1)) / 100.f;
            World::Get().GetRunner().Queue([playerId, volume]() { World::Get().ctx().at<VoiceService>().SetPlayerVolume(playerId, volume); });
        }
        else if (eventName == "setVoiceMeter")
        {
            const bool enabled = eventArgs->GetInt(0) != 0;
            World::Get().GetRunner().Queue([enabled]() { World::Get().ctx().at<VoiceService>().SetMeterEnabled(enabled); });
        }

        return true;
    }

    return false;
}

void OverlayClient::ProcessConnectMessage(CefRefPtr<CefListValue> aEventArgs)
{
    std::string baseIp = aEventArgs->GetString(0);
    if (baseIp == "localhost")
    {
        baseIp = "127.0.0.1";
    }

    uint16_t port = aEventArgs->GetInt(1) ? aEventArgs->GetInt(1) : 10578;
    World::Get().GetTransport().SetServerPassword(aEventArgs->GetString(2));

    std::string endpoint = baseIp + ":" + std::to_string(port);
    // Rejoining after the game stopped (ClientService.rejoin in the overlay)
    const bool cReturning = aEventArgs->GetSize() > 3 && aEventArgs->GetBool(3);

    World::Get().GetRunner().Queue([endpoint, cReturning] { World::Get().GetTransport().Connect(endpoint, cReturning); });
}

void OverlayClient::ProcessDisconnectMessage()
{
    World::Get().GetRunner().Queue([]() { World::Get().GetTransport().Close(); });
}

void OverlayClient::ProcessRevealPlayersMessage()
{
    SetUIVisible(false);
    World::Get().GetMagicService().StartRevealingOtherPlayers();
}

void OverlayClient::ProcessChatMessage(CefRefPtr<CefListValue> aEventArgs)
{
    std::string contents = aEventArgs->GetString(1).ToString();
    if (!contents.empty())
    {
        SendChatMessageRequest messageRequest;
        messageRequest.MessageType = static_cast<ChatMessageType>(aEventArgs->GetInt(0));
        messageRequest.ChatMessage = contents;

        spdlog::info(L"Send chat message of type {}: '{}' ", messageRequest.MessageType, aEventArgs->GetString(1).ToWString());

        m_transport.Send(messageRequest);
    }
}

void OverlayClient::ProcessSetTimeCommand(CefRefPtr<CefListValue> aEventArgs)
{
    const uint8_t hours = static_cast<uint8_t>(aEventArgs->GetInt(0));
    const uint8_t minutes = static_cast<uint8_t>(aEventArgs->GetInt(1));
    const uint32_t senderId = m_transport.GetLocalPlayerId();
    World::Get().GetDispatcher().trigger(SetTimeCommandEvent(hours, minutes, senderId));
}

void OverlayClient::ProcessWaveCommand()
{
    World::Get().GetRunner().Trigger(WaveCommandEvent());
}

void OverlayClient::ProcessTeleportMessage(CefRefPtr<CefListValue> aEventArgs)
{
    TeleportRequest request{};
    request.PlayerId = aEventArgs->GetInt(0);

    m_transport.Send(request);
}

void OverlayClient::ProcessToggleDebugUI()
{
    World::Get().GetDebugService().m_showDebugStuff = !World::Get().GetDebugService().m_showDebugStuff;
}

void OverlayClient::ProcessOpenPlayGuide()
{
    ShellExecuteW(nullptr, L"open", LR"(https://wiki.tiltedphoques.com/tilted-online/general-information/playguide)", nullptr, nullptr, SW_SHOWNORMAL);
}

// Only these pages: Nexus Mods (the refusal box's mod searches), the Plus release page (a version refusal) and a new
// Plus issue (reporting a crash, CrashReport). The
// overlay shows text from servers, so a page in it must not be able to open other addresses or programs through the shell
void OverlayClient::ProcessOpenUrl(CefRefPtr<CefListValue> aEventArgs)
{
    constexpr const char* kAllowedPrefixes[] = {"https://www.nexusmods.com/", "https://github.com/HAB3X/TiltedEvolution/releases",
                                                "https://github.com/HAB3X/TiltedEvolution/issues/new?"};
    const std::string cUrl = aEventArgs->GetString(0).ToString();
    const bool cPlain = std::all_of(cUrl.begin(), cUrl.end(), [](unsigned char c) { return c > ' ' && c < 0x7F && c != '"'; });
    const bool cAllowed = std::any_of(std::begin(kAllowedPrefixes), std::end(kAllowedPrefixes), [&](const char* apPrefix) { return cUrl.rfind(apPrefix, 0) == 0; });
    if (!cAllowed || !cPlain)
    {
        spdlog::warn("Not opening {}: only Nexus Mods and the Plus release and new issue pages can be opened from the overlay", cUrl);
        return;
    }
    ShellExecuteA(nullptr, "open", cUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// Numbers come as whole numbers from the UI: percentages for the volumes and units for the fade start
void OverlayClient::ProcessSetVoiceSettings(CefRefPtr<CefListValue> aEventArgs)
{
    VoiceService::Settings settings{};
    settings.InputDevice = aEventArgs->GetString(0).ToString().c_str();
    settings.Connect = aEventArgs->GetInt(1) != 0;
    settings.OutputDevice = aEventArgs->GetString(2).ToString().c_str();

    const auto channel = aEventArgs->GetString(3).ToString();
    settings.ListenChannel = channel == "party" ? VoiceService::Channel::kParty : channel == "strangers" ? VoiceService::Channel::kStrangers : VoiceService::Channel::kEveryone;

    settings.FadeStart = static_cast<float>(aEventArgs->GetInt(4));
    settings.MicVolume = static_cast<float>(aEventArgs->GetInt(5)) / 100.f;
    settings.NoiseGate = static_cast<float>(aEventArgs->GetInt(6)) / 100.f;

    World::Get().GetRunner().Queue([settings]() { World::Get().ctx().at<VoiceService>().ApplySettings(settings); });
}

void OverlayClient::SetUIVisible(bool aVisible) noexcept
{
    auto pRenderer = GetOverlayRenderHandler();
    if (!pRenderer)
        return;

    TiltedPhoques::DInputHook::Get().SetEnabled(aVisible);
    World::Get().GetOverlayService().SetActive(aVisible);
    pRenderer->SetCursorVisible(aVisible);
}
