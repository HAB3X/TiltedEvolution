#pragma once

#include <ExtraData.h>
#include <BSCore/BSAtomic.h>

struct TESQuest;

// Layouts from CommonLibSSE (RE/B/BGSBaseAlias.h, RE/E/ExtraAliasInstanceArray.h). GetQuestObjectQuest walks the
// aliases as the game's own check does (decompiled 1.6.1179 at 0x140182f70, called by ExtraDataList::HasQuestObjectAlias,
// ID 12052): read lock at +0x28, array at +0x10 with its length at +0x20, each instance's alias at +0x08 and its flags'
// bit 2 (kQuestObject) at +0x1C.
struct BGSBaseAlias
{
    enum Flags : uint32_t
    {
        kQuestObject = 1 << 2,
    };

    virtual ~BGSBaseAlias();

    const char* aliasName;
    TESQuest* owningQuest;
    uint32_t aliasID;
    uint32_t flags;
    uint16_t fillType;
    uint16_t pad22;
    uint32_t pad24;
};

static_assert(offsetof(BGSBaseAlias, owningQuest) == 0x10);
static_assert(offsetof(BGSBaseAlias, flags) == 0x1C);
static_assert(sizeof(BGSBaseAlias) == 0x28);

struct ExtraAliasInstanceArray : BSExtraData
{
    inline static constexpr auto eExtraData = ExtraDataType::AliasInstanceArray;

    struct Instance
    {
        TESQuest* quest;
        const BGSBaseAlias* alias;
        const void* instancedPackages;
    };

    virtual ~ExtraAliasInstanceArray();

    GameArray<Instance*> aliases;
    mutable BSReadWriteLock lock;

    // The quest of the first alias that makes this a quest item, or null
    [[nodiscard]] TESQuest* GetQuestObjectQuest() const noexcept
    {
        lock.LockForRead();
        TESQuest* pQuest = nullptr;
        for (const Instance* pInstance : aliases)
        {
            if (pInstance && pInstance->alias && (pInstance->alias->flags & BGSBaseAlias::kQuestObject))
            {
                pQuest = pInstance->quest;
                break;
            }
        }
        lock.UnlockRead();
        return pQuest;
    }
};

static_assert(sizeof(ExtraAliasInstanceArray::Instance) == 0x18);
static_assert(offsetof(ExtraAliasInstanceArray, aliases) == 0x10);
static_assert(offsetof(ExtraAliasInstanceArray, lock) == 0x28);
static_assert(sizeof(ExtraAliasInstanceArray) == 0x30);
