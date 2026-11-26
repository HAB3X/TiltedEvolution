#include <Services/PluginChecksums.h>

#include <Games/TES.h>
#include <RequiredMods.h>
#include <immersive_launcher/launcher.h>

#include <zlib.h>

#include <chrono>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <vector>

namespace
{
using Checksums = std::map<std::string, uint32_t>;

std::once_flag s_started;
std::shared_future<Checksums> s_checksums;

uint32_t ReadChecksum(const std::filesystem::path& acPath)
{
    std::ifstream file(acPath, std::ios::binary);
    std::vector<char> buffer(1 << 20);
    uLong crc = crc32(0L, Z_NULL, 0);
    while (file)
    {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (file.gcount() > 0)
            crc = crc32(crc, reinterpret_cast<const Bytef*>(buffer.data()), static_cast<uInt>(file.gcount()));
    }
    if (!file.eof())
    {
        spdlog::warn("Can't read {} for its checksum: only its size is compared with the host's", acPath.filename().string());
        return 0;
    }
    return static_cast<uint32_t>(crc);
}
} // namespace

void PluginChecksums::Start() noexcept
{
    std::call_once(s_started,
                   []()
                   {
                       // The plugin list comes from the game, so it's read here, on its thread
                       std::vector<std::string> plugins;
                       for (auto* pMod : ModManager::Get()->mods)
                       {
                           if (pMod->IsLoaded() && !RequiredMods::IsGamePlugin(pMod->filename))
                               plugins.emplace_back(pMod->filename);
                       }
                       const std::filesystem::path cData = launcher::GetLaunchContext()->gamePath / L"Data";

                       s_checksums = std::async(std::launch::async,
                                                [plugins = std::move(plugins), cData]()
                                                {
                                                    const auto cStart = std::chrono::steady_clock::now();
                                                    Checksums checksums;
                                                    for (const std::string& cPlugin : plugins)
                                                        checksums[RequiredMods::Lower(cPlugin)] = ReadChecksum(cData / cPlugin);
                                                    spdlog::info("Read the checksums of {} plugins in {} ms", plugins.size(),
                                                                 std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - cStart).count());
                                                    return checksums;
                                                })
                                         .share();
                   });
}

bool PluginChecksums::IsReady() noexcept
{
    return s_checksums.valid() && s_checksums.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

uint32_t PluginChecksums::Get(std::string_view aFileName) noexcept
{
    Start();
    const Checksums& checksums = s_checksums.get();
    const auto cIt = checksums.find(RequiredMods::Lower(aFileName));
    return cIt == checksums.end() ? 0 : cIt->second;
}
