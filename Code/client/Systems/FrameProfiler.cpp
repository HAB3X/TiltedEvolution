#include <TiltedOnlinePCH.h>

#include <Systems/FrameProfiler.h>

#include <base/simpleini/SimpleIni.h>

#include <algorithm>
#include <array>
#include <mutex>

namespace FrameProfiler
{
namespace
{
constexpr double kReportSeconds = 10.0;
constexpr double kHitchMs = 33.4; // a frame that missed two 60 Hz refreshes
constexpr size_t kMaxSlots = 128;
constexpr size_t kMaxFrames = 4096;
constexpr size_t kTopScopes = 10;

std::array<Slot, kMaxSlots> s_slots{};
std::atomic<size_t> s_slotCount{0};
std::mutex s_registerLock;

bool ReadEnabled() noexcept
{
    CSimpleIniA ini;
    if (ini.LoadFile("Data/SkyrimTogetherReborn/config/STServer.ini") < 0)
        return true;
    return ini.GetBoolValue("Debug", "bFrameProfiler", true);
}

double Frequency() noexcept
{
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    return static_cast<double>(frequency.QuadPart);
}

// Main thread state
std::array<float, kMaxFrames> s_frameMs{};
size_t s_frameCount = 0;
uint64_t s_lastFrame = 0;
uint64_t s_reportStart = 0;
} // namespace

bool IsEnabled() noexcept
{
    static const bool s_enabled = ReadEnabled();
    return s_enabled;
}

uint64_t Now() noexcept
{
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}

Slot& Register(const char* acpName) noexcept
{
    std::scoped_lock lock(s_registerLock);
    const size_t count = s_slotCount.load();
    for (size_t i = 0; i < count; ++i)
    {
        if (std::strcmp(s_slots[i].Name, acpName) == 0)
            return s_slots[i];
    }
    // Out of slots: share the last one, its name says so
    if (count == kMaxSlots)
        return s_slots[kMaxSlots - 1];

    s_slots[count].Name = count == kMaxSlots - 1 ? "(other scopes)" : acpName;
    s_slotCount.store(count + 1);
    return s_slots[count];
}

void OnFrame() noexcept
{
    if (!IsEnabled())
        return;

    static const double s_msPerTick = 1000.0 / Frequency();
    const uint64_t now = Now();

    if (s_lastFrame == 0)
    {
        s_lastFrame = now;
        s_reportStart = now;
        return;
    }

    const float frameMs = static_cast<float>((now - s_lastFrame) * s_msPerTick);
    s_lastFrame = now;
    if (s_frameCount < kMaxFrames)
        s_frameMs[s_frameCount++] = frameMs;

    const double elapsedMs = (now - s_reportStart) * s_msPerTick;
    if (elapsedMs < kReportSeconds * 1000.0 || s_frameCount == 0)
        return;

    // Frame rate
    std::sort(s_frameMs.begin(), s_frameMs.begin() + s_frameCount);
    double totalMs = 0.0;
    size_t hitches = 0;
    for (size_t i = 0; i < s_frameCount; ++i)
    {
        totalMs += s_frameMs[i];
        hitches += s_frameMs[i] >= kHitchMs;
    }
    const size_t frames = s_frameCount;
    const double averageFps = frames * 1000.0 / totalMs;
    // 1% low: the frame rate of the slowest 1% of frames
    const size_t slowStart = frames - std::max<size_t>(1, frames / 100);
    double slowMs = 0.0;
    for (size_t i = slowStart; i < frames; ++i)
        slowMs += s_frameMs[i];
    const double lowFps = (frames - slowStart) * 1000.0 / slowMs;
    const float worstMs = s_frameMs[frames - 1];

    // Scopes, most expensive first
    struct Row
    {
        const char* Name;
        double MsPerFrame;
        double CallsPerFrame;
    };
    std::array<Row, kMaxSlots> rows{};
    const size_t slotCount = s_slotCount.load();
    double profiledMs = 0.0;
    for (size_t i = 0; i < slotCount; ++i)
    {
        const uint64_t ticks = s_slots[i].Ticks.exchange(0, std::memory_order_relaxed);
        const uint64_t calls = s_slots[i].Calls.exchange(0, std::memory_order_relaxed);
        rows[i] = {s_slots[i].Name, ticks * s_msPerTick / frames, static_cast<double>(calls) / frames};
    }
    std::sort(rows.begin(), rows.begin() + slotCount, [](const Row& a, const Row& b) { return a.MsPerFrame > b.MsPerFrame; });

    std::string top;
    for (size_t i = 0; i < std::min(kTopScopes, slotCount); ++i)
    {
        if (rows[i].MsPerFrame < 0.005)
            break;
        top += fmt::format("{}{} {:.2f}ms x{:.0f}", top.empty() ? "" : ", ", rows[i].Name, rows[i].MsPerFrame, rows[i].CallsPerFrame);
    }

    // "World::Update" holds every service update; hooks run inside the game's own frame
    for (size_t i = 0; i < slotCount; ++i)
    {
        if (std::strcmp(rows[i].Name, "World::Update") == 0)
            profiledMs = rows[i].MsPerFrame;
    }

    spdlog::info("[Perf] {:.0f} fps (1% low {:.0f}, worst {:.0f} ms, {} frames over {:.0f} ms) | frame {:.1f} ms, "
                 "Skyrim Together updates {:.2f} ms | {}",
                 averageFps, lowFps, worstMs, hitches, kHitchMs, totalMs / frames, profiledMs, top);

    s_frameCount = 0;
    s_reportStart = now;
}
} // namespace FrameProfiler
