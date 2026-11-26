#pragma once

#include <Forms/TESForm.h>
#include <Components/TESFullName.h>
#include <BSCore/BSTHashMap.h>
#include <Games/Primitives.h>

struct TESObjectREFR;
struct BGSListForm;
struct BGSOutfit;
struct TESNPC;

struct TESReactionForm : BaseFormComponent
{
    GameValueList<void*> reactions;
    uint8_t groupFormType;
    uint8_t pad19;
    uint16_t pad1A;
    uint32_t pad1C;
};

static_assert(sizeof(TESReactionForm) == 0x20);
static_assert(offsetof(TESReactionForm, reactions) == 0x08);
static_assert(offsetof(TESReactionForm, groupFormType) == 0x18);

struct TESFaction : TESForm
{
    // Where the game sends the player's belongings when they are arrested by this faction.
    struct CrimeData
    {
        TESObjectREFR* jailMarker;               // JAIL
        TESObjectREFR* waitMarker;               // WAIT
        TESObjectREFR* stolenGoodsContainer;     // STOL: stolen items taken on arrest
        TESObjectREFR* playerInventoryContainer; // PLCN: the rest of the player's inventory, returned on release
        BGSListForm* crimeGroup;                 // CRGR
        BGSOutfit* jailOutfit;                   // JOUT
        uint8_t crimeValues[0x14];               // CRVA
        uint32_t pad44;
    };

    // A merchant faction's trade (VENV, VENC). Layout from CommonLibSSE's FACTION_VENDOR_DATA (RE/T/TESFaction.h).
    struct VendorData
    {
        uint8_t vendorValues[0xC]; // VENV: hours, radius, what they buy
        uint32_t pad0C;
        void* vendorLocation;           // PLVD
        void* vendorConditions;
        BGSListForm* vendorSellBuyList; // VEND
        TESObjectREFR* merchantContainer; // VENC: the merchant's stock and gold
        uint32_t lastDayReset;
        uint32_t pad34;
    };

    TESFullName fullname;
    TESReactionForm reactionForm;
    creation::BSTHashMap<const TESNPC*, uint32_t>* crimeGoldMap;
    uint32_t factionFlags;      // DATA
    uint32_t pad5C;
    CrimeData crimeData;
    VendorData vendorData;
    // Ranks, crime counts, and timestamps are not accessed here.
    uint8_t padE0[0x100 - 0xE0];
};

static_assert(sizeof(TESFaction::CrimeData) == 0x48);
static_assert(sizeof(TESFaction::VendorData) == 0x38);
static_assert(offsetof(TESFaction, vendorData) == 0xA8);
static_assert(offsetof(TESFaction, vendorData.merchantContainer) == 0xD0);
static_assert(sizeof(TESFaction) == 0x100);
static_assert(offsetof(TESFaction, fullname) == 0x20);
static_assert(offsetof(TESFaction, reactionForm) == 0x30);
static_assert(offsetof(TESFaction, crimeGoldMap) == 0x50);
static_assert(offsetof(TESFaction, factionFlags) == 0x58);
static_assert(offsetof(TESFaction, crimeData) == 0x60);
static_assert(offsetof(TESFaction, crimeData.stolenGoodsContainer) == 0x70);
static_assert(offsetof(TESFaction, crimeData.playerInventoryContainer) == 0x78);
