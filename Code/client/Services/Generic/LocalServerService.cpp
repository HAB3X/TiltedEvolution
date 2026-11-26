#include <TiltedOnlinePCH.h>

#include <Services/LocalServerService.h>

#include <Services/OverlayService.h>
#include <Services/TransportService.h>

#include <OverlayApp.hpp>
#include <World.h>
#include <Games/TES.h>
#include <RequiredMods.h>
#include <Services/PluginChecksums.h>
#include <Games/Skyrim/SaveLoad.h>

#include <fstream>

#include <cstring>
#include <thread>

namespace
{
constexpr wchar_t kServerExe[] = L"SkyrimTogetherServer.exe";
// The server prints this once it listens
constexpr char kStartedLine[] = "started on port";
// The server tags what the host should see (network check, router port), see server PortMapper
constexpr char kHostTag[] = "[Host] ";
constexpr char kPublicAddress[] = "Public address: ";

// The folder of the server to run: the Plus one next to the client, or the official-protocol one in classic/
std::filesystem::path ServerDir(bool aClassic)
{
    return aClassic ? TiltedPhoques::GetPath() / L"classic" : TiltedPhoques::GetPath();
}

std::filesystem::path IniPath(bool aClassic)
{
    return ServerDir(aClassic) / L"config" / L"STServer.ini";
}

const wchar_t* Bool(bool aValue)
{
    return aValue ? L"true" : L"false";
}

std::wstring Widen(const std::string& acText)
{
    if (acText.empty())
        return {};
    const int cLength = MultiByteToWideChar(CP_UTF8, 0, acText.data(), static_cast<int>(acText.size()), nullptr, 0);
    std::wstring out(cLength, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, acText.data(), static_cast<int>(acText.size()), out.data(), cLength);
    return out;
}
// The host's character, as co-op saves name it; "world" without one
std::string KeptWorldName()
{
    const String cCharacter = BGSSaveLoadManager::CoopSaveCharacter();
    return cCharacter.empty() ? std::string("world") : std::string(cCharacter.c_str());
}
} // namespace

LocalServerService::LocalServerService(World& aWorld) noexcept
    : m_world(aWorld)
{
    // The server stops by itself, cleanly, when this game exits (GameServer:uStopWithProcess through its environment
    // variable): the game exits without running this service's cleanup, and killing the server would leave its router
    // port open until the router drops it a day later
    SetEnvironmentVariableW(L"USTOPWITHPROCESS", std::to_wstring(GetCurrentProcessId()).c_str());
}

LocalServerService::~LocalServerService() noexcept
{
    if (m_process)
        CloseHandle(m_process);
}

bool LocalServerService::IsRunning() const noexcept
{
    return m_process && WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT;
}

void LocalServerService::Host(const HostSettings& acSettings) noexcept
{
    if (IsRunning())
    {
        // One server per game session: it stops with the game, and killing it would leave its router port open
        if (m_classic != acSettings.Classic)
        {
            Fail(std::string("You're already hosting a ") + (m_classic ? "classic" : "Plus") + " game. Restart Skyrim to host the other kind.");
            return;
        }
        spdlog::info("[LocalServer]: already hosting on port {}, connecting", m_port);
        Connect(acSettings.Password);
        return;
    }

    Start(acSettings);
}

std::vector<RequiredMods::Listed> LocalServerService::HostMods() noexcept
{
    std::vector<RequiredMods::Listed> mods;
    for (auto* pMod : ModManager::Get()->mods)
    {
        if (pMod->IsLoaded() && !RequiredMods::IsGamePlugin(pMod->filename))
            mods.push_back({pMod->filename, pMod->GetFileSize()});
    }
    return mods;
}

void LocalServerService::SendHostMods() const noexcept
{
    // The Host page is open: hosting will need the checksums soon
    PluginChecksums::Start();

    std::string json = "[";
    for (const RequiredMods::Listed& cMod : HostMods())
        json += (json.size() > 1 ? "," : "") + OverlayService::ToJsonString(cMod.Name);
    json += "]";

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, json);
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("hostMods", pArguments);
}

bool LocalServerService::WriteRequiredMods() const noexcept
{
    std::ofstream file(TiltedPhoques::GetPath() / RequiredMods::kFileName, std::ios::trunc);
    if (!file)
        return false;

    const auto cMods = HostMods();
    for (const RequiredMods::Listed& cMod : cMods)
        file << RequiredMods::FormatLine(cMod.Name, cMod.FileSize, PluginChecksums::Get(cMod.Name)) << '\n';
    spdlog::info("[LocalServer]: friends need the host's {} mods", cMods.size());
    return file.good();
}

bool LocalServerService::Start(const HostSettings& acSettings) noexcept
{
    const bool cClassic = acSettings.Classic;
    const std::filesystem::path cDir = ServerDir(cClassic);
    const std::filesystem::path cExe = cDir / kServerExe;
    std::error_code ec;
    if (!std::filesystem::exists(cExe, ec))
    {
        Fail(cClassic ? "The classic server (classic\\SkyrimTogetherServer.exe) isn't in the Skyrim Together folder, so this copy can't host classic games."
                      : "SkyrimTogetherServer.exe isn't in the Skyrim Together folder, so this copy can't host.");
        return false;
    }

    // The server reads its settings from here; the rest of the file keeps what the player set
    std::filesystem::create_directories(IniPath(cClassic).parent_path(), ec);
    const std::wstring cIni = IniPath(cClassic).wstring();
    bool written = WritePrivateProfileStringW(L"GameServer", L"sPassword", Widen(acSettings.Password).c_str(), cIni.c_str()) &&
                   WritePrivateProfileStringW(L"Gameplay", L"bEnablePvp", Bool(acSettings.Pvp), cIni.c_str());
    if (!cClassic)
    {
        written = written && WritePrivateProfileStringW(L"Gameplay", L"bHostWorld", Bool(acSettings.HostWorld), cIni.c_str()) &&
                  WritePrivateProfileStringW(L"ModPolicy", L"bRequireMods", Bool(acSettings.RequireMods), cIni.c_str()) &&
                  WritePrivateProfileStringW(L"Gameplay", L"bPauseWhenAllInMenus", Bool(acSettings.PauseInMenus), cIni.c_str()) &&
                  WritePrivateProfileStringW(L"Gameplay", L"sSharedQuestTypes", Widen(acSettings.SharedQuestTypes).c_str(), cIni.c_str()) &&
                  // The older Misc switch would share Misc quests even when the host leaves them out
                  WritePrivateProfileStringW(L"Gameplay", L"bEnableMiscQuestSync", L"false", cIni.c_str()) &&
                  WritePrivateProfileStringW(L"GameServer", L"sRelay", Widen(acSettings.Relay).c_str(), cIni.c_str()) &&
                  // Each character's playthrough keeps its own world (WorldSaveService)
                  WritePrivateProfileStringW(L"Gameplay", L"sKeepWorldName", Widen(KeptWorldName()).c_str(), cIni.c_str()) &&
                  WritePrivateProfileStringW(L"GameServer", L"sRelayPassword", Widen(acSettings.RelayPassword).c_str(), cIni.c_str());
    }
    if (!written)
    {
        Fail("Couldn't write the server settings (" + std::to_string(GetLastError()) + ").");
        return false;
    }
    if (!cClassic && acSettings.RequireMods && !WriteRequiredMods())
    {
        Fail("Couldn't write the list of your mods for the server.");
        return false;
    }
    m_port = static_cast<uint16_t>(GetPrivateProfileIntW(L"GameServer", L"uPort", 10578, cIni.c_str()));

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    HANDLE outRead{}, outWrite{}, inRead{}, inWrite{};
    if (!CreatePipe(&outRead, &outWrite, &inherit, 0) || !CreatePipe(&inRead, &inWrite, &inherit, 0))
    {
        Fail("Couldn't start the server (" + std::to_string(GetLastError()) + ").");
        return false;
    }
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = inRead;
    startup.hStdOutput = outWrite;
    startup.hStdError = outWrite;

    std::wstring commandLine = L"\"" + cExe.wstring() + L"\"";
    PROCESS_INFORMATION process{};
    const BOOL cStarted =
        CreateProcessW(cExe.c_str(), commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, cDir.c_str(), &startup, &process);
    const DWORD cError = GetLastError();
    CloseHandle(outWrite);
    CloseHandle(inRead);
    if (!cStarted)
    {
        CloseHandle(outRead);
        CloseHandle(inWrite);
        Fail("Couldn't start SkyrimTogetherServer.exe (" + std::to_string(cError) + ").");
        return false;
    }

    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    m_process = process.hProcess;
    m_classic = cClassic;

    spdlog::info("[LocalServer]: started the {} server on port {}, host world {}, require mods {}, pause in menus {}, shared quests {}", cClassic ? "classic" : "Plus", m_port,
                 acSettings.HostWorld, acSettings.RequireMods, acSettings.PauseInMenus, acSettings.SharedQuestTypes);

    // Reads the server's output for as long as it runs: it would stop when the pipe fills up. Its stdin stays open
    // here (inWrite is never closed), so that it doesn't see the end of its input.
    std::thread(
        [outRead, password = acSettings.Password, port = m_port]()
        {
            std::string pending, lastLine;
            bool started = false;
            char buffer[4096];
            DWORD read = 0;
            while (ReadFile(outRead, buffer, sizeof(buffer), &read, nullptr) && read > 0)
            {
                pending.append(buffer, read);
                for (size_t end; (end = pending.find('\n')) != std::string::npos; pending.erase(0, end + 1))
                {
                    std::string line = pending.substr(0, end);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (line.empty())
                        continue;
                    spdlog::info("[Server] {}", line);
                    lastLine = line;

                    if (const size_t cTag = line.find(kHostTag); cTag != std::string::npos)
                    {
                        const std::string cMessage = line.substr(cTag + std::strlen(kHostTag));
                        World::Get().GetRunner().Queue([cMessage]() { World::Get().ctx().at<LocalServerService>().OnHostMessage(cMessage); });
                    }

                    if (!started && line.find(kStartedLine) != std::string::npos)
                    {
                        started = true;
                        World::Get().GetRunner().Queue(
                            [password, port]()
                            {
                                World::Get().GetOverlayService().SendSystemMessage(
                                    "You're hosting. Friends join with your IP address, port " + std::to_string(port) +
                                    " and your password (from the internet, forward UDP port " + std::to_string(port) + " on your router).");
                                World::Get().ctx().at<LocalServerService>().Connect(password);
                            });
                    }
                }
            }
            CloseHandle(outRead);

            spdlog::warn("[LocalServer]: the server stopped, last output: {}", lastLine);
            if (!started)
                World::Get().GetRunner().Queue([lastLine]() { World::Get().ctx().at<LocalServerService>().Fail("The server stopped before it started: " + lastLine); });
        })
        .detach();

    return true;
}

void LocalServerService::OnHostMessage(const std::string& acMessage) noexcept
{
    m_world.GetOverlayService().SendSystemMessage(acMessage);

    // "Public address: 1.2.3.4:10578 (…)": what friends outside the network join with, for the invite
    if (acMessage.rfind(kPublicAddress, 0) != 0)
        return;
    const size_t cStart = std::strlen(kPublicAddress);
    const std::string cAddress = acMessage.substr(cStart, acMessage.find(' ', cStart) - cStart);
    spdlog::info("[LocalServer]: public address {}", cAddress);

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, cAddress);
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("hostAddress", pArguments);
}

void LocalServerService::Connect(const std::string& acPassword) const noexcept
{
    auto& transport = m_world.GetTransport();
    transport.SetServerPassword(acPassword);
    transport.Connect("127.0.0.1:" + std::to_string(m_port));
}

void LocalServerService::Fail(const std::string& acReason) const noexcept
{
    spdlog::error("[LocalServer]: {}", acReason);

    // Ends the overlay's connecting state, then shows why
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("disconnect");
    m_world.GetOverlayService().SendError(acReason);
}
