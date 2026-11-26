#include <Messages/RequestCompanions.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestCompanions::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Companions.size());
    for (const Companion& cCompanion : Companions)
    {
        Serialization::WriteVarInt(aWriter, cCompanion.ServerId);
        Serialization::WriteVarInt(aWriter, cCompanion.ExpectedOwnershipEpoch);
    }
}

void RequestCompanions::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Companions.resize(static_cast<size_t>(std::min<uint64_t>(Serialization::ReadVarInt(aReader), kMaxCompanions)));
    for (Companion& companion : Companions)
    {
        companion.ServerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        companion.ExpectedOwnershipEpoch = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    }
}
