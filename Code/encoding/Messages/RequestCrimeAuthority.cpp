#include <Messages/RequestCrimeAuthority.h>

#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

void RequestCrimeAuthority::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Faction.Serialize(aWriter);
    Serialization::WriteVarInt(aWriter, Bounty);
    Serialization::WriteVarInt(aWriter, Guards.size());
    for (const Guard& cGuard : Guards)
    {
        Serialization::WriteVarInt(aWriter, cGuard.ServerId);
        Serialization::WriteVarInt(aWriter, cGuard.ExpectedOwnershipEpoch);
    }
}

void RequestCrimeAuthority::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
    Faction.Deserialize(aReader);
    Bounty = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    const uint64_t cCount = Serialization::ReadVarInt(aReader);
    // More guards than a client ever sends is a corrupt message, not a reason to allocate them
    Guards.resize(static_cast<size_t>(std::min<uint64_t>(cCount, kMaxGuards)));
    for (Guard& guard : Guards)
    {
        guard.ServerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        guard.ExpectedOwnershipEpoch = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    }
}
