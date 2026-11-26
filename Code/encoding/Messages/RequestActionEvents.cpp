#include <Messages/RequestActionEvents.h>
#include <WireProtocol.h>
#include <TiltedCore/Serialization.hpp>

void RequestActionEvents::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, Actions.size());

    for (const auto& [serverId, actions] : Actions)
    {
        Serialization::WriteVarInt(aWriter, serverId);
        // Official v1.8.4 doesn't tag actions with the owner's epoch
        if (!Wire::IsOfficial184())
        {
            const auto epochIt = OwnershipEpochs.find(serverId);
            Serialization::WriteVarInt(aWriter, epochIt != OwnershipEpochs.end() ? epochIt->second : 0);
        }
        Serialization::WriteVarInt(aWriter, actions.size());

        for (const auto& action : actions)
            action.GenerateDifferential(ActionEvent{}, aWriter);
    }
}

void RequestActionEvents::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    const auto count = Serialization::ReadVarInt(aReader);

    for (auto i = 0u; i < count; ++i)
    {
        const uint32_t cServerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        // Left out on official v1.8.4, whose actions carry no epoch
        if (!Wire::IsOfficial184())
            OwnershipEpochs[cServerId] = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
        auto& actions = Actions[cServerId];

        actions.resize(Serialization::ReadVarInt(aReader));

        for (auto& action : actions)
            action.ApplyDifferential(aReader);
    }
}
