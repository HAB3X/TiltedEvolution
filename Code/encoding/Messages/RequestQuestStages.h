#pragma once

#include "Message.h"
#include <Structs/QuestLog.h>

// Every minute in a party: the stages of the shared quests this game has running or completed, which the server
// compares with the party leader's (NotifyQuestDesync)
struct RequestQuestStages final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestQuestStages;

    RequestQuestStages()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestQuestStages& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Stages == acRhs.Stages; }

    QuestLog Stages{};
};
