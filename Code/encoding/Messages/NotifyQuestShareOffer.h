#pragma once

#include "Message.h"
#include <Structs/GameId.h>

// A party member offers the receiver one of their quests (RequestQuestShare); answered with RequestQuestShareAnswer
struct NotifyQuestShareOffer final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyQuestShareOffer;

    NotifyQuestShareOffer()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyQuestShareOffer& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && PlayerId == acRhs.PlayerId && PlayerName == acRhs.PlayerName && Id == acRhs.Id; }

    // Who offers it
    uint32_t PlayerId{};
    TiltedPhoques::String PlayerName{};
    GameId Id{};
};
