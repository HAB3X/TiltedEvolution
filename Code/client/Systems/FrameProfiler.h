#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

// Cheap always-on frame profiler. Every report interval it logs the frame rate (average, 1% low, hitches) and how
// many milliseconds per frame each profiled scope of Skyrim Together took, so a play session tells where frame time
// goes on the player's own machine. Scopes cost two clock reads.
//
//   TP_PROFILE_SCOPE("NameTagService::OnUpdate");
//
// [Debug] bFrameProfiler=false in Data/SkyrimTogetherReborn/config/STServer.ini turns the reports off.
namespace FrameProfiler
{
struct Slot
{
    const char* Name;
    std::atomic<uint64_t> Ticks{0};
    std::atomic<uint64_t> Calls{0};
    // Never reset, for the per-second summary
    std::atomic<uint64_t> TotalTicks{0};
};

// Returns the slot for a scope name (one slot per call site, registered on first use)
Slot& Register(const char* acpName) noexcept;

[[nodiscard]] bool IsEnabled() noexcept;
[[nodiscard]] uint64_t Now() noexcept;

// Called once per game frame from the main thread; logs a report when the interval is over
void OnFrame() noexcept;

// The last whole second, for the overlay's performance line; empty while the profiler is off or before a second passed
struct Summary
{
    float Fps;
    float FrameMs;
    // Milliseconds per frame in Skyrim Together's service updates ("World::Update")
    float ModMs;
};
[[nodiscard]] std::optional<Summary> LastSecond() noexcept;

struct Scope
{
    explicit Scope(Slot& aSlot) noexcept
        : m_pSlot(IsEnabled() ? &aSlot : nullptr)
        , m_start(m_pSlot ? Now() : 0)
    {
    }

    ~Scope() noexcept
    {
        if (!m_pSlot)
            return;
        const uint64_t cTicks = Now() - m_start;
        m_pSlot->Ticks.fetch_add(cTicks, std::memory_order_relaxed);
        m_pSlot->Calls.fetch_add(1, std::memory_order_relaxed);
        m_pSlot->TotalTicks.fetch_add(cTicks, std::memory_order_relaxed);
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Slot* m_pSlot;
    uint64_t m_start;
};
} // namespace FrameProfiler

#define TP_PROFILE_CONCAT_INNER(a, b) a##b
#define TP_PROFILE_CONCAT(a, b) TP_PROFILE_CONCAT_INNER(a, b)
#define TP_PROFILE_SCOPE(name)                                                                                         \
    static FrameProfiler::Slot& TP_PROFILE_CONCAT(s_profileSlot, __LINE__) = FrameProfiler::Register(name);            \
    const FrameProfiler::Scope TP_PROFILE_CONCAT(profileScope, __LINE__)(TP_PROFILE_CONCAT(s_profileSlot, __LINE__))
