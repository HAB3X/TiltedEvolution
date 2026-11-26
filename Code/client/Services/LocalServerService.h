#pragma once

struct World;

/**
 * @brief Hosting from inside the game: runs the Skyrim Together server that sits next to the client and connects to it.
 *
 * The server runs without a window and is tied to the game: it stops when the game exits. Its output goes to the
 * client log.
 */
struct LocalServerService
{
    explicit LocalServerService(World& aWorld) noexcept;
    ~LocalServerService() noexcept;

    TP_NOCOPYMOVE(LocalServerService);

    // Starts the server with this password (once per game session), then connects to it when it's up
    void Host(const std::string& acPassword, bool aHostWorld) noexcept;

    // A [Host] line of the server: shown in chat, and its public address sent to the overlay for the invite
    void OnHostMessage(const std::string& acMessage) noexcept;

private:
    bool IsRunning() const noexcept;
    bool Start(const std::string& acPassword, bool aHostWorld) noexcept;
    void Connect(const std::string& acPassword) const noexcept;
    void Fail(const std::string& acReason) const noexcept;

    World& m_world;
    // Kills the server when the game exits, however it exits
    HANDLE m_job{};
    HANDLE m_process{};
    uint16_t m_port{};
};
