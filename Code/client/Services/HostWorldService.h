#pragma once

#include <Structs/HostWorldState.h>

#include <Messages/NotifyHostWorldState.h>
#include <Messages/TeleportCommandResponse.h>

#include <chrono>
#include <optional>

struct World;
struct TransportService;
struct PartyJoinedEvent;
struct UpdateEvent;
struct NotifyHostWorldRequested;
struct NotifyHostWorldState;
struct TeleportCommandResponse;

/**
 * @brief Host world mode (the server's Gameplay:bHostWorld): a player who joins takes over the host's world.
 *
 * The host is the party leader. On joining the party, a member asks for the host's world; the host's game collects
 * its quests, cleared locations and dead unique NPCs, and the member's game is made to match, after which the server
 * moves them to the host. From then on the regular party sync keeps the two games together.
 *
 * The member's own world is saved first, as BeforeHostWorld_<character>, so that their newest save isn't the only one
 * and going back to their own world is one load away.
 */
struct HostWorldService
{
    HostWorldService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransportService) noexcept;
    ~HostWorldService() = default;

    TP_NOCOPYMOVE(HostWorldService);

protected:
    void OnPartyJoined(const PartyJoinedEvent& acEvent) noexcept;
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnHostWorldRequested(const NotifyHostWorldRequested& acMessage) noexcept;
    void OnHostWorldState(const NotifyHostWorldState& acMessage) noexcept;

public:
    // The joiner's answer to the offer: join the host's world (saving their own first), or keep theirs and leave
    void Answer(bool aJoin) noexcept;
    // While the offer is open, the server's move to the host waits for the answer: true when held
    bool HoldTeleport(const TeleportCommandResponse& acTeleport) noexcept;

protected:

private:
    enum class Mode
    {
        // Only count what would change, for the offer
        Count,
        Apply
    };

    // Tells the overlay how far joining the host's world got (its hostWorldSync event): requesting, received, saving,
    // applying, travelling, done or cancelled, with the counts of what changed once applied
    void ReportSync(const char* apStep, uint32_t aQuests = 0, uint32_t aPlaces = 0, uint32_t aPeople = 0) const noexcept;
    HostWorldState CollectWorld() const noexcept;
    void RequestWorld() noexcept;
    void ApplyPending() noexcept;
    // Each returns how many things it changes (or would) here
    uint32_t ApplyQuests(const HostWorldState& acState, Mode aMode) noexcept;
    uint32_t ApplyLocations(const HostWorldState& acState, Mode aMode) const noexcept;
    uint32_t ApplyUniqueActorDeaths(const HostWorldState& acState, Mode aMode) const noexcept;

    World& m_world;
    TransportService& m_transport;

    // The host's world, offered and not answered or applied yet
    std::optional<NotifyHostWorldState> m_pending;
    std::optional<TeleportCommandResponse> m_heldTeleport;
    // Waiting for the save of the member's own world, up to this time
    bool m_savingOwnWorld{false};
    std::chrono::steady_clock::time_point m_ownWorldSaveDeadline{};
    // The host's world was applied in this game session: saving again would save it as the member's own
    bool m_appliedThisSession{false};

    entt::scoped_connection m_partyJoinedConnection;
    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_hostWorldRequestedConnection;
    entt::scoped_connection m_hostWorldStateConnection;
};
