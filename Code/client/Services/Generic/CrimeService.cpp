#include <Services/CrimeService.h>

#include <Services/TransportService.h>
#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Events/UpdateEvent.h>
#include <Messages/RequestCrimeAuthority.h>

#include <Forms/TESFaction.h>
#include <Games/ActorExtension.h>
#include <PlayerCharacter.h>
#include <WireProtocol.h>

namespace
{
constexpr std::chrono::seconds kScanInterval{1};
// A refused guard (another offender holds it) is asked for again after this
constexpr std::chrono::seconds kRetryInterval{10};
// Guards this close (game units, about 85 m) can see the player; farther ones are left to whoever runs them
constexpr float kGuardRange = 6000.f;
} // namespace

CrimeService::CrimeService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&CrimeService::OnUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&CrimeService::OnDisconnected>(this);
}

void CrimeService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_reportedBounties.clear();
    m_askedGuards.clear();
}

void CrimeService::OnUpdate(const UpdateEvent&) noexcept
{
    const auto cNow = std::chrono::steady_clock::now();
    // Official v1.8.4 servers have no such message: there, guards stay with whoever runs them
    if (cNow < m_nextScan || !m_transport.IsOnline() || Wire::IsOfficial184())
        return;
    m_nextScan = cNow + kScanInterval;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return;

    // One request per crime faction: the bounty, and its guards near the player not yet asked for
    std::map<uint32_t, RequestCrimeAuthority> requests;
    const glm::vec3 cPlayerPosition{pPlayer->position.x, pPlayer->position.y, pPlayer->position.z};

    const auto askFor = [&](const uint32_t aFormId, const uint32_t aServerId, const uint32_t aOwnershipEpoch)
    {
        Actor* pActor = Cast<Actor>(TESForm::GetById(aFormId));
        if (!pActor || !pActor->IsGuard() || pActor->IsDead() || pActor->GetExtension()->IsPlayer())
            return;
        if (glm::distance(cPlayerPosition, glm::vec3{pActor->position.x, pActor->position.y, pActor->position.z}) > kGuardRange)
            return;

        TESFaction* pCrimeFaction = pActor->GetCrimeFaction();
        if (!pCrimeFaction || pPlayer->GetCrimeGoldValue(pCrimeFaction) == 0)
            return;

        if (const auto cAsked = m_askedGuards.find(aServerId); cAsked != m_askedGuards.end() && cAsked->second.OwnershipEpoch == aOwnershipEpoch && cNow < cAsked->second.Retry)
            return;
        m_askedGuards[aServerId] = {aOwnershipEpoch, cNow + kRetryInterval};

        auto& guards = requests[pCrimeFaction->formID].Guards;
        if (guards.size() < RequestCrimeAuthority::kMaxGuards)
            guards.push_back({aServerId, aOwnershipEpoch});
    };

    const auto remoteView = m_world.view<FormIdComponent, RemoteComponent>();
    for (const auto cEntity : remoteView)
    {
        const auto& cRemote = remoteView.get<RemoteComponent>(cEntity);
        askFor(remoteView.get<FormIdComponent>(cEntity).Id, cRemote.Id, cRemote.OwnershipEpoch);
    }
    // Guards this game already runs: named too, so the party leader can't take them while the bounty lasts
    const auto localView = m_world.view<FormIdComponent, LocalComponent>();
    for (const auto cEntity : localView)
    {
        const auto& cLocal = localView.get<LocalComponent>(cEntity);
        askFor(localView.get<FormIdComponent>(cEntity).Id, cLocal.Id, cLocal.OwnershipEpoch);
    }

    for (const auto& [cFactionId, cBounty] : m_reportedBounties)
        requests.try_emplace(cFactionId);

    for (auto& [cFactionId, request] : requests)
    {
        const TESFaction* pFaction = Cast<TESFaction>(TESForm::GetById(cFactionId));
        request.Bounty = pFaction ? pPlayer->GetCrimeGoldValue(pFaction) : 0;

        const auto cReported = m_reportedBounties.find(cFactionId);
        const uint32_t cLastBounty = cReported != m_reportedBounties.end() ? cReported->second : 0;
        if (request.Bounty == cLastBounty && request.Guards.empty())
            continue;

        if (!m_world.GetModSystem().GetServerModId(cFactionId, request.Faction))
        {
            spdlog::warn("Crime faction {:X} isn't from a plugin the server knows, its guards stay where they are", cFactionId);
            continue;
        }

        spdlog::info("Bounty {} with crime faction {:X}, asking for {} guards", request.Bounty, cFactionId, request.Guards.size());
        m_transport.Send(request);
        if (request.Bounty == 0)
            m_reportedBounties.erase(cFactionId);
        else
            m_reportedBounties[cFactionId] = request.Bounty;
    }
}
