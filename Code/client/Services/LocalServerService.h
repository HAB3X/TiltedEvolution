#pragma once

#include <RequiredMods.h>

struct World;

// What the host chose in Host a game
struct HostSettings
{
    std::string Password;
    // Joiners take over the host's world (Gameplay:bHostWorld)
    bool HostWorld{};
    // Joiners need the host's mods (ModPolicy:bRequireMods)
    bool RequireMods{};
    // Players can hurt each other (Gameplay:bEnablePvp)
    bool Pvp{};
    // The quest categories the party shares, SharedQuests names (Gameplay:sSharedQuestTypes)
    std::string SharedQuestTypes;
    // The world pauses while every player is in a menu (Gameplay:bPauseWhenAllInMenus)
    bool PauseInMenus{};
    // Runs the official-protocol server in the classic folder instead, so players with the regular Skyrim Together
    // v1.8.4 can join; the Plus-only settings above (host world, required mods, shared quests, pause) don't exist there
    bool Classic{};
    // A relay friends join through when the router can't be opened, address:port, and its password (GameServer:sRelay,
    // sRelayPassword); empty for none. Plus servers only
    std::string Relay;
    std::string RelayPassword;
    // Each player gets their own copy of what is in containers instead of first come first served
    // (Gameplay:bPerPlayerLoot). Plus servers only
    bool PerPlayerLoot{};
};

/**
 * @brief Hosting from inside the game: runs the Skyrim Together server that sits next to the client and connects to it.
 *
 * Plus games run the server next to the client, classic games the official-protocol one in its classic folder (the
 * client then joins it as official v1.8.4, see TransportService). The server runs without a window and stops when the game exits (it watches the game). Its output goes to the
 * client log.
 */
struct LocalServerService
{
    explicit LocalServerService(World& aWorld) noexcept;
    ~LocalServerService() noexcept;

    TP_NOCOPYMOVE(LocalServerService);

    // Starts the server with these settings (once per game session), then connects to it when it's up
    void Host(const HostSettings& acSettings) noexcept;

    // A [Host] line of the server: shown in chat, and its public address sent to the overlay for the invite
    void OnHostMessage(const std::string& acMessage) noexcept;

    // Sends the overlay the mods friends need when the host requires theirs (a hostMods event with their names)
    void SendHostMods() const noexcept;

private:
    // The host's loaded plugins that friends can be required to have (not the game's own), with their sizes
    static std::vector<RequiredMods::Listed> HostMods() noexcept;
    bool IsRunning() const noexcept;
    bool Start(const HostSettings& acSettings) noexcept;
    // The host's mods, for the server to require: written to its RequiredMods::kFileName
    bool WriteRequiredMods() const noexcept;
    void Connect(const std::string& acPassword) const noexcept;
    void Fail(const std::string& acReason) const noexcept;

    World& m_world;
    HANDLE m_process{};
    uint16_t m_port{};
    // Which server m_process is
    bool m_classic{};
};
