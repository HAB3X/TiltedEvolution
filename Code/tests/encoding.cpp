#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>

#include <cstring>
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
#include <QuestDesync.h>
#include <PartyCompass.h>
#include <WireProtocol.h>
#include <RequiredMods.h>
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

TEST_CASE("Quest desync", "[encoding.quest_desync]")
{
    const GameId cA(0, 0x10), cB(0, 0x20), cC(1, 0x05), cD(2, 0x01);
    QuestLog leader, member;
    leader.Entries = {{cB, 20}, {cA, 10}, {cC, 5}};
    member.Entries = {{cA, 10}, {cD, 1}, {cB, 30}};

    GIVEN("The same quests at the same stages, in any order")
    {
        QuestLog same;
        same.Entries = {{cC, 5}, {cA, 10}, {cB, 20}};
        REQUIRE(QuestDesync::Differences(leader, same).empty());
    }

    GIVEN("A different stage, and quests only one side has")
    {
        REQUIRE(QuestDesync::Differences(leader, member) == Vector<GameId>{cB, cC, cD});
        REQUIRE(QuestDesync::Differences(leader, QuestLog{}) == Vector<GameId>{cA, cB, cC});
    }

    GIVEN("Only differences seen twice in a row count")
    {
        REQUIRE(QuestDesync::InBoth({cB, cC, cD}, {cA, cC, cD}) == Vector<GameId>{cC, cD});
        REQUIRE(QuestDesync::InBoth({}, {cA}).empty());
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

    GIVEN("Mods with their plugin sizes")
    {
        Mods sendMods, recvMods;
        sendMods.ModList.push_back({"SomeMod.esp", 3, false, 12345});
        sendMods.ModList.push_back({"Big.esm", 4, false, 0xFFFFFFFF});
        sendMods.ModList.push_back({"Unknown.esl", 5, true, 0});

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMods.Serialize(writer);

        Buffer::Reader reader(&buff);
        recvMods.Deserialize(reader);

        REQUIRE(recvMods.ModList.size() == 3);
        REQUIRE(recvMods.ModList[0].FileSize == 12345);
        REQUIRE(recvMods.ModList[1].FileSize == 0xFFFFFFFF);
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

    SECTION("RequestTimeSkip")
    {
        Buffer buff(1000);

        RequestTimeSkip sendMessage, recvMessage;
        sendMessage.Hours = 7.5f;

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    SECTION("NotifyTimeSkip")
    {
        Buffer buff(1000);

        NotifyTimeSkip sendMessage, recvMessage;
        sendMessage.Hours = 7.5f;
        sendMessage.ReadyCount = 1;
        sendMessage.RequiredCount = 2;

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

    SECTION("AssignCharacterResponse")
    {
        Buffer buff(1000);

        AssignCharacterResponse sendMessage, recvMessage;
        sendMessage.Cookie = 1234;
        sendMessage.ServerId = 42;
        sendMessage.OwnershipEpoch = 7;
        sendMessage.Owner = true;
        sendMessage.IsDead = true;
        sendMessage.LeveledNpcPickId = GameId{1, 0x1234};
        sendMessage.ForceDead = true;

        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
        REQUIRE(recvMessage.ForceDead);
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

    GIVEN("RequestPlayerHealthUpdate with the party bar's vitals")
    {
        RequestPlayerHealthUpdate sendMessage, recvMessage;
        sendMessage.Percentage = 42.5f;
        sendMessage.Vitals = PartyVitals{.Magicka = 80, .Stamina = 5, .InCombat = true, .Downed = true};

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyPlayerHealthUpdate with the party bar's vitals")
    {
        NotifyPlayerHealthUpdate sendMessage, recvMessage;
        sendMessage.PlayerId = 77;
        sendMessage.Percentage = 100.f;
        sendMessage.Vitals = PartyVitals{.Magicka = 100, .Stamina = 63, .InCombat = true, .Downed = false};

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("RequestPartyPing")
    {
        RequestPartyPing sendMessage, recvMessage;
        sendMessage.WorldSpaceId = GameId(0, 0x3C);
        sendMessage.CellId = GameId(0, 0x9732);
        sendMessage.Position = glm::vec3{1200.f, -340.f, 56.f};
        sendMessage.Target = "Bandit Chief";

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyPartyPing")
    {
        NotifyPartyPing sendMessage, recvMessage;
        sendMessage.PlayerId = 9;
        sendMessage.PlayerName = "Lydia";
        sendMessage.CellId = GameId(0, 0x165A7);
        sendMessage.Position = glm::vec3{-80.f, 20.f, 0.f};

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("RequestPartyDoor")
    {
        RequestPartyDoor sendMessage, recvMessage;
        sendMessage.CellId = GameId(0, 0x165A7);
        sendMessage.Position = glm::vec3{12.f, 340.f, -56.f};
        sendMessage.Place = "Bleak Falls Barrow";

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyPartyDoor")
    {
        NotifyPartyDoor sendMessage, recvMessage;
        sendMessage.PlayerId = 4;
        sendMessage.PlayerName = "Lydia";
        sendMessage.WorldSpaceId = GameId(0, 0x3C);
        sendMessage.CellId = GameId(0, 0x9732);
        sendMessage.Position = glm::vec3{-1200.f, 40.f, 6.f};
        sendMessage.Place = "Skyrim";

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("RequestPartyTravel")
    {
        RequestPartyTravel sendMessage, recvMessage;
        sendMessage.Place = "Riverwood";

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyPartyTravel")
    {
        NotifyPartyTravel sendMessage, recvMessage;
        sendMessage.LeaderId = 4321;
        sendMessage.LeaderName = "Lydia";
        sendMessage.Place = "Dragonsreach";

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("RequestQuestStages")
    {
        RequestQuestStages sendMessage, recvMessage;
        sendMessage.Stages.Entries.push_back({GameId(0, 0x3372B), 10});
        sendMessage.Stages.Entries.push_back({GameId(2, 0x1234), 200});

        Buffer buff(1000);
        Buffer::Writer writer(&buff);
        sendMessage.Serialize(writer);

        Buffer::Reader reader(&buff);

        uint64_t trash;
        reader.ReadBits(trash, 8); // pop opcode

        recvMessage.DeserializeRaw(reader);

        REQUIRE(sendMessage == recvMessage);
    }

    GIVEN("NotifyQuestDesync")
    {
        NotifyQuestDesync sendMessage, recvMessage;
        sendMessage.OtherPlayer = "Lydia";
        sendMessage.OtherIsLeader = true;
        sendMessage.Quests.push_back(GameId(0, 0x3372B));
        sendMessage.Quests.push_back(GameId(2, 0x1234));

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
        REQUIRE_FALSE(Wire::ToWire(kRequestPartyTravel).has_value());
        REQUIRE_FALSE(Wire::ToWire(kRequestQuestStages).has_value());
        REQUIRE_FALSE(Wire::ToWire(kRequestPartyPing).has_value());
        REQUIRE_FALSE(Wire::ToWire(kRequestPartyDoor).has_value());
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

    GIVEN("Mods without their plugin sizes, laid out as v1.8.4 does")
    {
        Mods sent;
        sent.ModList.push_back({"SomeMod.esp", 3, true, 12345});

        // Zeroed, so the comparison below doesn't see leftover bits after the last one written
        Buffer buffer(1000);
        std::memset(buffer.GetWriteData(), 0, buffer.GetSize());
        Buffer::Writer writer(&buffer);
        sent.Serialize(writer);
        Serialization::WriteVarInt(writer, 42);

        // v1.8.4's layout: a 13-bit count, then each mod's 16-bit id, lite flag and file name
        Buffer expected(1000);
        std::memset(expected.GetWriteData(), 0, expected.GetSize());
        Buffer::Writer expectedWriter(&expected);
        expectedWriter.WriteBits(1, 13);
        expectedWriter.WriteBits(3, 16);
        Serialization::WriteBool(expectedWriter, true);
        Serialization::WriteString(expectedWriter, "SomeMod.esp");
        Serialization::WriteVarInt(expectedWriter, 42);
        REQUIRE(writer.Size() == expectedWriter.Size());
        REQUIRE(std::memcmp(buffer.GetData(), expected.GetData(), writer.Size()) == 0);

        Buffer::Reader reader(&buffer);
        Mods received;
        received.Deserialize(reader);
        REQUIRE(received.ModList.size() == 1);
        REQUIRE(received.ModList[0].Filename == "SomeMod.esp");
        REQUIRE(received.ModList[0].FileSize == 0);
        REQUIRE(Serialization::ReadVarInt(reader) == 42);
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

    GIVEN("A health update without the party bar's vitals, laid out as v1.8.4 does")
    {
        NotifyPlayerHealthUpdate sent;
        sent.PlayerId = 3;
        sent.Percentage = 25.f;
        sent.Vitals = PartyVitals{.Magicka = 50, .Stamina = 50, .InCombat = true, .Downed = true};

        Buffer buffer(1000);
        Buffer::Writer writer(&buffer);
        sent.Serialize(writer);
        Serialization::WriteVarInt(writer, 42);

        Buffer::Reader reader(&buffer);
        uint64_t opcode;
        reader.ReadBits(opcode, 8);
        NotifyPlayerHealthUpdate received;
        received.DeserializeRaw(reader);
        REQUIRE(received.PlayerId == 3);
        REQUIRE(received.Percentage == 25.f);
        REQUIRE(received.Vitals == PartyVitals{});
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

TEST_CASE("Party compass", "[encoding.party_compass]")
{
    using namespace PartyCompass;

    GIVEN("A spot in each direction, +Y north and +X east")
    {
        REQUIRE(Toward(0.f, 0.f, 0.f, 100.f) == kNorth);
        REQUIRE(Toward(0.f, 0.f, 100.f, 100.f) == kNorthEast);
        REQUIRE(Toward(0.f, 0.f, 100.f, 0.f) == kEast);
        REQUIRE(Toward(0.f, 0.f, 100.f, -100.f) == kSouthEast);
        REQUIRE(Toward(0.f, 0.f, 0.f, -100.f) == kSouth);
        REQUIRE(Toward(0.f, 0.f, -100.f, -100.f) == kSouthWest);
        REQUIRE(Toward(0.f, 0.f, -100.f, 0.f) == kWest);
        REQUIRE(Toward(0.f, 0.f, -100.f, 100.f) == kNorthWest);
        // Measured from the first spot, not the origin
        REQUIRE(Toward(500.f, 500.f, 500.f, 400.f) == kSouth);
    }

    GIVEN("Bearings near a boundary go to the nearest point")
    {
        // 20 degrees east of north is still north, 25 is north-east
        REQUIRE(Toward(0.f, 0.f, std::tan(0.349f) * 100.f, 100.f) == kNorth);
        REQUIRE(Toward(0.f, 0.f, std::tan(0.436f) * 100.f, 100.f) == kNorthEast);
        // Just west of south wraps to south, not to north-west
        REQUIRE(Toward(0.f, 0.f, -1.f, -100.f) == kSouth);
    }

    GIVEN("Distances in metres")
    {
        REQUIRE(Distance(0.f, 0.f, 0.f, 3.f, 4.f, 12.f) == 13.f);
        REQUIRE(ToMeters(7000.f) == Approx(99.96f));
    }
}

TEST_CASE("Required mods lists", "[encoding.required_mods]")
{
    using RequiredMods::Listed;

    GIVEN("Lines of a mods file")
    {
        REQUIRE(RequiredMods::ParseLine("SomeMod.esp") == Listed{"SomeMod.esp", 0});
        REQUIRE(RequiredMods::ParseLine("SomeMod.esp|12345") == Listed{"SomeMod.esp", 12345});
        REQUIRE(RequiredMods::ParseLine("SomeMod.esp|12345\r") == Listed{"SomeMod.esp", 12345});
        REQUIRE(RequiredMods::ParseLine("SomeMod.esp\r") == Listed{"SomeMod.esp", 0});
        REQUIRE_FALSE(RequiredMods::ParseLine("SomeMod.esp|big").has_value());
        REQUIRE_FALSE(RequiredMods::ParseLine("SomeMod.esp|12x").has_value());
        REQUIRE_FALSE(RequiredMods::ParseLine("SomeMod.esp|").has_value());
        REQUIRE_FALSE(RequiredMods::ParseLine("|12345").has_value());
        REQUIRE_FALSE(RequiredMods::ParseLine("").has_value());

        REQUIRE(RequiredMods::FormatLine("SomeMod.esp", 12345) == "SomeMod.esp|12345");
        REQUIRE(RequiredMods::FormatLine("SomeMod.esp", 0) == "SomeMod.esp");
        REQUIRE(RequiredMods::ParseLine(RequiredMods::FormatLine("A B.esp", 7)) == Listed{"A B.esp", 7});
    }

    GIVEN("A player's mods compared with the listed ones")
    {
        const std::vector<Listed> cListed{{"Sized.esp", 100}, {"Plain.esp", 0}, {"Gone.esp", 5}};
        auto player = [](std::initializer_list<std::pair<const char*, uint32_t>> aMods)
        {
            Vector<Mods::Entry> mods;
            for (const auto& [cpName, cSize] : aMods)
                mods.push_back({cpName, 0, false, cSize});
            return mods;
        };

        const auto cMismatch = RequiredMods::Compare(cListed, player({{"sized.ESP", 99}, {"Plain.esp", 1234}}));
        REQUIRE(cMismatch.Missing == std::vector<Listed>{{"Gone.esp", 5}});
        REQUIRE(cMismatch.DifferentVersion == std::vector<Listed>{{"Sized.esp", 100}});
        REQUIRE_FALSE(cMismatch.Empty());

        REQUIRE(RequiredMods::Compare(cListed, player({{"Sized.esp", 100}, {"Plain.esp", 1}, {"Gone.esp", 5}, {"Extra.esp", 9}})).Empty());
        // A size the player's game couldn't read isn't refused
        REQUIRE(RequiredMods::Compare(cListed, player({{"Sized.esp", 0}, {"Plain.esp", 1}, {"Gone.esp", 5}})).Empty());
    }
}
