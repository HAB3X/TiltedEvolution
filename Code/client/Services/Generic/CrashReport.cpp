#include <TiltedOnlinePCH.h>

#include <Services/CrashReport.h>
#include <Services/OverlayService.h>

#include <OverlayApp.hpp>
#include <World.h>
#include <VersionDb.h>

#include <filesystem>

namespace
{
// "Windows 10.0.19045", plus "(Wine 9.0)" under Wine
std::string SystemDescription()
{
    std::string text = "Windows";
    const HMODULE cNtdll = GetModuleHandleW(L"ntdll.dll");
    using TRtlGetVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (const auto pRtlGetVersion = cNtdll ? reinterpret_cast<TRtlGetVersion>(GetProcAddress(cNtdll, "RtlGetVersion")) : nullptr)
    {
        OSVERSIONINFOW info{sizeof(info)};
        if (pRtlGetVersion(&info) == 0)
            text += fmt::format(" {}.{}.{}", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber);
    }
    using TWineGetVersion = const char*(CDECL*)();
    if (const auto pWineGetVersion = cNtdll ? reinterpret_cast<TWineGetVersion>(GetProcAddress(cNtdll, "wine_get_version")) : nullptr)
        text += fmt::format(" (Wine {})", pWineGetVersion());
    return text;
}
} // namespace

void CrashReport::Send() noexcept
{
    const std::filesystem::path cFolder = TiltedPhoques::GetPath();
    std::error_code error;
    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime{};
    for (const auto& cEntry : std::filesystem::directory_iterator(cFolder, error))
    {
        const std::string cName = cEntry.path().filename().string();
        if (cName.rfind("crash_UTC_", 0) != 0 || cEntry.path().extension() != ".dmp")
            continue;
        const auto cTime = cEntry.last_write_time(error);
        if (!error && (newest.empty() || cTime > newestTime))
        {
            newest = cEntry.path();
            newestTime = cTime;
        }
    }
    if (error)
    {
        spdlog::warn("Couldn't look for crash dumps in {}: {}", cFolder.string(), error.message());
        return;
    }
    if (newest.empty())
        return;

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, newest.filename().string());
    pArguments->SetString(1, cFolder.string());
    pArguments->SetString(2, BUILD_COMMIT);
    pArguments->SetString(3, VersionDb::Get().GetLoadedVersionString());
    pArguments->SetString(4, SystemDescription());
    World::Get().GetOverlayService().GetOverlayApp()->ExecuteAsync("crashDump", pArguments);
}
