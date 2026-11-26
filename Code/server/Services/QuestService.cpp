#include <GameServer.h>
#include <Components.h>

#include <World.h>
#include <Services/QuestService.h>

#include <Messages/RequestQuestUpdate.h>
#include <Messages/NotifyQuestUpdate.h>
#include <Messages/NotifyQuestDesync.h>
#include <Messages/RequestQuestStages.h>
#include <Messages/RequestPartyQuestSharing.h>
#include <Messages/NotifyPartyQuestSharing.h>
#include <Messages/RequestQuestShare.h>
#include <Messages/NotifyQuestShareOffer.h>
#include <Messages/RequestQuestShareAnswer.h>
#include <Messages/NotifyQuestCatchUp.h>

#include <QuestDesync.h>

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

uint32_t QuestService::GetSharedQuestTypes(const PartyService::Party& acParty) noexcept
{
    return acParty.QuestSharingEnabled ? acParty.SharedQuestTypes.value_or(GetSharedQuestTypes()) : 0u;
}

void QuestService::SendQuestSharing(const PartyService::Party& acParty, Player& aPlayer) noexcept
{
    NotifyPartyQuestSharing notify;
    notify.Enabled = acParty.QuestSharingEnabled;
    notify.Types = acParty.SharedQuestTypes.value_or(GetSharedQuestTypes());
    aPlayer.Send(notify);
}

QuestService::QuestService(World& aWorld, entt::dispatcher& aDispatcher)
    : m_world(aWorld)
{
    m_questUpdateConnection = aDispatcher.sink<PacketEvent<RequestQuestUpdate>>().connect<&QuestService::OnQuestChanges>(this);
    m_questStagesConnection = aDispatcher.sink<PacketEvent<RequestQuestStages>>().connect<&QuestService::OnQuestStages>(this);
    m_partyQuestSharingConnection = aDispatcher.sink<PacketEvent<RequestPartyQuestSharing>>().connect<&QuestService::OnPartyQuestSharing>(this);
    m_questShareConnection = aDispatcher.sink<PacketEvent<RequestQuestShare>>().connect<&QuestService::OnQuestShare>(this);
    m_questShareAnswerConnection = aDispatcher.sink<PacketEvent<RequestQuestShareAnswer>>().connect<&QuestService::OnQuestShareAnswer>(this);
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

    // Relayed to the party when it shares the quest's kind, else among the members who took it when one offered it
    PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(pPlayer);
    PartyService::Party::OfferedQuest* pOffered = nullptr;
    if (!SharedQuests::IsShared(pParty ? GetSharedQuestTypes(*pParty) : GetSharedQuestTypes(), message.ClientQuestType))
    {
        if (pParty)
        {
            auto offeredIt = pParty->OfferedQuests.find(message.Id);
            if (offeredIt != pParty->OfferedQuests.end() && offeredIt->second.Players.contains(pPlayer->GetId()))
                pOffered = &offeredIt.value();
        }
        if (!pOffered)
        {
            spdlog::debug("{}: quest category {} isn't shared, not relaying gameId {:X} questStage {} questStatus {}", __FUNCTION__, message.ClientQuestType,
                          notify.Id.LogFormat(), notify.Stage, notify.Status);
            return;
        }
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

    if (!pOffered)
    {
        GameServer::Get()->SendToParty(notify, pPlayer->GetParty(), acMessage.GetSender());
        return;
    }

    if (message.Status != RequestQuestUpdate::Stopped)
        pOffered->Stage = message.Stage;
    for (Player* pMember : pParty->Members)
    {
        if (pMember != pPlayer && pOffered->Players.contains(pMember->GetId()))
            pMember->Send(notify);
    }
}

void QuestService::OnPartyQuestSharing(const PacketEvent<RequestPartyQuestSharing>& acMessage) noexcept
{
    Player* pPlayer = acMessage.pPlayer;
    PartyService& partyService = m_world.GetPartyService();
    PartyService::Party* pParty = partyService.GetPlayerParty(pPlayer);
    if (!pParty || !partyService.IsPlayerLeader(pPlayer))
    {
        spdlog::warn("{} isn't a party leader, not changing quest sharing", pPlayer->GetUsername().c_str());
        return;
    }

    pParty->QuestSharingEnabled = acMessage.Packet.Enabled;
    pParty->SharedQuestTypes = acMessage.Packet.Types;
    spdlog::info("{} turned their party's quest sharing {}, kinds {:X}", pPlayer->GetUsername().c_str(), acMessage.Packet.Enabled ? "on" : "off", acMessage.Packet.Types);
    for (Player* pMember : pParty->Members)
        SendQuestSharing(*pParty, *pMember);
}

void QuestService::OnQuestShare(const PacketEvent<RequestQuestShare>& acMessage) noexcept
{
    Player* pPlayer = acMessage.pPlayer;
    const RequestQuestShare& cShare = acMessage.Packet;
    PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(pPlayer);
    if (!pParty)
    {
        spdlog::warn("{} isn't in a party, not offering quest {:X}", pPlayer->GetUsername().c_str(), cShare.Id.LogFormat());
        return;
    }
    if (SharedQuests::IsShared(GetSharedQuestTypes(*pParty), cShare.ClientQuestType))
    {
        spdlog::warn("{} offered quest {:X}, which the party shares already", pPlayer->GetUsername().c_str(), cShare.Id.LogFormat());
        return;
    }

    PartyService::Party::OfferedQuest& offered = pParty->OfferedQuests[cShare.Id];
    offered.Players.insert(pPlayer->GetId());
    offered.Stage = cShare.Stage;
    offered.ClientQuestType = cShare.ClientQuestType;

    NotifyQuestShareOffer offer;
    offer.PlayerId = pPlayer->GetId();
    offer.PlayerName = pPlayer->GetUsername();
    offer.Id = cShare.Id;
    for (Player* pMember : pParty->Members)
    {
        if (offered.Players.contains(pMember->GetId()))
            continue;
        offered.Asked.insert(pMember->GetId());
        pMember->Send(offer);
    }
    spdlog::info("{} offered quest {:X} at stage {} to their party", pPlayer->GetUsername().c_str(), cShare.Id.LogFormat(), cShare.Stage);
}

void QuestService::OnQuestShareAnswer(const PacketEvent<RequestQuestShareAnswer>& acMessage) noexcept
{
    Player* pPlayer = acMessage.pPlayer;
    const RequestQuestShareAnswer& cAnswer = acMessage.Packet;
    PartyService::Party* pParty = m_world.GetPartyService().GetPlayerParty(pPlayer);
    PartyService::Party::OfferedQuest* pOffered = nullptr;
    if (pParty)
    {
        if (auto offeredIt = pParty->OfferedQuests.find(cAnswer.Id); offeredIt != pParty->OfferedQuests.end())
            pOffered = &offeredIt.value();
    }
    if (!pOffered || pOffered->Asked.erase(pPlayer->GetId()) == 0)
    {
        spdlog::warn("{} answered an offer of quest {:X} they weren't asked about", pPlayer->GetUsername().c_str(), cAnswer.Id.LogFormat());
        return;
    }
    if (!cAnswer.Accept)
        return;

    pOffered->Players.insert(pPlayer->GetId());

    // The offered quest's current stage, which starts it where it isn't running yet
    NotifyQuestUpdate update;
    update.Id = cAnswer.Id;
    update.Stage = pOffered->Stage;
    update.Status = NotifyQuestUpdate::StageUpdate;
    update.ClientQuestType = pOffered->ClientQuestType;
    pPlayer->Send(update);
    spdlog::info("{} took the party's quest {:X} at stage {}", pPlayer->GetUsername().c_str(), cAnswer.Id.LogFormat(), pOffered->Stage);
}

void QuestService::OnQuestStages(const PacketEvent<RequestQuestStages>& acMessage) noexcept
{
    Player* pPlayer = acMessage.pPlayer;
    pPlayer->GetQuestLogComponent().ReportedStages = acMessage.Packet.Stages;

    PartyService& partyService = m_world.GetPartyService();
    const PartyService::Party* pParty = partyService.GetPlayerParty(pPlayer);
    if (!pParty)
        return;

    Player* pLeader = m_world.GetPlayerManager().GetById(pParty->LeaderPlayerId);
    if (!pLeader)
        return;

    if (pLeader != pPlayer)
    {
        CheckDesync(*pLeader, *pPlayer);
        return;
    }

    for (Player* pMember : pParty->Members)
    {
        if (pMember != pLeader)
            CheckDesync(*pLeader, *pMember);
    }
}

void QuestService::CheckDesync(const Player& acLeader, Player& aMember) const noexcept
{
    const auto& cLeaderLog = acLeader.GetQuestLogComponent().ReportedStages;
    auto& memberLog = aMember.GetQuestLogComponent();
    if (!cLeaderLog || !memberLog.ReportedStages)
        return;

    // A new leader starts a new comparison
    const bool cFirstComparison = memberLog.DesyncLeaderId != acLeader.GetId();
    if (cFirstComparison)
    {
        memberLog.DesyncLeaderId = acLeader.GetId();
        memberLog.LastDifferences.clear();
        memberLog.NotifiedDifferences.clear();
    }

    Vector<GameId> differences = QuestDesync::Differences(*cLeaderLog, *memberLog.ReportedStages);

    // A player joining the party mid-quest (or getting a new leader) may take the leader's progress. In host world mode
    // the joiner takes over the host's whole world instead.
    if (cFirstComparison && !GameServer::Get()->IsHostWorld())
    {
        NotifyQuestCatchUp catchUp;
        catchUp.LeaderName = acLeader.GetUsername();
        catchUp.Quests = QuestDesync::LeaderStages(*cLeaderLog, differences);
        if (!catchUp.Quests.Entries.empty())
        {
            spdlog::info("[QuestDesync]: offering {} {}'s stages of {} quests", aMember.GetUsername().c_str(), acLeader.GetUsername().c_str(), catchUp.Quests.Entries.size());
            aMember.Send(catchUp);
        }
    }
    Vector<GameId> lasting = QuestDesync::InBoth(memberLog.LastDifferences, differences);
    memberLog.LastDifferences = std::move(differences);

    // Only when what differs changes, so the same desync is reported once
    if (lasting == memberLog.NotifiedDifferences)
        return;
    memberLog.NotifiedDifferences = lasting;
    if (lasting.empty())
    {
        spdlog::info("[QuestDesync]: {}'s quests match their leader {}'s again", aMember.GetUsername().c_str(), acLeader.GetUsername().c_str());
        return;
    }

    spdlog::info("[QuestDesync]: {} shared quests differ between {} and their leader {}", lasting.size(), aMember.GetUsername().c_str(), acLeader.GetUsername().c_str());

    NotifyQuestDesync toMember;
    toMember.OtherPlayer = acLeader.GetUsername();
    toMember.OtherIsLeader = true;
    toMember.Quests = lasting;
    toMember.LeaderStages = QuestDesync::LeaderStages(*cLeaderLog, lasting);
    aMember.Send(toMember);

    NotifyQuestDesync toLeader;
    toLeader.OtherPlayer = aMember.GetUsername();
    toLeader.OtherIsLeader = false;
    toLeader.Quests = std::move(lasting);
    acLeader.Send(toLeader);
}
