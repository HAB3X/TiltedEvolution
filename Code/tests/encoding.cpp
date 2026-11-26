#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "StringCache.h"
#include "Messages/StringCacheUpdate.h"

#include <catch2/catch.hpp>

#include <Messages/ClientMessageFactory.h>
#include <Messages/RequestOwnershipTransfer.h>
#include <Structs/Movement.h>
#include <Structs/ServerSettings.h>
#include <WireProtocol.h>
#include <Messages/ServerMessageFactory.h>
#include <Structs/Vector2_NetQuantize.h>

#include <TiltedCore/Math.hpp>
#include <TiltedCore/Platform.hpp>

using namespace TiltedPhoques;

TEST_CASE("Encoding factory", "[encoding.factory]")
{
    Buffer buff(1000);

    {
        AuthenticationRequest request;
        request.Token = "TesSt";

        Buffer::Writer writer(&buff);
        request.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());

        auto pRequest = CastUnique<AuthenticationRequest>(std::move(pMessage));
        REQUIRE(pRequest->Token == request.Token);
    }

    {
        PartyAcceptInviteRequest request;
        request.InviterId = 123456;

        Buffer::Writer writer(&buff);
        request.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == request.GetOpcode());

        auto pRequest = CastUnique<PartyAcceptInviteRequest>(std::move(pMessage));
        REQUIRE(pRequest->InviterId == request.InviterId);
    }
}

TEST_CASE("Static structures", "[encoding.static]")
{
    GIVEN("GameId")
    {
        GameId sendObjects, recvObjects;
        sendObjects.ModId = 1456987;
        sendObjects.BaseId = 0x789654;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Vector3_NetQuantize")
    {
        Vector3_NetQuantize sendObjects, recvObjects;
        sendObjects.x = 142.56f;
        sendObjects.y = 45687.7f;
        sendObjects.z = -142.56f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Vector2_NetQuantize")
    {
        Vector2_NetQuantize sendObjects, recvObjects;
        sendObjects.x = 1000.89f;
        sendObjects.y = -485632.75f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Rotator2_NetQuantize")
    {
        Rotator2_NetQuantize sendObjects, recvObjects;
        sendObjects.x = 1.89f;
        sendObjects.y = TiltedPhoques::Pi * 2.0f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("Rotator2_NetQuantize needing wrap")
    {
        // This test is a bit dangerous as floating errors can lead to sendObjects != recvObjects but the difference is minuscule so we don't care abut such cases
        Rotator2_NetQuantize sendObjects, recvObjects;
        sendObjects.x = -1.87f;
        sendObjects.y = static_cast<float>(TiltedPhoques::Pi) * 18.0f + 3.6f;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendObjects.Serialize(writer);

            Buffer::Reader reader(&buff);
            recvObjects.Deserialize(reader);

            REQUIRE(sendObjects == recvObjects);
        }
    }

    GIVEN("ServerSettings")
    {
        ServerSettings sent;
        sent.HostWorld = true;
        sent.SharedQuestTypes = SharedQuests::Bit(SharedQuests::kMain) | SharedQuests::Bit(SharedQuests::kDawnguard);

        Buffer buffer(1000);
        Buffer::Writer writer(&buffer);
        sent.Serialize(writer);

        Buffer::Reader reader(&buffer);
        ServerSettings received;
        received.SharedQuestTypes = 0;
        received.Deserialize(reader);
        REQUIRE(received == sent);
        REQUIRE(received.SharedQuestTypes == sent.SharedQuestTypes);
    }

    GIVEN("Shared quest categories")
    {
        std::vector<std::string> unknown;
        REQUIRE(SharedQuests::Parse(SharedQuests::kDefaultText, unknown) == SharedQuests::kDefault);
        REQUIRE(unknown.empty());

        // What servers shared before the choice existed: everything but untyped and Miscellaneous quests
        REQUIRE_FALSE(SharedQuests::IsShared(SharedQuests::kDefault, SharedQuests::kNone));
        REQUIRE_FALSE(SharedQuests::IsShared(SharedQuests::kDefault, SharedQuests::kMisc));
        REQUIRE(SharedQuests::IsShared(SharedQuests::kDefault, SharedQuests::kMain));
        REQUIRE(SharedQuests::IsShared(SharedQuests::kDefault, SharedQuests::kDragonborn));

        const uint32_t cMask = SharedQuests::Parse(" main , MISC,,Nonsense", unknown);
        REQUIRE(cMask == (SharedQuests::Bit(SharedQuests::kMain) | SharedQuests::Bit(SharedQuests::kMisc)));
        REQUIRE(unknown == std::vector<std::string>{"nonsense"});
        // Untyped quests go with Miscellaneous
        REQUIRE(SharedQuests::IsShared(cMask, SharedQuests::kNone));
        REQUIRE_FALSE(SharedQuests::IsShared(cMask, SharedQuests::kDaedric));
        REQUIRE_FALSE(SharedQuests::IsShared(cMask, 200));
    }
}

TEST_CASE("Differential structures", "[encoding.differential]")
{
    GIVEN("Full ActionEvent")
    {
        ActionEvent sendAction, recvAction;

        sendAction.ActionId = 42;
        sendAction.State1 = 6547;
        sendAction.Tick = 48;
        sendAction.ActorId = 12345678;
        sendAction.EventName = "test";
        sendAction.IdleId = 87964;
        sendAction.State2 = 8963;
        sendAction.TargetEventName = "toast";
        sendAction.TargetId = 963741;
        sendAction.Type = 4;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.EventName = "Plot twist !";

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }
    }

    GIVEN("A single cached event name")
    {
        ActionEvent sendAction, recvAction;

        TP_UNUSED(StringCache::Get().Add("test"))

        sendAction.ActionId = 42;
        sendAction.State1 = 6547;
        sendAction.Tick = 48;
        sendAction.ActorId = 12345678;
        sendAction.EventName = "test";
        sendAction.IdleId = 87964;
        sendAction.State2 = 8963;
        sendAction.TargetEventName = "toast";
        sendAction.TargetId = 963741;
        sendAction.Type = 4;

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }

        {
            Buffer buff(1000);
            Buffer::Writer writer(&buff);

            sendAction.EventName = "Plot twist !";

            sendAction.GenerateDifferential(recvAction, writer);

            Buffer::Reader reader(&buff);
            recvAction.ApplyDifferential(reader);

            REQUIRE(sendAction == recvAction);
        }
    }

    GIVEN("Full Mods")
    {
        Mods sendMods, recvMods;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);

        sendMods.ModList.push_back({"Hello", 42});
        sendMods.ModList.push_back({"Hi", 14});
        sendMods.ModList.push_back({"Test", 8});
        sendMods.ModList.push_back({"Toast", 49});

        sendMods.Serialize(writer);

        Buffer::Reader reader(&buff);
        recvMods.Deserialize(reader);

        REQUIRE(sendMods == recvMods);
    }

    GIVEN("AnimationVariables")
    {
        AnimationVariables vars, recvVars;
 
        vars.Booleans.resize(76);
        String testString("\xDE\xAD\xBE\xEF"
                          "\xDE\xAD\xBE\xEF\x76\xB");
        vars.String_to_VectorBool(testString, vars.Booleans);

        vars.Floats.push_back(1.f);
        vars.Floats.push_back(7.f);
        vars.Floats.push_back(12.f);
        vars.Floats.push_back(0.f);
        vars.Floats.push_back(145.f);
        vars.Floats.push_back(100.f);
        vars.Floats.push_back(-1.f);

        vars.Integers.push_back(0);
        vars.Integers.push_back(12000);
        vars.Integers.push_back(06);
        vars.Integers.push_back(7778);
        vars.Integers.push_back(41104539);

        Buffer buff(1000);
        {
            Buffer::Writer writer(&buff);

            vars.GenerateDiff(recvVars, writer);

            Buffer::Reader reader(&buff);
            recvVars.ApplyDiff(reader);

            REQUIRE(vars.Booleans == recvVars.Booleans);
            REQUIRE(vars.Floats == recvVars.Floats);
            REQUIRE(vars.Integers == recvVars.Integers);
        }

        vars.Booleans.resize(33);
        vars.Booleans[16] = false;
        vars.Booleans[17] = false;
        vars.Booleans[18] = false;
        vars.Booleans[19] = false;
        vars.Floats[3] = 42.f;
        vars.Integers[0] = 18;
        vars.Integers[3] = 0;

        {
            Buffer::Writer writer(&buff);

            vars.GenerateDiff(recvVars, writer);

            Buffer::Reader reader(&buff);
            recvVars.ApplyDiff(reader);

            REQUIRE(vars.Booleans == recvVars.Booleans);
            REQUIRE(vars.Floats == recvVars.Floats);
            REQUIRE(vars.Integers == recvVars.Integers);
        }
    }
}

TEST_CASE("Packets", "[encoding.packets]")
{
    SECTION("AuthenticationRequest")
    {
        Buffer buff(1000);

        AuthenticationRequest sendMessage, recvMessage;
        sendMessage.Token = "TesSt";
        sendMessage.UserMods.ModList.push_back({"Hello", 42});
        sendMessage.UserMods.ModList.push_back({"Hi", 14});
        sendMessage.UserMods.ModList.push_back({"Test", 8});
        sendMessage.UserMods.ModList.push_back({"Toast", 49});

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    SECTION("AuthenticationResponse")
    {
        Buffer buff(1000);

        AuthenticationResponse sendMessage, recvMessage;
        sendMessage.Type = AuthenticationResponse::ResponseType::kAccepted;
        sendMessage.UserMods.ModList.push_back({"Hello", 42});
        sendMessage.UserMods.ModList.push_back({"Hi", 14});
        sendMessage.UserMods.ModList.push_back({"Test", 8});
        sendMessage.UserMods.ModList.push_back({"Toast", 49});

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    SECTION("AssignCharacterRequest")
    {
        Buffer buff(1000);

        ActionEvent sendAction;
        sendAction.ActionId = 42;
        sendAction.State1 = 6547;
        sendAction.Tick = 48;
        sendAction.ActorId = 12345678;
        sendAction.EventName = "test";
        sendAction.IdleId = 87964;
        sendAction.State2 = 8963;
        sendAction.TargetEventName = "toast";
        sendAction.TargetId = 963741;
        sendAction.Type = 4;

        AssignCharacterRequest sendMessage, recvMessage;
        sendMessage.Cookie = 14523698;
        sendMessage.AppearanceBuffer = "toto";
        sendMessage.CellId.BaseId = 45;
        sendMessage.FormId.ModId = 48;
        sendMessage.ReferenceId.BaseId = 456799;
        sendMessage.ReferenceId.ModId = 4079;
        sendMessage.LatestAction = sendAction;
        sendMessage.Position.x = -452.4f;
        sendMessage.Position.y = 452.4f;
        sendMessage.Position.z = 125452.4f;
        sendMessage.Rotation.x = -1.87f;
        sendMessage.Rotation.y = 45.35f;
        sendMessage.IsEncounterCandidate = true;

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    SECTION("AssignCharacterResponse")
    {
        Buffer buff(1000);

        AssignCharacterResponse sendMessage, recvMessage;
        sendMessage.Cookie = 14523698;
        sendMessage.ServerId = 4521;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.Owner = true;
        sendMessage.Suppressed = true;

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("ClientReferencesMoveRequest")
    {
        ClientReferencesMoveRequest sendMessage, recvMessage;
        sendMessage.Tick = 1712345678901;
        auto& move = sendMessage.Updates[1];
        move.Position.x = 1234.5f;
        move.Direction = 0.75f;
        move.HasLookAt = true;
        move.LookOffset.x = -320.f;
        move.LookOffset.y = 215.f;
        move.LookOffset.z = -48.f;

        AnimationVariables vars;
        vars.Booleans.resize(76);
        String testString("\xDE\xAD\xBE\xEF\x76\xB");
        vars.String_to_VectorBool(testString, vars.Booleans);

        vars.Floats.push_back(1.f);
        vars.Floats.push_back(7.f);
        vars.Floats.push_back(12.f);
        vars.Floats.push_back(0.f);
        vars.Floats.push_back(145.f);
        vars.Floats.push_back(100.f);
        vars.Floats.push_back(-1.f);

        vars.Integers.push_back(0);
        vars.Integers.push_back(12000);
        vars.Integers.push_back(06);
        vars.Integers.push_back(7778);
        vars.Integers.push_back(41104539);

        move.Variables = vars;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(recvMessage.Tick == sendMessage.Tick);
        REQUIRE(recvMessage.Updates[1] == sendMessage.Updates[1]);
    }

    GIVEN("RequestActionEvents")
    {
        ActionEvent firstAction;
        firstAction.Tick = 1712345678901;
        firstAction.ActionId = 42;
        firstAction.TargetId = 963741;
        firstAction.State1 = 6547;
        firstAction.EventName = "jumpStandingStart";

        ActionEvent secondAction = firstAction;
        secondAction.Tick += 120;
        secondAction.ActionId = 43;
        secondAction.EventName = "attackStart";
        secondAction.TargetEventName = "toast";

        RequestActionEvents sendMessage, recvMessage;
        sendMessage.Actions[1] = {firstAction, secondAction};
        sendMessage.Actions[7] = {secondAction};
        sendMessage.OwnershipEpochs[1] = 271;
        sendMessage.OwnershipEpochs[7] = 272;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("HostWorldStateRequest")
    {
        HostWorldStateRequest sendMessage, recvMessage;
        sendMessage.PlayerId = 70000;
        sendMessage.State.RunningQuests.Entries.push_back({GameId(0, 0x3372B), 10});
        sendMessage.State.RunningQuests.Entries.push_back({GameId(0x01000000, 0x2BA4), 65535});
        sendMessage.State.CompletedQuests.Entries.push_back({GameId(0, 0x46EF2), 200});
        for (uint32_t i = 0; i < 300; ++i)
            sendMessage.State.ClearedLocations.push_back(GameId(0, 0x10000 + i));
        sendMessage.State.DeadUniqueActors.push_back(GameId(0x02000000, 0x1A69C));

        Buffer buff(10000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyHostWorldState")
    {
        NotifyHostWorldState sendMessage, recvMessage;
        sendMessage.HostName = "Lydia's host";
        sendMessage.State.CompletedQuests.Entries.push_back({GameId(0, 0x46EF2), 200});
        sendMessage.State.DeadUniqueActors.push_back(GameId(0, 0x1A69C));

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyHostWorldRequested")
    {
        NotifyHostWorldRequested sendMessage, recvMessage;
        sendMessage.PlayerId = 1234567;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("RequestModeratePlayer")
    {
        RequestModeratePlayer sendMessage, recvMessage;
        sendMessage.PlayerId = 77777;
        sendMessage.Ban = true;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyPartyCheckpoint")
    {
        NotifyPartyCheckpoint sendMessage, recvMessage;
        sendMessage.CheckpointId = 1234567;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyActionEvents")
    {
        ActionEvent action;
        action.Tick = 1712345678901;
        action.ActionId = 42;
        action.EventName = "blockStart";

        NotifyActionEvents sendMessage;
        sendMessage.Actions[3] = {action, action};
        sendMessage.OwnershipEpochs[3] = 314;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ServerMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<NotifyActionEvents>(std::move(pMessage)) == sendMessage);
    }

    SECTION("VoiceDataRequest")
    {
        Buffer buff(1000);

        VoiceDataRequest sendMessage;
        sendMessage.Frame.Sequence = 65535;
        for (uint32_t i = 0; i < 97; ++i)
            sendMessage.Frame.Data.push_back(static_cast<uint8_t>(i * 7));

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<VoiceDataRequest>(std::move(pMessage)) == sendMessage);
    }

    SECTION("NotifyVoiceData")
    {
        Buffer buff(1000);

        NotifyVoiceData sendMessage;
        sendMessage.PlayerId = 123456;
        sendMessage.Frame.Sequence = 42;
        for (uint32_t i = 0; i < 60; ++i)
            sendMessage.Frame.Data.push_back(static_cast<uint8_t>(255 - i));

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ServerMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<NotifyVoiceData>(std::move(pMessage)) == sendMessage);
    }

    SECTION("VoiceFrame rejects oversized data")
    {
        Buffer buff(2000);

        // Written by hand, Serialize itself never writes more than kMaxSize
        Buffer::Writer writer(&buff);
        writer.WriteBits(7, 16);
        Serialization::WriteVarInt(writer, VoiceFrame::kMaxSize + 1);

        Buffer::Reader reader(&buff);

        VoiceFrame frame;
        REQUIRE_FALSE(frame.Deserialize(reader));
        REQUIRE(frame.Data.empty());
    }

    SECTION("VoiceStateRequest")
    {
        Buffer buff(1000);

        VoiceStateRequest sendMessage;
        sendMessage.Listening = true;
        sendMessage.ListenChannel = VoiceStateRequest::kStrangers;

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<VoiceStateRequest>(std::move(pMessage)) == sendMessage);
    }

    GIVEN("RequestInventorySnapshot")
    {
        Inventory::Entry armor;
        armor.BaseId.BaseId = 0x12E46;
        armor.Count = 1;
        armor.ExtraWorn = true;

        Inventory::Entry gold;
        gold.BaseId.BaseId = 0xF;
        gold.Count = 250;

        RequestInventorySnapshot sendMessage;
        sendMessage.ServerId = 0x10042;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.CurrentInventory.Entries = {armor, gold};
        sendMessage.CurrentInventory.CurrentMagicEquipment.RightHandSpell.BaseId = 0x12FCD;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ClientMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<RequestInventorySnapshot>(std::move(pMessage)) == sendMessage);
    }

    GIVEN("NotifyInventorySnapshot")
    {
        Inventory::Entry armor;
        armor.BaseId.BaseId = 0x12E46;
        armor.Count = 1;
        armor.ExtraWornLeft = true;

        NotifyInventorySnapshot sendMessage;
        sendMessage.ServerId = 0x10042;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.CurrentInventory.Entries = {armor};
        sendMessage.CurrentInventory.CurrentMagicEquipment.Shout.BaseId = 0x13E07;

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        const ServerMessageFactory factory;
        auto pMessage = factory.Extract(reader);

        REQUIRE(pMessage);
        REQUIRE(pMessage->GetOpcode() == sendMessage.GetOpcode());
        REQUIRE(*CastUnique<NotifyInventorySnapshot>(std::move(pMessage)) == sendMessage);
    }
}

TEST_CASE("StringCache", "[encoding.string_cache]")
{
    SECTION("Messages")
    {
        StringCacheUpdate update;
        update.Values.push_back("Hello");
        update.Values.push_back("Bye");

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        update.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        StringCacheUpdate recvUpdate;
        recvUpdate.DeserializeRaw(reader);

        REQUIRE(update == recvUpdate);
    }
}

TEST_CASE("Official v1.8.4 wire protocol", "[encoding.official184]")
{
    // Restores this build's protocol for the other tests, however this one ends
    struct ProtocolScope
    {
        ProtocolScope() { Wire::Set(WireProtocol::Official184); }
        ~ProtocolScope() { Wire::Set(WireProtocol::Current); }
    } scope;

    GIVEN("Message numbers")
    {
        // v1.8.4 has no inventory snapshot messages, so everything after them is one lower there
        REQUIRE(Wire::ToWire(kRequestInventoryChanges) == static_cast<uint8_t>(kRequestInventoryChanges));
        REQUIRE(Wire::ToWire(kSendChatMessageRequest) == static_cast<uint8_t>(kSendChatMessageRequest) - 1);
        REQUIRE(Wire::FromWire(static_cast<uint8_t>(kNotifyChatMessageBroadcast) - 1) == kNotifyChatMessageBroadcast);
        REQUIRE_FALSE(Wire::ToWire(kRequestInventorySnapshot).has_value());
        REQUIRE_FALSE(Wire::ToWire(kRequestHostWorld).has_value());
        REQUIRE_FALSE(Wire::FromWire(55).has_value());
    }

    GIVEN("Fields v1.8.4 lacks are neither written nor read")
    {
        ServerSettings sent;
        sent.AutoPartyJoin = true;
        sent.ConsoleCommandsAllowed = false;
        sent.VoipEnabled = true;
        sent.HostWorld = true;
        sent.SharedQuestTypes = SharedQuests::Bit(SharedQuests::kMain);

        Buffer buffer(1000);
        Buffer::Writer writer(&buffer);
        sent.Serialize(writer);
        Serialization::WriteVarInt(writer, 1234567); // what follows the settings in an authentication response

        Buffer::Reader reader(&buffer);
        ServerSettings received;
        received.Deserialize(reader);
        REQUIRE(received.AutoPartyJoin);
        REQUIRE(received.ConsoleCommandsAllowed);
        REQUIRE_FALSE(received.VoipEnabled);
        REQUIRE_FALSE(received.HostWorld);
        REQUIRE(received.SharedQuestTypes == SharedQuests::kDefault);
        REQUIRE(Serialization::ReadVarInt(reader) == 1234567);
    }

    GIVEN("Movement without where the actor looks")
    {
        Movement sent;
        sent.Position = glm::vec3{10.f, -20.f, 30.f};
        sent.HasLookAt = true;
        sent.LookOffset = glm::vec3{1.f, 2.f, 3.f};

        Buffer buffer(1000);
        Buffer::Writer writer(&buffer);
        sent.Serialize(writer);
        Serialization::WriteVarInt(writer, 42);

        Buffer::Reader reader(&buffer);
        Movement received;
        received.Deserialize(reader);
        REQUIRE(received.Position == sent.Position);
        REQUIRE_FALSE(received.HasLookAt);
        REQUIRE(Serialization::ReadVarInt(reader) == 42);
    }

    GIVEN("A deleted actor is handed on")
    {
        RequestOwnershipTransfer sent;
        sent.ServerId = 7;
        sent.Reason = OwnershipReleaseReason::Deleted;

        Buffer buffer(1000);
        Buffer::Writer writer(&buffer);
        sent.Serialize(writer);

        Buffer::Reader reader(&buffer);
        uint64_t opcode;
        reader.ReadBits(opcode, 8);
        RequestOwnershipTransfer received;
        received.DeserializeRaw(reader);
        REQUIRE(received.ServerId == 7);
        REQUIRE(received.Reason == OwnershipReleaseReason::Relinquish);
    }
}
