#pragma once

#include <Events/PacketEvent.h>
#include <Structs/ActorData.h>
#include <Structs/Inventory.h>

struct UpdateEvent;
struct CharacterInteriorCellChangeEvent;
struct CharacterSpawnedEvent;
struct World;
struct AssignCharacterRequest;
struct AssignCharacterResponse;
struct CharacterSpawnRequest;
struct ClientReferencesMoveRequest;
struct RequestActionEvents;
struct RequestFactionsChanges;
struct GridCellCoords;
struct RequestOwnershipTransfer;
struct CharacterRemoveEvent;
struct CharacterExteriorCellChangeEvent;
struct RequestOwnershipClaim;
struct RequestCrimeAuthority;
struct OwnershipTransferEvent;
struct MountRequest;
struct NewPackageRequest;
struct RequestRespawn;
struct SyncExperienceRequest;
struct DialogueRequest;
struct SubtitleRequest;
struct Player;

/**
 * @brief Manages player and actor state.
 */
struct CharacterService
{
    CharacterService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~CharacterService() noexcept = default;

    TP_NOCOPYMOVE(CharacterService);

    // apRecipient: who it is for, so a per player body shows their own loot (InventoryService::InventoryFor)
    static void Serialize(World& aRegistry, entt::entity aEntity, CharacterSpawnRequest* apSpawnRequest, const Player* apRecipient = nullptr) noexcept;

    // A killed NPC's loot: what everyone finds, and under per player loot each player's own copy
    struct DeadLoot
    {
        Inventory Content{};
        Map<String, Inventory> PlayerCopies{};
    };

    // NPCs killed whose entity is gone, with their loot (see m_deadActors): the world save keeps them between runs
    [[nodiscard]] const Map<GameId, DeadLoot>& GetDeadActors() const noexcept { return m_deadActors; }
    void RestoreDeadActors(Map<GameId, DeadLoot> aDeadActors) noexcept { m_deadActors = std::move(aDeadActors); }

protected:
    enum class OwnershipTransferReason : uint8_t
    {
        LeaderAssignment,
        LeaderClaim,
        Mount,
        Relinquish,
        OwnerUnavailable,
        // A player's game takes a guard to deal with that player's crime, and hands it back once the bounty is gone
        CrimeAuthority,
        CrimeAuthorityEnded
    };

    void OnUpdate(const UpdateEvent& acEvent) const noexcept;
    void OnCharacterExteriorCellChange(const CharacterExteriorCellChangeEvent& acEvent) const noexcept;
    void OnCharacterInteriorCellChange(const CharacterInteriorCellChangeEvent& acEvent) const noexcept;
    void OnAssignCharacterRequest(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept;
    void OnOwnershipTransferRequest(const PacketEvent<RequestOwnershipTransfer>& acMessage) const noexcept;
    void OnOwnershipTransferEvent(const OwnershipTransferEvent& acEvent) const noexcept;
    void OnOwnershipClaimRequest(const PacketEvent<RequestOwnershipClaim>& acMessage) const noexcept;
    void OnCrimeAuthorityRequest(const PacketEvent<RequestCrimeAuthority>& acMessage) const noexcept;
    void OnCharacterRemoveEvent(const CharacterRemoveEvent& acEvent) const noexcept;
    void OnCharacterSpawned(const CharacterSpawnedEvent& acEvent) const noexcept;
    void OnReferencesMoveRequest(const PacketEvent<ClientReferencesMoveRequest>& acMessage) const noexcept;
    void OnActionEventsRequest(const PacketEvent<RequestActionEvents>& acMessage) const noexcept;
    void OnFactionsChanges(const PacketEvent<RequestFactionsChanges>& acMessage) const noexcept;
    void OnMountRequest(const PacketEvent<MountRequest>& acMessage) const noexcept;
    void OnNewPackageRequest(const PacketEvent<NewPackageRequest>& acMessage) const noexcept;
    void OnRequestRespawn(const PacketEvent<RequestRespawn>& acMessage) const noexcept;
    void OnSyncExperienceRequest(const PacketEvent<SyncExperienceRequest>& acMessage) const noexcept;
    void OnDialogueRequest(const PacketEvent<DialogueRequest>& acMessage) const noexcept;
    void OnSubtitleRequest(const PacketEvent<SubtitleRequest>& acMessage) const noexcept;

    void CreateCharacter(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept;
    bool ShouldSuppressEncounter(const PacketEvent<AssignCharacterRequest>& acMessage) const noexcept;
    void PopulateAssignmentResponse(entt::entity aEntity, AssignCharacterResponse& aResponse, const Player& acRecipient) const noexcept;
    // Sends to the players in range but apExcluded, each shown their own loot when the character is a per player body
    void SendToPlayersInRangeShowingLoot(ServerMessage& aMessage, Inventory& aInventory, entt::entity aEntity, const Player* apExcluded) const noexcept;
    static const char* GetOwnershipTransferReasonName(OwnershipTransferReason aReason) noexcept;
    bool CanClaimOwnership(Player* apPlayer, entt::entity aEntity, uint32_t aExpectedOwnershipEpoch, OwnershipTransferReason aReason) const noexcept;
    bool TransferOwnership(Player* apPlayer, entt::entity aEntity, OwnershipTransferReason aReason, bool aResetInvalidOwners = true) const noexcept;
    void TransferToNextOwner(entt::entity aEntity, OwnershipTransferReason aReason) const noexcept;
    // Releases the guards apPlayer holds for their crime against aCrimeFaction, back to their party leader when near
    void EndCrimeAuthority(Player* apPlayer, const GameId& acCrimeFaction) const noexcept;
    ActorData BuildActorData(const entt::entity acEntity) const noexcept;

    void ProcessFactionsChanges() const noexcept;

private:
    World& m_world;

    // Dead actors whose server entity was destroyed, so a later loader gets them dead and with their looted inventory.
    // shortcut: lives until server restart, so an NPC respawned by a cell reset comes back dead; store game time to expire if it matters.
    mutable Map<GameId, DeadLoot> m_deadActors;

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_exteriorCellChangeEventConnection;
    entt::scoped_connection m_interiorCellChangeEventConnection;
    entt::scoped_connection m_characterAssignRequestConnection;
    entt::scoped_connection m_transferOwnershipConnection;
    entt::scoped_connection m_ownershipTransferEventConnection;
    entt::scoped_connection m_claimOwnershipConnection;
    entt::scoped_connection m_crimeAuthorityConnection;
    entt::scoped_connection m_removeCharacterConnection;
    entt::scoped_connection m_characterSpawnedConnection;
    entt::scoped_connection m_referenceMovementSnapshotConnection;
    entt::scoped_connection m_actionEventsConnection;
    entt::scoped_connection m_factionsChangesConnection;
    entt::scoped_connection m_mountConnection;
    entt::scoped_connection m_newPackageConnection;
    entt::scoped_connection m_requestRespawnConnection;
    entt::scoped_connection m_syncExperienceConnection;
    entt::scoped_connection m_dialogueConnection;
    entt::scoped_connection m_subtitleConnection;
};
