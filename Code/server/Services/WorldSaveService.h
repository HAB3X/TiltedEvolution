#pragma once

struct World;
struct UpdateEvent;

/**
 * @brief Keeps a host world's shared state between server runs (Gameplay:bKeepWorld), so the party can continue later.
 *
 * What the server holds of the world itself: containers (what players stored or took) and NPCs killed while nobody
 * had them loaded, with their loot; and the plugin ids those refer to. Quests, the date and everything else come from
 * the host's own save. Written to world/<sKeepWorldName>.dat (format: Game/WorldSave.h) every uKeepWorldSeconds and when
 * the server stops, keeping the three before as <name>.1.dat to <name>.3.dat; loaded when the server starts. Hosting
 * from the game names it after the host's character, so another playthrough doesn't get this one's world.
 */
struct WorldSaveService
{
    WorldSaveService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~WorldSaveService() noexcept = default;

    TP_NOCOPYMOVE(WorldSaveService);

    // Writes the world now, when this server keeps it
    void Save() noexcept;

private:
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    bool IsActive() const noexcept;
    void Load() noexcept;

    World& m_world;
    uint64_t m_nextSave{};
    entt::scoped_connection m_updateConnection;
};
