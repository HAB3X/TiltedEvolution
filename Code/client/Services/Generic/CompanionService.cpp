#include <Services/CompanionService.h>

#include <Services/TransportService.h>
#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Events/UpdateEvent.h>
#include <Messages/RequestCompanions.h>

#include <Games/ActorExtension.h>
#include <PlayerCharacter.h>
#include <WireProtocol.h>

namespace
{
constexpr std::chrono::seconds kScanInterval{1};
// A refused claim (another player holds the actor) is tried again this often
constexpr std::chrono::seconds kResendInterval{10};
// Companions this close (game units, about 85 m) travel with the player; farther ones were left behind
constexpr float kCompanionRange = 6000.f;
} // namespace

CompanionService::CompanionService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&CompanionService::OnUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&CompanionService::OnDisconnected>(this);
}

void CompanionService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_sent.clear();
}

void CompanionService::OnUpdate(const UpdateEvent&) noexcept
{
    const auto cNow = std::chrono::steady_clock::now();
    // Official v1.8.4 servers have no such message: there, companions stay with whoever runs them
    if (cNow < m_nextScan || !m_transport.IsOnline() || Wire::IsOfficial184())
        return;
    m_nextScan = cNow + kScanInterval;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return;
    const Actor* pHorse = pPlayer->GetLastRiddenMount();
    const glm::vec3 cPlayerPosition{pPlayer->position.x, pPlayer->position.y, pPlayer->position.z};

    Vector<std::pair<uint32_t, uint32_t>> companions;
    const auto consider = [&](const uint32_t aFormId, const uint32_t aServerId, const uint32_t aOwnershipEpoch)
    {
        Actor* pActor = Cast<Actor>(TESForm::GetById(aFormId));
        if (!pActor || pActor->IsDead() || pActor->GetExtension()->IsPlayer() || pActor->IsPlayerSummon())
            return;
        if (!pActor->IsPlayerTeammate() && pActor != pHorse)
            return;
        if (glm::distance(cPlayerPosition, glm::vec3{pActor->position.x, pActor->position.y, pActor->position.z}) > kCompanionRange)
            return;
        if (companions.size() < RequestCompanions::kMaxCompanions)
            companions.emplace_back(aServerId, aOwnershipEpoch);
    };

    const auto remoteView = m_world.view<FormIdComponent, RemoteComponent>();
    for (const auto cEntity : remoteView)
    {
        const auto& cRemote = remoteView.get<RemoteComponent>(cEntity);
        consider(remoteView.get<FormIdComponent>(cEntity).Id, cRemote.Id, cRemote.OwnershipEpoch);
    }
    const auto localView = m_world.view<FormIdComponent, LocalComponent>();
    for (const auto cEntity : localView)
    {
        const auto& cLocal = localView.get<LocalComponent>(cEntity);
        consider(localView.get<FormIdComponent>(cEntity).Id, cLocal.Id, cLocal.OwnershipEpoch);
    }

    std::sort(companions.begin(), companions.end());
    if (companions == m_sent && cNow < m_nextResend)
        return;
    m_nextResend = cNow + kResendInterval;
    // Nothing to say: no companions now, and none were named before
    if (companions.empty() && m_sent.empty())
        return;

    RequestCompanions request;
    for (const auto& [cServerId, cEpoch] : companions)
        request.Companions.push_back({cServerId, cEpoch});
    m_transport.Send(request);
    if (companions != m_sent)
        spdlog::info("Companions near the player: {}", companions.size());
    m_sent = std::move(companions);
}
