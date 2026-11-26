#pragma once

#include <Games/Primitives.h>

// What the crosshair is on. Layout from CommonLibSSE (include/RE/C/CrosshairPickData.h); only the reference handles
// are read here.
struct CrosshairPickData
{
    static CrosshairPickData* Get() noexcept;

    uint32_t pad00;
    uint32_t target;      // ObjectRefHandle of the reference the crosshair is on
    uint32_t targetActor; // ObjectRefHandle
    uint32_t grabPickRef; // ObjectRefHandle
    NiPoint3 collisionPoint;
};

static_assert(offsetof(CrosshairPickData, target) == 0x4);
static_assert(offsetof(CrosshairPickData, targetActor) == 0x8);
static_assert(offsetof(CrosshairPickData, collisionPoint) == 0x10);
