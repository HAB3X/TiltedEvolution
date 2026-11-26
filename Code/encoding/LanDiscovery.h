#pragma once

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

// Finding servers on the local network: a client broadcasts kLanQuery to kLanDiscoveryPort over UDP and each server
// that hears it answers with a LanServerReply. Plain text, one field per line, the name last since it may hold anything.
namespace LanDiscovery
{
constexpr uint16_t kPort = 10579;
constexpr char kQuery[] = "STLAN?1";
constexpr char kReplyTag[] = "STLAN!1";

struct Reply
{
    uint16_t Port{};
    uint32_t Players{};
    uint32_t MaxPlayers{};
    bool HasPassword{};
    bool HostWorld{};
    std::string Name;

    std::string Serialize() const
    {
        std::ostringstream out;
        out << kReplyTag << '\n' << Port << '\n' << Players << '\n' << MaxPlayers << '\n' << HasPassword << '\n' << HostWorld << '\n' << Name;
        return out.str();
    }

    // Empty for anything that isn't a whole reply
    static std::optional<Reply> Parse(const std::string& acText)
    {
        std::istringstream in(acText);
        std::string tag;
        Reply reply;
        uint32_t port = 0;
        if (!std::getline(in, tag) || tag != kReplyTag || !(in >> port >> reply.Players >> reply.MaxPlayers >> reply.HasPassword >> reply.HostWorld) || port == 0 ||
            port > 65535)
            return std::nullopt;
        in.ignore(1); // the line break before the name
        std::getline(in, reply.Name, '\0');
        reply.Port = static_cast<uint16_t>(port);
        return reply;
    }
};
} // namespace LanDiscovery
