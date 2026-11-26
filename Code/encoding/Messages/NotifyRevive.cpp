#include <Messages/NotifyRevive.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyRevive::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(static_cast<uint8_t>(Result), 1);
    Serialization::WriteVarInt(aWriter, ReviverId);
    Serialization::WriteString(aWriter, ReviverName);
}

void NotifyRevive::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    uint64_t result = 0;
    aReader.ReadBits(result, 1);
    Result = static_cast<Outcome>(result);
    ReviverId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ReviverName = Serialization::ReadString(aReader);
}
