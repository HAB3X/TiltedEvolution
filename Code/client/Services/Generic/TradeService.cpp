#include <Services/TradeService.h>

#include <Services/OverlayService.h>
#include <Services/TransportService.h>
#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Messages/NotifyTrade.h>
#include <Messages/RequestTrade.h>

#include <OverlayApp.hpp>
#include <PlayerCharacter.h>
#include <Trade.h>
#include <WireProtocol.h>

namespace
{
// An item as the window shows it: name, count and whether it is enchanted
std::string ItemJson(const Inventory::Entry& acEntry) noexcept
{
    const TESForm* pForm = TESForm::GetById(World::Get().GetModSystem().GetGameId(acEntry.BaseId));
    const char* pName = pForm ? pForm->GetName() : nullptr;
    const std::string cName = pName && *pName ? pName : fmt::format("{:X}:{:X}", acEntry.BaseId.ModId, acEntry.BaseId.BaseId);
    return fmt::format(R"({{"name":{},"count":{},"enchanted":{}}})", OverlayService::ToJsonString(cName), acEntry.Count, acEntry.ExtraEnchantId != GameId{} ? "true" : "false");
}

std::string ItemsJson(const Vector<Inventory::Entry>& acEntries) noexcept
{
    std::string json;
    for (const Inventory::Entry& cEntry : acEntries)
        json += (json.empty() ? "" : ",") + ItemJson(cEntry);
    return "[" + json + "]";
}
} // namespace

TradeService::TradeService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_tradeConnection = aDispatcher.sink<NotifyTrade>().connect<&TradeService::OnTrade>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&TradeService::OnDisconnected>(this);
}

void TradeService::Send(const uint8_t aAction, const Inventory& acOffer) noexcept
{
    // Official v1.8.4 servers have no trading
    if (m_partnerId == 0 || !m_transport.IsOnline() || Wire::IsOfficial184())
        return;
    RequestTrade request;
    request.PartnerId = m_partnerId;
    request.Act = static_cast<RequestTrade::Action>(aAction);
    request.Offer = acOffer;
    m_transport.Send(request);
}

void TradeService::Start(const uint32_t aPartnerId) noexcept
{
    m_partnerId = aPartnerId;
    SendInventory();
    Send(static_cast<uint8_t>(RequestTrade::Action::kOffer));
}

void TradeService::Offer(const Vector<std::pair<uint32_t, int32_t>>& acPicks) noexcept
{
    Inventory offer;
    for (const auto& [cIndex, cCount] : acPicks)
    {
        if (cIndex >= m_tradeable.size() || cCount <= 0)
        {
            spdlog::warn("Trade window picked stack {} ({}), which isn't in the list it was sent", cIndex, cCount);
            continue;
        }
        Inventory::Entry entry = m_tradeable[cIndex];
        entry.Count = std::min(cCount, entry.Count);
        offer.Entries.push_back(std::move(entry));
    }
    Send(static_cast<uint8_t>(RequestTrade::Action::kOffer), offer);
}

void TradeService::Confirm() noexcept
{
    Send(static_cast<uint8_t>(RequestTrade::Action::kConfirm));
}

void TradeService::Cancel() noexcept
{
    Send(static_cast<uint8_t>(RequestTrade::Action::kCancel));
    m_partnerId = 0;
}

void TradeService::SendInventory() noexcept
{
    m_tradeable.clear();
    if (PlayerCharacter* pPlayer = PlayerCharacter::Get())
    {
        for (const Inventory::Entry& cEntry : pPlayer->GetActorInventory().Entries)
        {
            Inventory single;
            single.Entries.push_back(cEntry);
            if (Trade::IsValidOffer(single))
                m_tradeable.push_back(cEntry);
        }
    }

    auto pArguments = CefListValue::Create();
    pArguments->SetString(0, ItemsJson(m_tradeable));
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("tradeInventory", pArguments);
}

void TradeService::OnTrade(const NotifyTrade& acMessage) noexcept
{
    using Event = NotifyTrade::Event;
    // A partner opening a trade with this player: the window opens here too
    if (m_partnerId != acMessage.PartnerId && acMessage.What == Event::kUpdated)
    {
        m_partnerId = acMessage.PartnerId;
        SendInventory();
    }
    // After a trade the inventory changed: the window picks from the new one next time
    if (acMessage.What == Event::kCancelled || acMessage.What == Event::kCompleted || acMessage.What == Event::kUnavailable)
        m_partnerId = 0;
    else if (acMessage.What == Event::kMissingItems)
        SendInventory();

    auto pArguments = CefListValue::Create();
    pArguments->SetInt(0, static_cast<int>(acMessage.PartnerId));
    pArguments->SetString(1, acMessage.PartnerName.c_str());
    pArguments->SetInt(2, static_cast<int>(acMessage.What));
    pArguments->SetString(3, ItemsJson(acMessage.MyOffer.Entries));
    pArguments->SetString(4, ItemsJson(acMessage.TheirOffer.Entries));
    pArguments->SetBool(5, acMessage.MyConfirmed);
    pArguments->SetBool(6, acMessage.TheirConfirmed);
    m_world.GetOverlayService().GetOverlayApp()->ExecuteAsync("trade", pArguments);
}

void TradeService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_partnerId = 0;
    m_tradeable.clear();
}
