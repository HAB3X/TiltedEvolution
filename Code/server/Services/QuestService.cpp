#include <GameServer.h>
#include <Components.h>

#include <World.h>
#include <Services/QuestService.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/NotifyQuestUpdate.h>

#include <Setting.h>
#include <SharedQuests.h>
namespace
{
Console::StringSetting sSharedQuestTypes{"Gameplay:sSharedQuestTypes",
                                         "Quest categories shared with the party, comma separated: Main, MagesGuild, ThievesGuild, DarkBrotherhood, Companions, Misc, "
                                         "Daedric, Side, CivilWar, Dawnguard, Dragonborn (Misc also covers quests with no category)",
                                         SharedQuests::kDefaultText};
// The older switch for Misc, kept so servers that turned it on keep sharing them
Console::Setting bEnableMiscQuestSync{"Gameplay:bEnableMiscQuestSync", "(Experimental) Syncs miscellaneous quests when possible, as if Misc were in sSharedQuestTypes", false};

}

uint32_t QuestService::GetSharedQuestTypes() noexcept
{
    // Parsed again only when the setting changes, so a bad name is reported once
    static std::string s_parsedText;
    static uint32_t s_mask = SharedQuests::kDefault;
    if (s_parsedText != sSharedQuestTypes.value())
    {
        s_parsedText = sSharedQuestTypes.value();
        std::vector<std::string> unknown;
        s_mask = SharedQuests::Parse(s_parsedText, unknown);
        for (const std::string& cName : unknown)
            spdlog::warn("Gameplay:sSharedQuestTypes: unknown quest category '{}', ignored", cName);
    }
    return s_mask | (bEnableMiscQuestSync ? SharedQuests::Bit(SharedQuests::kMisc) : 0u);
}

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_questUpdateConnection = aDispatcher.sink<PacketEvent<RequestQuestUpdate>>().connect<&QuestService::OnQuestChanges>(this);
}

void QuestService::OnQuestChanges(const PacketEvent<RequestQuestUpdate>& acMessage) noexcept
{
    const auto& message = acMessage.Packet;

    auto* pPlayer = acMessage.pPlayer;

    auto& questComponent = pPlayer->GetQuestLogComponent();
    auto& entries = questComponent.QuestContent.Entries;

    auto questIt = std::find_if(entries.begin(), entries.end(), [&message](const auto& e) { return e.Id == message.Id; });

    NotifyQuestUpdate notify{};
    notify.Id = message.Id;
    notify.Stage = message.Stage;
    notify.Status = message.Status;
    notify.ClientQuestType = message.ClientQuestType;

    if (!SharedQuests::IsShared(GetSharedQuestTypes(), message.ClientQuestType))
    {
        spdlog::debug("{}: quest category {} isn't shared, not relaying gameId {:X} questStage {} questStatus {}", __FUNCTION__, message.ClientQuestType,
                      notify.Id.LogFormat(), notify.Stage, notify.Status);
        return;
    }

    if (message.Status == RequestQuestUpdate::Started || message.Status == RequestQuestUpdate::StageUpdate)
    {
        // in order to prevent bugs when a quest is in progress
        // and being updated we add it as a new quest record to
        // maintain a proper remote questlog state.
        if (questIt == entries.end())
        {
            auto& newQuest = entries.emplace_back();
            newQuest.Id = message.Id;
            newQuest.Stage = message.Stage;

            if (message.Status == RequestQuestUpdate::Started)
            {
                spdlog::debug("Started quest: {:X} stage: {}", message.Id.LogFormat(), message.Stage);

                notify.Status = NotifyQuestUpdate::Started;
            }
            else
            {
                notify.Status = NotifyQuestUpdate::StageUpdate;
            }
        }
        else
        {
            spdlog::debug("Updated quest: {:X}, stage: {}", message.Id.LogFormat(), message.Stage);

            auto& record = *questIt;
            record.Id = message.Id;
            record.Stage = message.Stage;

            notify.Status = NotifyQuestUpdate::StageUpdate;
        }
    }
    else if (message.Status == RequestQuestUpdate::Stopped)
    {
        spdlog::debug("Stopped quest: {:X}, stage: {}", message.Id.LogFormat(), message.Stage);

        if (questIt != entries.end())
            entries.erase(questIt);
        else
            spdlog::warn("Unable to delete quest object {:X}", message.Id.LogFormat());

        notify.Status = NotifyQuestUpdate::Stopped;
    }

    const auto& partyComponent = acMessage.pPlayer->GetParty();
    PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(pPlayer);
    if (!pParty)
        return;

    // Party members' scripts often reach the same stage on their own, only relay it once
    if (message.Status == RequestQuestUpdate::StageUpdate)
    {
        if (!pParty->QuestStages[message.Id].insert(message.Stage).second)
        {
            spdlog::debug("Dropped duplicate quest stage from player {}, quest: {:X}, stage: {}", pPlayer->GetId(), message.Id.LogFormat(), message.Stage);
            return;
        }
    }
    else
    {
        // A started quest has run none of its stages yet, a stopped one may run them again when restarted
        pParty->QuestStages.erase(message.Id);
    }

    GameServer::Get()->SendToParty(notify, partyComponent, acMessage.GetSender());
}
