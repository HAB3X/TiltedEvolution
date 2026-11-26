#pragma once

#include <cstdint>
#include "AnimationVariables.h"
#include "CachedString.h"

using TiltedPhoques::String;

struct ActionEvent
{
    // Type bit: TargetId is the target actor's server id, not a form ID. The same form ID is another actor in each game
    // (0x14 is every game's own player, other players are temporary references), so synced actors go by server id.
    static constexpr uint32_t kTargetIsServerId = 1 << 3;

    uint64_t Tick{0};
    uint32_t ActorId{0};
    uint32_t ActionId{0};
    uint32_t TargetId{0};
    uint32_t IdleId{0};
    uint32_t State1{0};
    uint32_t State2{0};
    uint32_t Type{0};
    CachedString EventName{};
    CachedString TargetEventName{};
    AnimationVariables Variables{};

    bool operator==(const ActionEvent& acRhs) const noexcept;
    bool operator!=(const ActionEvent& acRhs) const noexcept;

    void GenerateDifferential(const ActionEvent& aPrevious, TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void ApplyDifferential(TiltedPhoques::Buffer::Reader& aReader) noexcept;
};
