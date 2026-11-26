#pragma once

#include "Message.h"
#include <Structs/GameId.h>

// The answer to a party member's quest offer (NotifyQuestShareOffer). On yes the server sends the quest's current
// stage (NotifyQuestUpdate) and relays its updates to this player from then on.
struct RequestQuestShareAnswer final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestQuestShareAnswer;

    RequestQuestShareAnswer()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestQuestShareAnswer& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Id == acRhs.Id && Accept == acRhs.Accept; }

    GameId Id{};
    bool Accept{};
};
