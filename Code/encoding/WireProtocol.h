#pragma once

#include <Opcodes.h>

#include <cstdint>
#include <optional>

// The protocol a client speaks with its server: this build's, or official Skyrim Together v1.8.4's, so that this build
// can join official v1.8.4 servers (and our base mac release's, which speaks it). Official v1.8.4 numbers its messages
// differently (later builds inserted the inventory snapshot messages) and lacks fields and messages added since; the
// messages that changed read and write it here. Set by the client's TransportService for its one connection; servers
// always speak Current.
enum class WireProtocol : uint8_t
{
    Current,
    Official184
};

namespace Wire
{
// The version an official v1.8.4 server checks for and answers a wrong version with
constexpr char kOfficial184Version[] = "v1.8.4";

WireProtocol Get() noexcept;
void Set(WireProtocol aProtocol) noexcept;
inline bool IsOfficial184() noexcept { return Get() == WireProtocol::Official184; }

// The number this client message has on the wire, empty when the server's protocol has no such message
std::optional<uint8_t> ToWire(ClientOpcode aOpcode) noexcept;
// This build's server message for a number read off the wire, empty for an unknown one
std::optional<ServerOpcode> FromWire(uint8_t aWireOpcode) noexcept;
// Both the other way, for the server side of tests
std::optional<uint8_t> ToWire(ServerOpcode aOpcode) noexcept;
std::optional<ClientOpcode> ClientFromWire(uint8_t aWireOpcode) noexcept;
} // namespace Wire
