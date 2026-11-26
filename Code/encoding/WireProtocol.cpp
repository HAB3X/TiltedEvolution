#include <WireProtocol.h>

#include <atomic>
#include <iterator>

namespace
{
std::atomic<WireProtocol> s_protocol{WireProtocol::Current};

// Generated from v1.8.4's Code/encoding/Opcodes.h
// Official v1.8.4's ClientOpcodes in their order there: the index is the number on the wire
constexpr ClientOpcode kOfficial184Client[] = {
    kAuthenticationRequest,
    kAssignCharacterRequest,
    kClientReferencesMoveRequest,
    kEnterExteriorCellRequest,
    kEnterInteriorCellRequest,
    kRequestFactionsChanges,
    kRequestQuestUpdate,
    kPartyInviteRequest,
    kPartyAcceptInviteRequest,
    kPartyLeaveRequest,
    kPartyCreateRequest,
    kPartyChangeLeaderRequest,
    kPartyKickRequest,
    kRequestActorValueChanges,
    kRequestActorMaxValueChanges,
    kRequestHealthChangeBroadcast,
    kActivateRequest,
    kLockChangeRequest,
    kAssignObjectsRequest,
    kRequestDeathStateChange,
    kShiftGridCellRequest,
    kRequestOwnershipTransfer,
    kRequestOwnershipClaim,
    kRequestObjectInventoryChanges,
    kRequestInventoryChanges,
    kSpellCastRequest,
    kInterruptCastRequest,
    kAddTargetRequest,
    kRequestRemoveSpell,
    kProjectileLaunchRequest,
    kScriptAnimationRequest,
    kDrawWeaponRequest,
    kMountRequest,
    kNewPackageRequest,
    kRequestRespawn,
    kSyncExperienceRequest,
    kRequestEquipmentChanges,
    kSendChatMessageRequest,
    kTeleportCommandRequest,
    kPlayerRespawnRequest,
    kDialogueRequest,
    kSubtitleRequest,
    kPlayerDialogueRequest,
    kPlayerLevelRequest,
    kTeleportRequest,
    kRequestPlayerHealthUpdate,
    kRequestWeatherChange,
    kRequestCurrentWeather,
    kRequestSetWaypoint,
    kRequestRemoveWaypoint,
    kSetTimeCommandRequest,
    kRequestActionEvents,
};

// Official v1.8.4's ServerOpcodes in their order there: the index is the number on the wire
constexpr ServerOpcode kOfficial184Server[] = {
    kAuthenticationResponse,
    kAssignCharacterResponse,
    kServerReferencesMoveRequest,
    kServerTimeSettings,
    kCharacterSpawnRequest,
    kNotifyFactionsChanges,
    kNotifyRemoveCharacter,
    kNotifyQuestUpdate,
    kNotifyPlayerList,
    kNotifyPartyInfo,
    kNotifyPartyInvite,
    kNotifyPartyJoined,
    kNotifyPartyLeft,
    kNotifyActorValueChanges,
    kNotifyActorMaxValueChanges,
    kNotifyHealthChangeBroadcast,
    kNotifyActivate,
    kNotifyLockChange,
    kAssignObjectsResponse,
    kNotifyDeathStateChange,
    kNotifyOwnershipTransfer,
    kNotifyObjectInventoryChanges,
    kNotifyInventoryChanges,
    kNotifySpellCast,
    kNotifyInterruptCast,
    kNotifyAddTarget,
    kNotifyRemoveSpell,
    kNotifyProjectileLaunch,
    kNotifyScriptAnimation,
    kNotifyDrawWeapon,
    kNotifyMount,
    kNotifyNewPackage,
    kNotifyRespawn,
    kNotifySyncExperience,
    kNotifyEquipmentChanges,
    kNotifyChatMessageBroadcast,
    kTeleportCommandResponse,
    kStringCacheUpdate,
    kNotifyPlayerRespawn,
    kNotifyDialogue,
    kNotifySubtitle,
    kNotifyPlayerDialogue,
    kNotifyActorTeleport,
    kNotifyPlayerLeft,
    kNotifyPlayerJoined,
    kNotifyPlayerLevel,
    kNotifyPlayerCellChanged,
    kNotifyTeleport,
    kNotifyPlayerHealthUpdate,
    kNotifySettingsChange,
    kNotifyWeatherChange,
    kNotifySetWaypoint,
    kNotifyRemoveWaypoint,
    kNotifySetTimeResult,
    kNotifyActionEvents,
};

template <class T, size_t N> std::optional<uint8_t> IndexOf(const T (&acTable)[N], T aOpcode) noexcept
{
    for (size_t i = 0; i < N; ++i)
    {
        if (acTable[i] == aOpcode)
            return static_cast<uint8_t>(i);
    }
    return std::nullopt;
}

template <class T, size_t N> std::optional<T> At(const T (&acTable)[N], uint8_t aIndex) noexcept
{
    return aIndex < N ? std::optional<T>(acTable[aIndex]) : std::nullopt;
}
} // namespace

WireProtocol Wire::Get() noexcept
{
    return s_protocol;
}

void Wire::Set(WireProtocol aProtocol) noexcept
{
    s_protocol = aProtocol;
}

std::optional<uint8_t> Wire::ToWire(ClientOpcode aOpcode) noexcept
{
    if (!IsOfficial184())
        return static_cast<uint8_t>(aOpcode);
    return IndexOf(kOfficial184Client, aOpcode);
}

std::optional<ServerOpcode> Wire::FromWire(uint8_t aWireOpcode) noexcept
{
    if (!IsOfficial184())
        return aWireOpcode < kServerOpcodeMax ? std::optional<ServerOpcode>(static_cast<ServerOpcode>(aWireOpcode)) : std::nullopt;
    return At(kOfficial184Server, aWireOpcode);
}

std::optional<uint8_t> Wire::ToWire(ServerOpcode aOpcode) noexcept
{
    if (!IsOfficial184())
        return static_cast<uint8_t>(aOpcode);
    return IndexOf(kOfficial184Server, aOpcode);
}

std::optional<ClientOpcode> Wire::ClientFromWire(uint8_t aWireOpcode) noexcept
{
    if (!IsOfficial184())
        return aWireOpcode < kClientOpcodeMax ? std::optional<ClientOpcode>(static_cast<ClientOpcode>(aWireOpcode)) : std::nullopt;
    return At(kOfficial184Client, aWireOpcode);
}
