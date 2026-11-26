#include <TiltedOnlinePCH.h>

#include <Services/PartySaves.h>

#include <Services/OverlayService.h>
#include <Games/TES.h>
#include <Games/Skyrim/SaveLoad.h>
#include <immersive_launcher/launcher.h>
#include <OverlayApp.hpp>
#include <World.h>

#include <ShlObj.h>

#include <chrono>
#include <optional>

namespace
{
// The labels co-op saves start with: Together_<character>_<slot> (party checkpoints, SaveLoad.cpp),
// BeforeHostWorld_<character> (HostWorldService) and BeforeCoop_<character> (TransportService)
constexpr const char* kCoopPrefixes[] = {"Together_", "BeforeHostWorld_", "BeforeCoop_"};
constexpr char kSaveExtension[] = ".ess";

// Co-op save names are a label, the character's letters and digits, and a slot (BuildSaveName)
bool IsCoopSave(const std::string& acName) noexcept
{
    if (!std::all_of(acName.begin(), acName.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; }))
        return false;
    return std::any_of(std::begin(kCoopPrefixes), std::end(kCoopPrefixes), [&](const char* apPrefix) { return acName.rfind(apPrefix, 0) == 0; });
}

// Where the game keeps saves: Documents\My Games\<game folder>\<sLocalSavePath>. The GOG build names its folder
// "Skyrim Special Edition GOG" and ships Galaxy64.dll next to SkyrimSE.exe (both seen on a GOG 1.6.1179 install).
std::optional<std::filesystem::path> SaveFolder(std::string& aError) noexcept
{
    PWSTR pDocuments = nullptr;
    const HRESULT cResult = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &pDocuments);
    if (FAILED(cResult))
    {
        CoTaskMemFree(pDocuments);
        aError = fmt::format("Windows didn't say where the Documents folder is ({:#x})", static_cast<uint32_t>(cResult));
        return std::nullopt;
    }
    std::filesystem::path folder(pDocuments);
    CoTaskMemFree(pDocuments);

    std::error_code error;
    const bool cGog = std::filesystem::exists(launcher::GetLaunchContext()->gamePath / L"Galaxy64.dll", error);
    folder = folder / L"My Games" / (cGog ? L"Skyrim Special Edition GOG" : L"Skyrim Special Edition");

    // The game's own setting, read as it read it from its ini files
    const Setting* pSavePath = INISettingCollection::Get()->GetSetting("sLocalSavePath:General");
    const char* cpSavePath = pSavePath ? reinterpret_cast<const char*>(pSavePath->data) : nullptr;
    if (!cpSavePath || !*cpSavePath)
    {
        // The game's default, so the page still works; logged because a custom save folder would then be missed
        spdlog::warn("[PartySaves]: the game has no sLocalSavePath:General setting, using Saves\\");
        cpSavePath = "Saves\\";
    }
    return folder / cpSavePath;
}

// Paths hold the player's own folder names, which the narrow code page may not have
std::string Utf8(const std::filesystem::path& acPath) noexcept
{
    const std::u8string cText = acPath.u8string();
    return std::string(cText.begin(), cText.end());
}

int64_t UnixSeconds(const std::filesystem::file_time_type& acTime) noexcept
{
    const auto cSystem = std::chrono::clock_cast<std::chrono::system_clock>(acTime);
    return std::chrono::duration_cast<std::chrono::seconds>(cSystem.time_since_epoch()).count();
}

void SendJson(const std::string& acJson) noexcept
{
    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acJson);
    World::Get().GetOverlayService().GetOverlayApp()->ExecuteAsync("partySaves", pArguments);
}
} // namespace

namespace PartySaves
{
void Send() noexcept
{
    std::string error;
    const auto cFolder = SaveFolder(error);
    if (!cFolder)
    {
        spdlog::error("[PartySaves]: {}", error);
        SendJson(fmt::format(R"({{"error":{}}})", OverlayService::ToJsonString(error)));
        return;
    }

    std::string saves;
    std::error_code ec;
    // No folder yet: the game hasn't saved, so there are no co-op saves either
    if (std::filesystem::exists(*cFolder, ec))
    {
        for (std::filesystem::directory_iterator it(*cFolder, ec), end; !ec && it != end; it.increment(ec))
        {
            const std::filesystem::path& cPath = it->path();
            const std::string cName = Utf8(cPath.stem());
            if (cPath.extension() != kSaveExtension || !IsCoopSave(cName))
                continue;

            std::error_code timeError;
            const auto cTime = it->last_write_time(timeError);
            saves += fmt::format(R"({}{{"name":{},"time":{}}})", saves.empty() ? "" : ",", OverlayService::ToJsonString(cName), timeError ? 0 : UnixSeconds(cTime));
        }
    }
    if (ec)
    {
        error = fmt::format("Couldn't read {} ({})", Utf8(*cFolder), ec.message());
        spdlog::error("[PartySaves]: {}", error);
        SendJson(fmt::format(R"({{"error":{}}})", OverlayService::ToJsonString(error)));
        return;
    }

    SendJson(fmt::format(R"({{"character":{},"folder":{},"saves":[{}]}})", OverlayService::ToJsonString(BGSSaveLoadManager::CoopSaveCharacter().c_str()),
                         OverlayService::ToJsonString(Utf8(*cFolder)), saves));
}

void Load(const std::string& acName) noexcept
{
    std::string error;
    const auto cFolder = SaveFolder(error);
    std::error_code ec;
    if (!IsCoopSave(acName) || !cFolder || !std::filesystem::exists(*cFolder / (acName + kSaveExtension), ec))
    {
        spdlog::warn("[PartySaves]: not loading {}: it isn't a co-op save in the save folder {}", acName, error);
        World::Get().GetOverlayService().SendSystemMessage("Couldn't find the save " + acName + ".");
        return;
    }
    BGSSaveLoadManager::QueueLoad(acName.c_str());
}
} // namespace PartySaves
