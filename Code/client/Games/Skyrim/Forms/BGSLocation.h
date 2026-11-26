#pragma once

#include <Forms/TESForm.h>

struct Actor;

// Layout from CommonLibSSE (RE/B/BGSLocation.h), the same in SE and AE
struct BGSLocation : TESForm
{
    enum ChangeFlags : uint32_t
    {
        CHANGE_CLEARED = 1u << 31,
    };

    // A unique NPC that lives in this location (LCUN)
    struct UniqueNPC
    {
        Actor* actor;
        uint32_t refId;
        uint32_t pad0C;
        BGSLocation* editorLocation;
    };

    // TESFullName at 0x20, BGSKeywordForm at 0x30, then the location's own data
    uint8_t pad20[0x88 - 0x20];
    GameArray<UniqueNPC> uniqueNPCs;
    uint8_t padA0[0xEC - 0xA0];
    bool cleared;
    bool everCleared;
    uint16_t padEE;
};

static_assert(sizeof(BGSLocation::UniqueNPC) == 0x18);
static_assert(offsetof(BGSLocation, uniqueNPCs) == 0x88);
static_assert(offsetof(BGSLocation, cleared) == 0xEC);
static_assert(sizeof(BGSLocation) == 0xF0);
