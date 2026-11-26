#include <Services/WorldSaveService.h>

#include <Components.h>
#include <GameServer.h>
#include <World.h>
#include <Game/WorldSave.h>
#include <Services/CharacterService.h>

#include <Events/UpdateEvent.h>
#include <Setting.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
Console::Setting bKeepWorld{"Gameplay:bKeepWorld", "In a host world: saves what the server keeps of the world (containers, NPCs killed while nobody was near) to world/world.dat, and loads it on start, so the party can continue later", true};
Console::StringSetting sKeepWorldName{"Gameplay:sKeepWorldName", "Which kept world a host world uses: world/<name>.dat. Hosting from the game sets the host's character, so each playthrough keeps its own", "world"};
Console::Setting uKeepWorldSeconds{"Gameplay:uKeepWorldSeconds", "How often a host world's server saves the world it keeps, in seconds (it also saves when it stops)", 300u};

const std::filesystem::path kFolder = "world";
constexpr int kBackups = 3;

// Letters, digits, '_' and '-' only: it becomes a file name
std::string WorldName()
{
    const std::string cName = sKeepWorldName.value();
    const bool cValid = !cName.empty() && std::all_of(cName.begin(), cName.end(), [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
    if (!cValid)
        spdlog::error("Gameplay:sKeepWorldName \"{}\" can't be a file name (letters, digits, _ and - only): keeping the world as \"world\"", cName);
    return cValid ? cName : "world";
}

std::filesystem::path File()
{
    return kFolder / (WorldName() + ".dat");
}

std::filesystem::path Backup(int aIndex)
{
    return kFolder / (WorldName() + "." + std::to_string(aIndex) + ".dat");
}
} // namespace

WorldSaveService::WorldSaveService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&WorldSaveService::OnUpdate>(this))
{
    if (IsActive())
        Load();
}

bool WorldSaveService::IsActive() const noexcept
{
    return bKeepWorld && GameServer::Get()->IsHostWorld();
}

void WorldSaveService::OnUpdate(const UpdateEvent&) noexcept
{
    const uint64_t cNow = GameServer::Get()->GetTick();
    if (cNow < m_nextSave)
        return;
    const bool cFirst = m_nextSave == 0;
    m_nextSave = cNow + std::max(uKeepWorldSeconds.value_as<uint32_t>(), 1u) * 1000ull;
    // Not right at start: there's nothing new to keep
    if (!cFirst)
        Save();
}

void WorldSaveService::Save() noexcept
{
    if (!IsActive())
        return;

    WorldSave save;
    const auto& mods = m_world.ctx().at<ModsComponent>();
    for (const auto& [cName, cEntry] : mods.GetStandardMods())
        save.Plugins.push_back({cName, cEntry.id, false});
    for (const auto& [cName, cEntry] : mods.GetLiteMods())
        save.Plugins.push_back({cName, cEntry.id, true});

    // Killed and unloaded, then killed and still loaded
    Map<GameId, CharacterService::DeadLoot> deadActors = m_world.GetCharacterService().GetDeadActors();
    for (auto entity : m_world.view<CharacterComponent, FormIdComponent, InventoryComponent>())
    {
        const auto& cCharacter = m_world.get<CharacterComponent>(entity);
        const auto& cInventory = m_world.get<InventoryComponent>(entity);
        if (cCharacter.IsDead() && !cCharacter.IsPlayerSummon())
            deadActors[m_world.get<FormIdComponent>(entity).Id] = {cInventory.Content, cInventory.PlayerCopies};
    }
    for (const auto& [cId, cLoot] : deadActors)
    {
        WorldSave::DeadActor actor{cId, cLoot.Content};
        for (const auto& [cName, cCopy] : cLoot.PlayerCopies)
            actor.PlayerCopies.push_back({cName, cCopy});
        save.DeadActors.push_back(std::move(actor));
    }

    for (auto entity : m_world.view<FormIdComponent, ObjectComponent, CellIdComponent, InventoryComponent>())
    {
        const auto& cObject = m_world.get<ObjectComponent>(entity);
        if (!cObject.IsContainer)
            continue;
        const auto& cCell = m_world.get<CellIdComponent>(entity);
        WorldSave::Container container;
        container.Object.Id = m_world.get<FormIdComponent>(entity).Id;
        container.Object.CellId = cCell.Cell;
        container.Object.WorldSpaceId = cCell.WorldSpaceId;
        container.Object.CurrentCoords = cCell.CenterCoords;
        container.Object.CurrentLockData = cObject.CurrentLockData;
        container.Object.CurrentInventory = m_world.get<InventoryComponent>(entity).Content;
        container.Object.IsContainer = true;
        container.MergedPlayers.assign(cObject.MergedPlayers.begin(), cObject.MergedPlayers.end());
        for (const auto& [cName, cCopy] : m_world.get<InventoryComponent>(entity).PlayerCopies)
            container.PlayerCopies.push_back({cName, cCopy});
        save.Containers.push_back(std::move(container));
    }

    std::error_code error;
    std::filesystem::create_directories(kFolder, error);
    const std::filesystem::path cTemporary = kFolder / (WorldName() + ".dat.tmp");
    {
        std::ofstream file(cTemporary, std::ios::binary | std::ios::trunc);
        const std::string cBytes = save.Serialize();
        file.write(cBytes.data(), static_cast<std::streamsize>(cBytes.size()));
        if (!file)
        {
            spdlog::error("Couldn't write {}: the world isn't saved", cTemporary.string());
            return;
        }
    }

    // world.dat becomes world.1.dat, world.1.dat world.2.dat, and so on
    std::filesystem::remove(Backup(kBackups), error);
    for (int i = kBackups - 1; i >= 1; --i)
        std::filesystem::rename(Backup(i), Backup(i + 1), error);
    std::filesystem::rename(File(), Backup(1), error);
    std::filesystem::rename(cTemporary, File(), error);
    if (error)
    {
        spdlog::error("Couldn't replace {} ({}): the new world is in {}", File().string(), error.message(), cTemporary.string());
        return;
    }
    spdlog::info("Saved the world to {}: {} containers, {} dead NPCs", File().string(), save.Containers.size(), save.DeadActors.size());
}

void WorldSaveService::Load() noexcept
{
    std::error_code error;
    if (!std::filesystem::exists(File(), error))
    {
        spdlog::info("No saved world in {}: this host world starts fresh", File().string());
        return;
    }

    std::ifstream file(File(), std::ios::binary);
    std::stringstream bytes;
    bytes << file.rdbuf();
    const auto cSave = WorldSave::Parse(bytes.str());
    if (!cSave)
    {
        spdlog::error("{} isn't a world this server can read: starting fresh. Its backups ({}) are untouched until the next save", File().string(),
                      Backup(1).string());
        return;
    }

    // The saved GameIds use the plugin ids of then: register the plugins again in id order, which gives them the same ids
    auto plugins = cSave->Plugins;
    std::sort(plugins.begin(), plugins.end(), [](const auto& acLeft, const auto& acRight) { return acLeft.Id < acRight.Id; });
    auto& mods = m_world.ctx().at<ModsComponent>();
    for (const WorldSave::Plugin& cPlugin : plugins)
    {
        const uint32_t cId = cPlugin.IsLite ? mods.AddLite(cPlugin.Name) : mods.AddStandard(cPlugin.Name);
        if (cId != cPlugin.Id)
        {
            spdlog::error("{} gives {} the id {}, but it got {} now: the saved world isn't loaded", File().string(), cPlugin.Name.c_str(), cPlugin.Id, cId);
            return;
        }
    }

    Map<GameId, CharacterService::DeadLoot> deadActors;
    for (const WorldSave::DeadActor& cActor : cSave->DeadActors)
    {
        auto& loot = deadActors[cActor.Id];
        loot.Content = cActor.Loot;
        for (const WorldSave::PlayerCopy& cCopy : cActor.PlayerCopies)
            loot.PlayerCopies[cCopy.Name] = cCopy.Content;
    }
    m_world.GetCharacterService().RestoreDeadActors(std::move(deadActors));

    for (const WorldSave::Container& cContainer : cSave->Containers)
    {
        const ObjectData& cObject = cContainer.Object;
        const auto cEntity = m_world.create();
        m_world.emplace<FormIdComponent>(cEntity, cObject.Id);
        auto& object = m_world.emplace<ObjectComponent>(cEntity);
        object.CurrentLockData = cObject.CurrentLockData;
        object.IsContainer = true;
        object.MergedPlayers.insert(cContainer.MergedPlayers.begin(), cContainer.MergedPlayers.end());
        m_world.emplace<CellIdComponent>(cEntity, cObject.CellId, cObject.WorldSpaceId, cObject.CurrentCoords);
        auto& inventory = m_world.emplace<InventoryComponent>(cEntity);
        inventory.Content = cObject.CurrentInventory;
        for (const WorldSave::PlayerCopy& cCopy : cContainer.PlayerCopies)
            inventory.PlayerCopies[cCopy.Name] = cCopy.Content;
    }

    spdlog::info("Loaded the world from {}: {} containers, {} dead NPCs, {} plugins", File().string(), cSave->Containers.size(), cSave->DeadActors.size(),
                 cSave->Plugins.size());
}
