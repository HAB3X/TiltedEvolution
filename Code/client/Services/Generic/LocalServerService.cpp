#include <TiltedOnlinePCH.h>

#include <Services/LocalServerService.h>

#include <Services/OverlayService.h>
#include <Services/TransportService.h>

#include <OverlayApp.hpp>
#include <World.h>

#include <thread>

namespace
{
constexpr wchar_t kServerExe[] = L"SkyrimTogetherServer.exe";
// The server prints this once it listens
constexpr char kStartedLine[] = "started on port";

std::filesystem::path IniPath()
{
    return TiltedPhoques::GetPath() / L"config" / L"STServer.ini";
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
} // namespace

LocalServerService::LocalServerService(World& aWorld) noexcept
    : m_world(aWorld)
{
    m_job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!m_job || !SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        spdlog::error("[LocalServer]: couldn't create the job that stops the server with the game ({})", GetLastError());
}

LocalServerService::~LocalServerService() noexcept
{
    if (m_process)
        CloseHandle(m_process);
    if (m_job)
        CloseHandle(m_job);
}

bool LocalServerService::IsRunning() const noexcept
{
    return m_process && WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT;
}

void LocalServerService::Host(const std::string& acPassword, bool aHostWorld) noexcept
{
    if (IsRunning())
    {
        spdlog::info("[LocalServer]: already hosting on port {}, connecting", m_port);
        Connect(acPassword);
        return;
    }

    Start(acPassword, aHostWorld);
}

bool LocalServerService::Start(const std::string& acPassword, bool aHostWorld) noexcept
{
    const std::filesystem::path cDir = TiltedPhoques::GetPath();
    const std::filesystem::path cExe = cDir / kServerExe;
    std::error_code ec;
    if (!std::filesystem::exists(cExe, ec))
    {
        Fail("SkyrimTogetherServer.exe isn't in the Skyrim Together folder, so this copy can't host.");
        return false;
    }

    // The server reads its settings from here; the rest of the file keeps what the player set
    std::filesystem::create_directories(IniPath().parent_path(), ec);
    const std::wstring cIni = IniPath().wstring();
    if (!WritePrivateProfileStringW(L"GameServer", L"sPassword", Widen(acPassword).c_str(), cIni.c_str()) ||
        !WritePrivateProfileStringW(L"Gameplay", L"bHostWorld", aHostWorld ? L"true" : L"false", cIni.c_str()))
    {
        Fail("Couldn't write the server settings (" + std::to_string(GetLastError()) + ").");
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

    if (!m_job || !AssignProcessToJobObject(m_job, process.hProcess))
        spdlog::warn("[LocalServer]: the server won't stop by itself when the game exits ({})", GetLastError());
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    m_process = process.hProcess;

    spdlog::info("[LocalServer]: started the server on port {}, host world {}", m_port, aHostWorld);

    // Reads the server's output for as long as it runs: it would stop when the pipe fills up. Its stdin stays open
    // here (inWrite is never closed), so that it doesn't see the end of its input.
    std::thread(
        [outRead, password = acPassword, port = m_port]()
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
    auto* pOverlay = m_world.GetOverlayService().GetOverlayApp();
    pOverlay->ExecuteAsync("disconnect");
    auto pArgs = CefListValue::Create();
    pArgs->SetString(0, acReason);
    pOverlay->ExecuteAsync("triggerError", pArgs);
}
