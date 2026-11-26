#pragma once

#include <include/internal/cef_ptr.h>

namespace TiltedPhoques
{
struct OverlayApp;
}

struct RenderSystemD3D11;
struct D3D11RenderProvider;
struct FormIdComponent;
struct World;
struct GameId;
struct Actor;
struct UpdateEvent;
struct ConnectedEvent;
struct DisconnectedEvent;
struct TransportService;
struct NotifyChatMessageBroadcast;
struct NotifyPlayerList;
struct NotifyPlayerJoined;
struct NotifyPlayerLeft;
struct NotifyPlayerDialogue;
struct ConnectionErrorEvent;
struct NotifyPlayerLevel;
struct NotifyPlayerCellChanged;
struct NotifyTeleport;
struct NotifyPlayerHealthUpdate;
struct NotifyVote;
enum ChatMessageTypes;
struct PartyJoinedEvent;
struct PartyLeftEvent;

using TiltedPhoques::OverlayApp;

/**
 * @brief Controls the UI from the client.
 */
struct OverlayService
{
    OverlayService(World& aWorld, TransportService& transport, entt::dispatcher& aDispatcher);
    ~OverlayService() noexcept;

    TP_NOCOPYMOVE(OverlayService);

    void Create(RenderSystemD3D11* apRenderSystem) noexcept;

    void Render() noexcept;
    void Reset() const noexcept;
    void Reload() noexcept;

    void Initialize() noexcept;

    void SetActive(bool aActive) noexcept;
    [[nodiscard]] bool GetActive() const noexcept;

    void SetInGame(bool aInGame) noexcept;
    [[nodiscard]] bool GetInGame() const noexcept;

    void SetVersion(const std::string& acVersion);

    OverlayApp* GetOverlayApp() const noexcept { return m_pOverlay.get(); }

    void SendSystemMessage(const std::string& acMessage);

    // Shows the error box with this message

    void SendError(const std::string& acMessage) const noexcept;

    // The text as a JSON string literal, for event arguments the overlay parses

    static std::string ToJsonString(const std::string& acText) noexcept;

    // The game's name for a worldspace, or for the cell when there's no worldspace (an interior); "UNKNOWN" when this
    // client has neither
    static TiltedPhoques::String GetCellName(const GameId& aWorldSpaceId, const GameId& aCellId) noexcept;

    void SetPlayerHealthPercentage(uint32_t aFormId) const noexcept;

    // The overlay's party bar is shown: members' distances are only worked out then
    void SetPartyBarEnabled(bool aEnabled) noexcept { m_partyBarEnabled = aEnabled; }
    // The overlay's performance line is shown: its numbers are only sent then
    void SetPerfOverlayEnabled(bool aEnabled) noexcept { m_perfOverlayEnabled = aEnabled; }

protected:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnConnectedEvent(const ConnectedEvent&) noexcept;
    void OnDisconnectedEvent(const DisconnectedEvent&) noexcept;
    void OnWaitingFor3DRemoved(entt::registry& aRegistry, entt::entity aEntity) const noexcept;
    void OnPlayerComponentRemoved(entt::registry& aRegistry, entt::entity aEntity) const noexcept;
    void OnConnectionError(const ConnectionErrorEvent& acConnectedEvent) const noexcept;
    void OnChatMessageReceived(const NotifyChatMessageBroadcast&) noexcept;
    void OnPlayerDialogue(const NotifyPlayerDialogue&) noexcept;
    void OnPlayerJoined(const NotifyPlayerJoined&) noexcept;
    void OnPlayerLeft(const NotifyPlayerLeft&) noexcept;
    void OnPlayerLevel(const NotifyPlayerLevel&) noexcept;
    void OnPlayerCellChanged(const NotifyPlayerCellChanged& acMessage) const noexcept;
    void OnNotifyTeleport(const NotifyTeleport& acMessage) noexcept;
    void OnNotifyPlayerHealthUpdate(const NotifyPlayerHealthUpdate& acMessage) noexcept;
    void OnNotifyVote(const NotifyVote& acMessage) noexcept;
    void OnPartyJoinedEvent(const PartyJoinedEvent& acEvent) noexcept;
    void OnPartyLeftEvent(const PartyLeftEvent& acEvent) noexcept;

private:
    void RunDebugDataUpdates() noexcept;
    void RunPlayerHealthUpdates() noexcept;
    void RunPartyDistanceUpdates() noexcept;
    void SendPerfData() noexcept;

    CefRefPtr<OverlayApp> m_pOverlay{nullptr};
    TiltedPhoques::UniquePtr<D3D11RenderProvider> m_pProvider;

    World& m_world;
    TransportService& m_transport;

    bool m_active = false;
    bool m_inGame = false;
    bool m_partyBarEnabled = true;
    bool m_perfOverlayEnabled = false;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_connectedConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_connectionErrorConnection;
    entt::scoped_connection m_cellChangeEventConnection;
    entt::scoped_connection m_chatMessageConnection;
    entt::scoped_connection m_playerDialogueConnection;
    entt::scoped_connection m_playerJoinedConnection;
    entt::scoped_connection m_playerLeftConnection;
    entt::scoped_connection m_playerAddedConnection;
    entt::scoped_connection m_playerRemovedConnection;
    entt::scoped_connection m_playerLevelConnection;
    entt::scoped_connection m_cellChangedConnection;
    entt::scoped_connection m_teleportConnection;
    entt::scoped_connection m_playerHealthConnection;
    entt::scoped_connection m_voteConnection;
    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_partyLeftConnection;
};
