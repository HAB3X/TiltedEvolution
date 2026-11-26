#pragma once

/**
 * @brief Dispatched when the client disconnects from the server.
 */
struct DisconnectedEvent
{
    explicit DisconnectedEvent(bool aUnexpected = false)
        : Unexpected(aUnexpected)
    {
    }

    // The connection was lost (a timeout or a network problem), not ended by the player or the server
    bool Unexpected;
};
