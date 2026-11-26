#include "ServerSettings.h"
#include <WireProtocol.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

bool ServerSettings::operator==(const ServerSettings& acRhs) const noexcept
{
    return Difficulty == acRhs.Difficulty && GreetingsEnabled == acRhs.GreetingsEnabled && PvpEnabled == acRhs.PvpEnabled && SyncPlayerHomes == acRhs.SyncPlayerHomes && DeathSystemEnabled == acRhs.DeathSystemEnabled && AutoPartyJoin == acRhs.AutoPartyJoin && ConsoleCommandsAllowed == acRhs.ConsoleCommandsAllowed && VoipEnabled == acRhs.VoipEnabled && HostWorld == acRhs.HostWorld;
}

bool ServerSettings::operator!=(const ServerSettings& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void ServerSettings::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Difficulty);
    Serialization::WriteBool(aWriter, GreetingsEnabled);
    Serialization::WriteBool(aWriter, PvpEnabled);
    Serialization::WriteBool(aWriter, SyncPlayerHomes);
    Serialization::WriteBool(aWriter, DeathSystemEnabled);
    Serialization::WriteBool(aWriter, SyncPlayerCalendar);
    Serialization::WriteBool(aWriter, AutoPartyJoin);
    // Official v1.8.4 has none of these
    if (Wire::IsOfficial184())
        return;
    Serialization::WriteBool(aWriter, ConsoleCommandsAllowed);
    Serialization::WriteBool(aWriter, VoipEnabled);
    Serialization::WriteBool(aWriter, HostWorld);
}

void ServerSettings::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    Difficulty = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    GreetingsEnabled = Serialization::ReadBool(aReader);
    PvpEnabled = Serialization::ReadBool(aReader);
    SyncPlayerHomes = Serialization::ReadBool(aReader);
    DeathSystemEnabled = Serialization::ReadBool(aReader);
    SyncPlayerCalendar = Serialization::ReadBool(aReader);
    AutoPartyJoin = Serialization::ReadBool(aReader);
    // On official v1.8.4: the console works, and there's no voice chat or host world
    if (Wire::IsOfficial184())
    {
        ConsoleCommandsAllowed = true;
        VoipEnabled = false;
        HostWorld = false;
        return;
    }
    ConsoleCommandsAllowed = Serialization::ReadBool(aReader);
    VoipEnabled = Serialization::ReadBool(aReader);
    HostWorld = Serialization::ReadBool(aReader);
}
