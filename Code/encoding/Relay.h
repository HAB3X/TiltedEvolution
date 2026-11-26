#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

// The relay (Code/relay): lets friends join a host whose router can't be opened (no UPnP, CGNAT). Anyone runs it on a
// machine friends and host can all reach. The host's server asks it for a port (Allocate); friends connect their game
// to the relay's address and that port, and the relay passes their packets to and from the host's server over the
// connection the host opened, which works through any router. Joining players need nothing: to them the relay is the
// server.
//
// Datagrams between the host's server and the relay's control port: "STRY", the version, the type, then the type's
// fields, numbers big-endian.
namespace Relay
{
constexpr uint16_t kPort = 10600;
constexpr uint8_t kVersion = 1;
// Above this the host's server stops forwarding: game packets are well under it
constexpr size_t kMaxPayload = 1400;

enum class Type : uint8_t
{
    // Host: a port for its friends, with the relay's password (one byte of length, then the text)
    Allocate = 1,
    // Relay: the port friends connect to
    Allocated = 2,
    // Relay: no port (wrong password, full, or no allocation for this host any more), with the reason as text
    Refused = 3,
    // Both ways: a packet of one friend (peer), whose public address the relay adds on the way to the host
    Data = 4,
    // Host, every few seconds: still here; the relay answers KeepaliveAck, or Refused once it forgot the host
    Keepalive = 5,
    KeepaliveAck = 6,
    // Relay: the peer went quiet and is forgotten
    PeerClosed = 7,
};

struct Message
{
    explicit Message(Type aKind = Type::Keepalive) noexcept
        : Kind(aKind)
    {
    }

    Type Kind;
    // Allocate: the password; Refused: the reason
    std::string Text;
    // Allocated
    uint16_t Port{};
    // Data, PeerClosed
    uint32_t PeerId{};
    // Data from the relay: the peer's public IPv4 address (host byte order) and port; zero from the host
    uint32_t PeerAddress{};
    uint16_t PeerPort{};
    // Data
    std::string Payload;

    std::string Serialize() const
    {
        std::string out = "STRY";
        out += static_cast<char>(kVersion);
        out += static_cast<char>(Kind);
        switch (Kind)
        {
        case Type::Allocate:
        case Type::Refused:
            out += static_cast<char>(std::min<size_t>(Text.size(), 255));
            out.append(Text, 0, std::min<size_t>(Text.size(), 255));
            break;
        case Type::Allocated: Put(out, Port, 2); break;
        case Type::Data:
            Put(out, PeerId, 4);
            Put(out, PeerAddress, 4);
            Put(out, PeerPort, 2);
            out += Payload;
            break;
        case Type::PeerClosed: Put(out, PeerId, 4); break;
        case Type::Keepalive:
        case Type::KeepaliveAck: break;
        }
        return out;
    }

    // Empty for anything that isn't a whole message of this version
    static std::optional<Message> Parse(const char* apData, size_t aSize)
    {
        if (aSize < 6 || std::memcmp(apData, "STRY", 4) != 0 || static_cast<uint8_t>(apData[4]) != kVersion)
            return std::nullopt;
        Message message(static_cast<Type>(apData[5]));
        const char* pField = apData + 6;
        const size_t cLeft = aSize - 6;
        switch (message.Kind)
        {
        case Type::Allocate:
        case Type::Refused:
        {
            if (cLeft < 1 || cLeft != 1u + static_cast<uint8_t>(pField[0]))
                return std::nullopt;
            message.Text.assign(pField + 1, cLeft - 1);
            return message;
        }
        case Type::Allocated:
            if (cLeft != 2)
                return std::nullopt;
            message.Port = static_cast<uint16_t>(Get(pField, 2));
            return message.Port ? std::optional(message) : std::nullopt;
        case Type::Data:
            if (cLeft < 10 || cLeft - 10 > kMaxPayload)
                return std::nullopt;
            message.PeerId = Get(pField, 4);
            message.PeerAddress = Get(pField + 4, 4);
            message.PeerPort = static_cast<uint16_t>(Get(pField + 8, 2));
            message.Payload.assign(pField + 10, cLeft - 10);
            return message;
        case Type::PeerClosed:
            if (cLeft != 4)
                return std::nullopt;
            message.PeerId = Get(pField, 4);
            return message;
        case Type::Keepalive:
        case Type::KeepaliveAck: return cLeft == 0 ? std::optional(message) : std::nullopt;
        }
        return std::nullopt;
    }

private:
    static void Put(std::string& aOut, uint32_t aValue, int aBytes)
    {
        for (int i = aBytes - 1; i >= 0; --i)
            aOut += static_cast<char>((aValue >> (8 * i)) & 0xFF);
    }
    static uint32_t Get(const char* apData, int aBytes)
    {
        uint32_t value = 0;
        for (int i = 0; i < aBytes; ++i)
            value = (value << 8) | static_cast<uint8_t>(apData[i]);
        return value;
    }
};
} // namespace Relay
