#include <Messages/RequestModeratePlayer.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestModeratePlayer::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, PlayerId);
    Serialization::WriteBool(aWriter, Ban);
}

void RequestModeratePlayer::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Ban = Serialization::ReadBool(aReader);
}
