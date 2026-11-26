#include <Messages/RequestPartyQuestSharing.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestPartyQuestSharing::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteBool(aWriter, Enabled);
    Serialization::WriteVarInt(aWriter, Types);
}

void RequestPartyQuestSharing::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Enabled = Serialization::ReadBool(aReader);
    Types = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
