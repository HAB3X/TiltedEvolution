#include <Services/ReturnService.h>

#include <GameServer.h>
#include <World.h>
#include <Services/CommandService.h>

namespace
{
// Long enough to restart the game after a crash and load a save
constexpr uint64_t kRememberMs = 10 * 60 * 1000;
} // namespace

ReturnService::ReturnService(World& aWorld) noexcept
    : m_world(aWorld)
{
}

String ReturnService::KeyOf(const Player& acPlayer) noexcept
{
    return acPlayer.GetUsername() + "@" + acPlayer.GetEndpoint();
}

void ReturnService::OnLost(Player& aPlayer) noexcept
{
    const uint64_t cNow = GameServer::Get()->GetTick();
    std::erase_if(m_lost, [&](const Lost& acLost) { return acLost.Key == KeyOf(aPlayer) || cNow - acLost.LostAt > kRememberMs; });

    Lost lost;
    lost.Key = KeyOf(aPlayer);
    lost.PartyId = aPlayer.GetParty().JoinedPartyId;
    lost.WasLeader = m_world.GetPartyService().IsPlayerLeader(&aPlayer);
    lost.Where = CommandService::WhereIs(m_world, aPlayer);
    lost.LostAt = cNow;
    spdlog::info("Lost the connection to {}: remembering their party and place for {} minutes", lost.Key.c_str(), kRememberMs / 60000);
    m_lost.push_back(std::move(lost));
}

void ReturnService::OnJoined(Player& aPlayer, bool aReturning) noexcept
{
    const auto cIt = std::find_if(m_lost.begin(), m_lost.end(), [&](const Lost& acLost) { return acLost.Key == KeyOf(aPlayer); });
    if (cIt == m_lost.end())
        return;

    const Lost cLost = *cIt;
    m_lost.erase(cIt);
    if (GameServer::Get()->GetTick() - cLost.LostAt > kRememberMs)
        return;

    if (cLost.PartyId)
    {
        const bool cRejoined = m_world.GetPartyService().Rejoin(&aPlayer, *cLost.PartyId, cLost.WasLeader);
        spdlog::info("{} is back: {}", cLost.Key.c_str(), cRejoined ? "rejoined their party" : "their party is gone or they joined another");
    }

    // A live reconnect keeps the player where their game is; a restarted game goes back to where the server last saw them
    if (aReturning && cLost.Where.CellId != GameId{})
    {
        spdlog::info("Moving {} back to where they were", cLost.Key.c_str());
        aPlayer.Send(cLost.Where);
    }
}
