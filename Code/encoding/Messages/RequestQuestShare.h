#pragma once

#include "Message.h"
#include <Structs/GameId.h>

// A party member offers one of their quests to the rest of the party, even of a kind the party doesn't share. Each
// other member is asked (NotifyQuestShareOffer); those who accept are put on its current stage and its updates are
// relayed among them.
struct RequestQuestShare final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestQuestShare;

    RequestQuestShare()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestQuestShare& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Id == acRhs.Id && Stage == acRhs.Stage && ClientQuestType == acRhs.ClientQuestType; }

    GameId Id{};
    // The quest's current stage in the offerer's game
    uint16_t Stage{};
    // TESQuest::Type, as in RequestQuestUpdate
    uint8_t ClientQuestType{};
};
