#pragma once

// Finds Skyrim Together servers on the local network (encoding/LanDiscovery.h) in the background and sends the list
// to the overlay as its lanServers event
namespace LanSearch
{
void Start() noexcept;
}
