#include <Messages/NotifyPartyTeleportRefused.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyPartyTeleportRefused::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Reason);
    Serialization::WriteString(aWriter, PlayerName);
    Serialization::WriteVarInt(aWriter, SecondsLeft);
}

void NotifyPartyTeleportRefused::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    Reason = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    PlayerName = Serialization::ReadString(aReader);
    SecondsLeft = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
