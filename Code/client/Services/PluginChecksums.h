#pragma once

#include <cstdint>
#include <string_view>

// CRC-32 of the loaded plugins' files, so a server requiring the host's mods can tell apart versions of a mod with the
// same size (RequiredMods). Reading every plugin takes a few seconds, so it happens once per game session on a
// background thread, started the first time anything needs it.
namespace PluginChecksums
{
// Starts reading the loaded plugins (not Bethesda's own, which are never required); call on the game thread
void Start() noexcept;
// Whether all are read, without waiting
bool IsReady() noexcept;
// The plugin's CRC-32, waiting for the reading if it isn't done; 0 for one that couldn't be read (logged) or isn't loaded
uint32_t Get(std::string_view aFileName) noexcept;
} // namespace PluginChecksums
