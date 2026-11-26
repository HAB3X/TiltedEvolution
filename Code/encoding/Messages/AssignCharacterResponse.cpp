#include <Messages/AssignCharacterResponse.h>
#include <WireProtocol.h>

void AssignCharacterResponse::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Cookie);
    Serialization::WriteVarInt(aWriter, ServerId);
    Serialization::WriteVarInt(aWriter, PlayerId);
    Position.Serialize(aWriter);
    CellId.Serialize(aWriter);
    WorldSpaceId.Serialize(aWriter);
    AllActorValues.Serialize(aWriter);
    CurrentInventory.Serialize(aWriter);
    ActionsToReplay.Serialize(aWriter);
    Serialization::WriteVarInt(aWriter, OwnershipEpoch);
    Serialization::WriteBool(aWriter, Owner);
    Serialization::WriteBool(aWriter, IsDead);
    Serialization::WriteBool(aWriter, IsWeaponDrawn);
    LeveledNpcPickId.Serialize(aWriter);
    if (!Wire::IsOfficial184())
        Serialization::WriteBool(aWriter, Suppressed);
}

void AssignCharacterResponse::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    Cookie = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ServerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    PlayerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Position.Deserialize(aReader);
    CellId.Deserialize(aReader);
    WorldSpaceId.Deserialize(aReader);
    AllActorValues.Deserialize(aReader);
    CurrentInventory.Deserialize(aReader);
    ActionsToReplay.Deserialize(aReader);
    OwnershipEpoch = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    Owner = Serialization::ReadBool(aReader);
    IsDead = Serialization::ReadBool(aReader);
    IsWeaponDrawn = Serialization::ReadBool(aReader);
    LeveledNpcPickId.Deserialize(aReader);
    // Official v1.8.4 never suppresses an actor
    Suppressed = !Wire::IsOfficial184() && Serialization::ReadBool(aReader);
}
