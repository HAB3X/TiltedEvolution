#include <Messages/NotifyPartyQuestSharing.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void NotifyPartyQuestSharing::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteBool(aWriter, Enabled);
    Serialization::WriteVarInt(aWriter, Types);
}

void NotifyPartyQuestSharing::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    Enabled = Serialization::ReadBool(aReader);
    Types = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
