// Copyright (C) 2021 TiltedPhoques SRL.
// For licensing information see LICENSE at the root of this distribution.

#include <Windows.h>

#include <optional>
#include <shobjidl.h>

#include "TargetConfig.h"
#include "launcher.h"

#include "utils/ComUtils.h"
#include "utils/Registry.h"

namespace oobe
{
using namespace TiltedPhoques;
namespace fs = std::filesystem;

namespace
{
constexpr wchar_t kBethesdaRegistryPath[] = LR"(Software\Wow6432Node\Bethesda Softworks\)" SHORT_NAME;
constexpr wchar_t kGogRegistryPath[] = LR"(Software\Wow6432Node\GOG.com\Games\1711230643)";
constexpr wchar_t kTiltedRegistryPath[] = LR"(Software\TiltedPhoques\TiltedEvolution\)" SHORT_NAME;
constexpr wchar_t kExeName[] = TARGET_NAME L".exe";
constexpr int kSelectionAttempts = 3;

bool HasGame(const fs::path& aDir)
{
    std::error_code ec;
    return !aDir.empty() && fs::exists(aDir / kExeName, ec);
}

// Skyrim Together is installed into the game's Data\SkyrimTogetherReborn folder, so the game is normally a parent of ours
fs::path FindGameAroundUs()
{
    for (fs::path dir = TiltedPhoques::GetPath(); dir.has_relative_path(); dir = dir.parent_path())
    {
        if (HasGame(dir))
            return dir;
    }
    return {};
}

// The game folder without asking: around our own folder, then where Steam or GOG Galaxy installed it
fs::path FindGame()
{
    if (fs::path dir = FindGameAroundUs(); !dir.empty())
        return dir;

    const std::pair<const wchar_t*, const wchar_t*> cInstallKeys[] = {{kBethesdaRegistryPath, L"installed path"}, {kGogRegistryPath, L"path"}};
    for (const auto& [subKey, value] : cInstallKeys)
    {
        fs::path dir = Registry::ReadString<wchar_t>(HKEY_LOCAL_MACHINE, subKey, value);
        if (HasGame(dir))
            return dir;
    }
    return {};
}

std::optional<fs::path> AskForGameFolder(const fs::path& aSuggestion)
{
    // in case some other stuff already tried to unregister the global instance
    ComScope _;

    ComPtr<IFileOpenDialog> pFileDialog;
    auto hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(pFileDialog.GetAddressOf()));
    if (FAILED(hr))
        return std::nullopt;

    pFileDialog->SetTitle(L"Select your " SHORT_NAME L" folder (the one with " TARGET_NAME L".exe)");
    DWORD options = 0;
    pFileDialog->GetOptions(&options);
    pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    if (!aSuggestion.empty())
    {
        ComPtr<IShellItem> pItem;
        if (SUCCEEDED(SHCreateItemFromParsingName(aSuggestion.c_str(), nullptr, IID_PPV_ARGS(&pItem))))
            pFileDialog->SetFolder(pItem.Get());
    }

    if (FAILED(pFileDialog->Show(nullptr)))
        return std::nullopt;

    ComPtr<IShellItem> pItem;
    PWSTR folder = nullptr;
    if (FAILED(pFileDialog->GetResult(pItem.GetAddressOf())) || FAILED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &folder)))
        return std::nullopt;

    fs::path result = folder;
    CoTaskMemFree(folder);
    return result;
}

// The folder picker until it gives a game folder; empty when cancelled or out of attempts
fs::path PickGameFolder()
{
    const fs::path cSuggestion = FindGame();
    for (int i = 0; i < kSelectionAttempts; i++)
    {
        auto folder = AskForGameFolder(cSuggestion);
        if (!folder)
            return {};
        // not our own folder: the game can't be installed into Skyrim Together's directory
        if (HasGame(*folder) && *folder != TiltedPhoques::GetPath())
            return *folder;

        MessageBoxW(nullptr, L"That folder doesn't have " TARGET_NAME L".exe in it. Pick the folder " SHORT_NAME L" is installed in.", PRODUCT_NAME, MB_ICONWARNING);
    }
    return {};
}
} // namespace

bool SelectInstall(bool aForceSelect)
{
    // TitleExe is separate so a custom executable can be chosen with a launch argument (PathArgument)
    fs::path gamePath = Registry::ReadString<wchar_t>(HKEY_CURRENT_USER, kTiltedRegistryPath, L"TitlePath");
    fs::path exePath = Registry::ReadString<wchar_t>(HKEY_CURRENT_USER, kTiltedRegistryPath, L"TitleExe");

    std::error_code ec;
    if (aForceSelect || !HasGame(gamePath) || !fs::exists(exePath, ec))
    {
        // Found by itself on a normal install, so the game starts straight away; asks only when it can't find it
        gamePath = aForceSelect ? PickGameFolder() : FindGame();
        if (gamePath.empty())
            gamePath = PickGameFolder();
        if (gamePath.empty())
            return false;

        exePath = gamePath / kExeName;
        // Remembered for the next start; a failed write only means finding it again then
        Registry::WriteString<wchar_t>(HKEY_CURRENT_USER, kTiltedRegistryPath, L"TitlePath", gamePath.wstring());
        Registry::WriteString<wchar_t>(HKEY_CURRENT_USER, kTiltedRegistryPath, L"TitleExe", exePath.wstring());
    }

    auto* apContext = launcher::GetLaunchContext();
    apContext->gamePath = gamePath;
    apContext->exePath = exePath;
    return true;
}
} // namespace oobe
