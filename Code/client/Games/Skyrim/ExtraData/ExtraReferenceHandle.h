#pragma once

#include <ExtraData.h>

// An inventory entry's link to the reference it stands for. Handle offset: decompiled 1.6.1179 at 0x14016d5d0
// (ExtraDataList::HasQuestObjectAlias, ID 12052) reads it at +0x10.
struct ExtraReferenceHandle : BSExtraData
{
    inline static constexpr auto eExtraData = ExtraDataType::ReferenceHandle;

    virtual ~ExtraReferenceHandle();

    uint32_t handle;
};

static_assert(offsetof(ExtraReferenceHandle, handle) == 0x10);
