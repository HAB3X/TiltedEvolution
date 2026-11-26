#include <Services/MapService.h>

#include <World.h>

#include <Events/SetWaypointEvent.h>
#include <Events/RemoveWaypointEvent.h>
#include <Messages/NotifySetWaypoint.h>
#include <Messages/NotifyRemoveWaypoint.h>
#include <Messages/RequestSetWaypoint.h>
#include <Messages/RequestRemoveWaypoint.h>
#include <Messages/RequestPartyPing.h>
#include <Messages/NotifyPartyPing.h>
#include <Events/UpdateEvent.h>

#include <Services/OverlayService.h>
#include <Services/PartyService.h>
#include <Services/TransportService.h>
#include <OverlayApp.hpp>
#include <PartyCompass.h>

#include <BSGraphics/BSGraphicsRenderer.h>
#include <Games/Misc/CrosshairPickData.h>

#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>

MapService::MapService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld), m_dispatcher(aDispatcher), m_transport(aTransport)
{
    m_playerNotifySetWaypointConnection =
        m_dispatcher.sink<NotifySetWaypoint>().connect<&MapService::OnNotifySetWaypoint>(this);
    m_playerNotifyRemoveWaypointConnection =
        m_dispatcher.sink<NotifyRemoveWaypoint>().connect<&MapService::OnNotifyRemoveWaypoint>(this);
    m_playerSetWaypointConnection =
        m_dispatcher.sink<SetWaypointEvent>().connect<&MapService::OnSetWaypoint>(this);
    m_playerRemoveWaypointConnection =
        m_dispatcher.sink<RemoveWaypointEvent>().connect<&MapService::OnRemoveWaypoint>(this);
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&MapService::OnUpdate>(this);
    m_partyPingConnection = m_dispatcher.sink<NotifyPartyPing>().connect<&MapService::OnNotifyPartyPing>(this);
}

void MapService::OnSetWaypoint(const SetWaypointEvent& acMessage) noexcept
{
    if (!m_transport.IsConnected())
        return;

    RequestSetWaypoint request{};
    request.Position = acMessage.Position;

    ModSystem& modSystem = m_world.Get().GetModSystem();
    if (!modSystem.GetServerModId(acMessage.WorldSpaceFormID, request.WorldSpaceFormID))
        return;

    m_transport.Send(request);
}

void MapService::OnRemoveWaypoint(const RemoveWaypointEvent& acMessage) noexcept
{
    if (!m_transport.IsConnected())
        return;

    RequestRemoveWaypoint request{};
    m_transport.Send(request);
}

void MapService::OnNotifySetWaypoint(const NotifySetWaypoint& acMessage) noexcept
{
    NiPoint3 pos{};
    pos.x = acMessage.Position.x;
    pos.y = acMessage.Position.y;
    pos.z = acMessage.Position.z;

    ModSystem& modSystem = m_world.Get().GetModSystem();
    const uint32_t cWorldSpaceID = modSystem.GetGameId(acMessage.WorldSpaceFormID);

    // The game takes the map's worldspace, or its cell on an interior map, and reads its form type without a null
    // check, so a waypoint whose location this client doesn't have must be skipped.
    TESForm* pLocation = TESForm::GetById(cWorldSpaceID);
    if (!Cast<TESWorldSpace>(pLocation) && !Cast<TESObjectCELL>(pLocation))
    {
        spdlog::warn("{}: no worldspace or cell {:X} for the waypoint, skipping it", __FUNCTION__, cWorldSpaceID);
        return;
    }

    PlayerCharacter::Get()->SetWaypoint(&pos, reinterpret_cast<TESWorldSpace*>(pLocation));
}

void MapService::OnNotifyRemoveWaypoint(const NotifyRemoveWaypoint& acMessage) noexcept
{
    PlayerCharacter::Get()->RemoveWaypoint();
}


namespace
{
constexpr int kPingKey = VK_F3;
// Long enough that holding or mashing the key doesn't flood the party's chat
constexpr auto kPingCooldown = 2s;

// The worldspace a waypoint goes on, or the interior cell when there's no worldspace; nullptr when this client has
// neither. The game reads the form type without a null check, so a ping without one gets no waypoint.
TESForm* WaypointLocation(const GameId& acWorldSpaceId, const GameId& acCellId) noexcept
{
    ModSystem& modSystem = World::Get().GetModSystem();
    const bool cExterior = acWorldSpaceId != GameId{};
    TESForm* pLocation = TESForm::GetById(modSystem.GetGameId(cExterior ? acWorldSpaceId : acCellId));
    if (cExterior ? !Cast<TESWorldSpace>(pLocation) : !Cast<TESObjectCELL>(pLocation))
        return nullptr;
    return pLocation;
}

// "partyPing" for the overlay: who pinged (empty for this player), what, and how far and which way it is from this
// player (-1 metres when it's in another worldspace or cell, then the place's name says where)
void ShowPing(const String& acPlayer, const String& acTarget, const GameId& acWorldSpaceId, const GameId& acCellId, const NiPoint3& acPosition) noexcept
{
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    ModSystem& modSystem = World::Get().GetModSystem();
    const bool cExterior = acWorldSpaceId != GameId{};
    const bool cHere = cExterior ? pPlayer->GetWorldSpace() && pPlayer->GetWorldSpace()->formID == modSystem.GetGameId(acWorldSpaceId)
                                 : !pPlayer->GetWorldSpace() && pPlayer->parentCell && pPlayer->parentCell->formID == modSystem.GetGameId(acCellId);

    const NiPoint3& cFrom = pPlayer->position;
    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, acPlayer.c_str());
    pArguments->SetString(1, acTarget.c_str());
    pArguments->SetInt(2, cHere ? static_cast<int>(std::lround(PartyCompass::ToMeters(PartyCompass::Distance(cFrom.x, cFrom.y, cFrom.z, acPosition.x, acPosition.y, acPosition.z)))) : -1);
    pArguments->SetInt(3, PartyCompass::Toward(cFrom.x, cFrom.y, acPosition.x, acPosition.y));
    pArguments->SetString(4, OverlayService::GetCellName(acWorldSpaceId, acCellId).c_str());
    World::Get().GetOverlayService().GetOverlayApp()->ExecuteAsync("partyPing", pArguments);
}

void SetPingWaypoint(const GameId& acWorldSpaceId, const GameId& acCellId, NiPoint3 aPosition) noexcept
{
    TESForm* pLocation = WaypointLocation(acWorldSpaceId, acCellId);
    if (!pLocation)
    {
        spdlog::warn("[Ping]: no worldspace {:X}:{:X} or cell {:X}:{:X} here, no waypoint", acWorldSpaceId.ModId, acWorldSpaceId.BaseId, acCellId.ModId, acCellId.BaseId);
        return;
    }
    // The original function, not the hook: a ping's waypoint isn't shared again as the player's own
    PlayerCharacter::Get()->SetWaypoint(&aPosition, reinterpret_cast<TESWorldSpace*>(pLocation));
}
} // namespace

void MapService::OnUpdate(const UpdateEvent&) noexcept
{
    // Read the key every frame so its "pressed since the last call" bit can't go stale, but only act on it when this
    // window has focus (as MagicService does for F4), so one press doesn't ping from two clients side by side
    const bool cPressed = (GetAsyncKeyState(kPingKey) & 0x01) != 0;
    if (!cPressed)
        return;

    auto* pWindow = BSGraphics::GetMainWindow();
    if (pWindow && pWindow->IsForeground())
        Ping();
}

void MapService::Ping() noexcept
{
    if (!m_transport.IsConnected() || !m_world.GetPartyService().IsInParty())
        return;

    const auto cNow = std::chrono::steady_clock::now();
    if (cNow - m_lastPing < kPingCooldown)
        return;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer->parentCell)
        return;

    TESObjectREFR* pTarget = nullptr;
    if (const CrosshairPickData* pPick = CrosshairPickData::Get())
    {
        pTarget = pPick->target ? TESObjectREFR::GetByHandle(pPick->target) : nullptr;
        if (!pTarget && pPick->targetActor)
            pTarget = TESObjectREFR::GetByHandle(pPick->targetActor);
    }

    RequestPartyPing request{};
    ModSystem& modSystem = m_world.GetModSystem();
    if (TESWorldSpace* pWorldSpace = pPlayer->GetWorldSpace())
    {
        if (!modSystem.GetServerModId(pWorldSpace->formID, request.WorldSpaceId))
            return;
    }
    if (!modSystem.GetServerModId(pPlayer->parentCell->formID, request.CellId))
        return;

    const NiPoint3 cPosition = pTarget ? pTarget->position : pPlayer->position;
    request.Position = cPosition;
    if (pTarget && pTarget->baseForm && pTarget->baseForm->GetName())
        request.Target = pTarget->baseForm->GetName();

    m_lastPing = cNow;
    spdlog::info("[Ping]: '{}' at ({:.0f}, {:.0f}, {:.0f})", request.Target.c_str(), cPosition.x, cPosition.y, cPosition.z);
    m_transport.Send(request);

    SetPingWaypoint(request.WorldSpaceId, request.CellId, cPosition);
    ShowPing({}, request.Target, request.WorldSpaceId, request.CellId, cPosition);
}

void MapService::OnNotifyPartyPing(const NotifyPartyPing& acMessage) const noexcept
{
    const NiPoint3 cPosition(acMessage.Position);
    if (m_pingMarkersEnabled)
        SetPingWaypoint(acMessage.WorldSpaceId, acMessage.CellId, cPosition);
    ShowPing(acMessage.PlayerName, acMessage.Target, acMessage.WorldSpaceId, acMessage.CellId, cPosition);
}
