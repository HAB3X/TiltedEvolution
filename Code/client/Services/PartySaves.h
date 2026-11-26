#pragma once

/**
 * @brief The overlay's Saves page: the co-op saves Skyrim Together made (BGSSaveLoadManager::QueueSaveAs and the
 * party checkpoints), listed from the game's save folder and loaded through the game. Never changes a save.
 */
namespace PartySaves
{
// Sends the overlay a partySaves event (JSON): {"character", "folder", "saves": [{"name", "time"}]}, or {"error"}
// when the save folder can't be read. Main thread: it reads the player's name.
void Send() noexcept;

// Loads one of the listed saves by its file name without .ess; anything else is refused and logged
void Load(const std::string& acName) noexcept;
} // namespace PartySaves
