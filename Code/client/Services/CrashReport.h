#pragma once

/**
 * @brief Offering to report the last crash: the crash handler leaves a crash_UTC_<time>.dmp next to SkyrimTogether.exe
 * (only the newest is kept). The overlay asks once per dump whether to open a pre-filled GitHub issue and tells the
 * player which file to attach; nothing is uploaded.
 */
namespace CrashReport
{
// Sends the overlay a crashDump event (file name, folder, version, Skyrim version, system) when a dump exists
void Send() noexcept;
} // namespace CrashReport
