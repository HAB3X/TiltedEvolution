#pragma once

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
};

/**
 * @brief Hosting from inside the game: runs the Skyrim Together server that sits next to the client and connects to it.
 *
 * The server runs without a window and stops when the game exits (it watches the game). Its output goes to the
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

private:
    bool IsRunning() const noexcept;
    bool Start(const HostSettings& acSettings) noexcept;
    // The host's mods, for the server to require: written to its RequiredMods::kFileName
    bool WriteRequiredMods() const noexcept;
    void Connect(const std::string& acPassword) const noexcept;
    void Fail(const std::string& acReason) const noexcept;

    World& m_world;
    HANDLE m_process{};
    uint16_t m_port{};
};
