#pragma once

#include <Structs/Movement.h>

#include <algorithm>
#include <cstdint>

/**
 * @brief Leaves out the movement snapshots of an actor standing still, most NPCs in a town most of the time.
 *
 * Receivers keep moving an actor the way its last two snapshots went while the next is late (see InterpolationSystem),
 * so a still actor is sent twice the same before snapshots stop: from two identical ones it holds still. A snapshot
 * still goes out every kKeepAliveInterval, for players coming into range and for a lost last packet.
 */
struct MovementSendFilter
{
    static constexpr uint64_t kKeepAliveInterval = 1000;
    static constexpr uint32_t kSendsBeforeSkipping = 2;

    // Whether the snapshot aMovement, captured at aTick (ms), should be sent
    bool ShouldSend(const Movement& acMovement, const uint64_t aTick) noexcept
    {
        if (acMovement != m_last)
        {
            m_last = acMovement;
            m_identicalSends = 0;
        }
        else if (m_identicalSends >= kSendsBeforeSkipping && aTick < m_lastSentTick + kKeepAliveInterval)
            return false;

        m_identicalSends = std::min(m_identicalSends + 1, kSendsBeforeSkipping);
        m_lastSentTick = aTick;
        return true;
    }

private:
    Movement m_last{};
    uint32_t m_identicalSends{0};
    uint64_t m_lastSentTick{0};
};
