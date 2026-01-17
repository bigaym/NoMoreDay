#include "engine/persistence/SaveManager.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace NoMoreDay {

// Helper to serialize an item entity to DTO
static SerializedItem SerializeItem(entt::registry &registry,
                                    entt::entity entity) {
  SerializedItem dto;
  if (!registry.all_of<ItemComponent>(entity))
    return dto;

  const auto &item = registry.get<ItemComponent>(entity);
  dto.itemId = item.id;
  dto.name = item.name;
  dto.type = item.type;
  dto.textureId = item.textureId;
  dto.quantity = item.quantity;

  // Note: If ItemComponent had a level field, we'd save it here.
  // For now we use a default of 1 or leave it uninitialized if not used.
  dto.stats.rarity = item.rarity;
  dto.stats.slot = item.slot;
  dto.stats.attack = item.attack;
  dto.stats.defense = item.defense;
  dto.stats.forgingPotential = item.forgingPotential;
  dto.stats.legendaryPotential = item.legendaryPotential;
  dto.stats.value = item.value;

  for (const auto &aff : item.affixes) {
    SerializedItem::SavedAffix sAff;
    sAff.type = aff.type;
    sAff.tier = aff.tier;
    sAff.value = aff.value;
    sAff.isPrefix = aff.isPrefix;
    sAff.isLegendary = aff.isLegendary;
    // sAff.name = aff.name; // REMOVED
    sAff.required_tags = aff.required_tags;
    dto.affixes.push_back(sAff);
  }

  for (const auto &aff : item.implicits) {
    SerializedItem::SavedAffix sAff;
    sAff.type = aff.type;
    sAff.tier = aff.tier;
    sAff.value = aff.value;
    sAff.isPrefix = aff.isPrefix;
    sAff.isLegendary = aff.isLegendary;
    // sAff.name = aff.name; // REMOVED
    sAff.required_tags = aff.required_tags;
    dto.implicits.push_back(sAff);
  }

  for (auto socketEntity : item.sockets) {
    if (registry.valid(socketEntity)) {
      dto.socketedItems.push_back(SerializeItem(registry, socketEntity));
    }
  }

  return dto;
}

CharacterSaveData SaveManager::createSnapshot(entt::registry &registry) {
  CharacterSaveData data;

  auto view = registry.view<PlayerTag>();
  if (view.begin() == view.end()) {
    LOG_WARN("SaveManager: No player entity found for snapshot.");
    return data;
  }

  auto playerEntity = *view.begin();

  // Core data
  if (registry.all_of<Position>(playerEntity))
    data.position = registry.get<Position>(playerEntity);
  if (registry.all_of<PrimaryStats>(playerEntity))
    data.primaryStats = registry.get<PrimaryStats>(playerEntity);

  // Economy & Inventory
  if (registry.all_of<InventoryComponent>(playerEntity)) {
    const auto &inv = registry.get<InventoryComponent>(playerEntity);
    data.gold = inv.gold;

    for (auto itemEntity : inv.items) {
      if (registry.valid(itemEntity)) {
        data.inventory.push_back(SerializeItem(registry, itemEntity));
      }
    }
  }

  // Equipment
  if (registry.all_of<EquipmentComponent>(playerEntity)) {
    const auto &eq = registry.get<EquipmentComponent>(playerEntity);
    for (auto itemEntity : eq.slots) {
      if (registry.valid(itemEntity)) {
        data.equipment.push_back(SerializeItem(registry, itemEntity));
      }
    }
  }

  // Progression
  if (registry.all_of<ActiveSkillsComponent>(playerEntity))
    data.skills = registry.get<ActiveSkillsComponent>(playerEntity);
  if (registry.all_of<AstrolabeComponent>(playerEntity))
    data.astrolabe = registry.get<AstrolabeComponent>(playerEntity);

  // Header
  data.header.name = "Hero"; // TODO: Implement name selection
  data.header.characterClass = "SwordCultivator";
  data.header.playtime = 0; // TODO: Implement playtime tracking
  data.header.timestamp = std::time(nullptr);
  data.header.version = 1;

  return data;
}

void SaveManager::restoreFromSnapshot(entt::registry &registry,
                                      const CharacterSaveData &data) {
  registry.clear();

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<Position>(player, data.position);
  registry.emplace<PrimaryStats>(player, data.primaryStats);

  // Inventory
  auto &inv = registry.emplace<InventoryComponent>(player);
  inv.gold = data.gold;

  // Clear the default null items and fill from snapshot
  inv.items.clear();
  for (const auto &itemDto : data.inventory) {
    inv.items.push_back(ItemFactory::restoreItem(registry, itemDto));
  }
  // Pad to capacity
  while (inv.items.size() < inv.capacity) {
    inv.items.push_back(entt::null);
  }

  // Equipment
  auto &eq = registry.emplace<EquipmentComponent>(player);
  for (const auto &itemDto : data.equipment) {
    auto itemEntity = ItemFactory::restoreItem(registry, itemDto);
    eq.set(itemDto.stats.slot, itemEntity);
  }

  // Skills & Astrolabe
  registry.emplace<ActiveSkillsComponent>(player, data.skills);
  registry.emplace<AstrolabeComponent>(player, data.astrolabe);

  // Re-apply traits if necessary (e.g. SwordHeart if activated in Astrolabe)
  // This usually happens in a progression system or during recount.

  LOG_INFO("SaveManager: Character restored from snapshot.");
}

std::future<bool> SaveManager::saveCharacterAsync(entt::registry &registry,
                                                  int slotIndex) {
  if (!m_executor) {
    LOG_ERROR("SaveManager: No executor initialized.");
    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
  }

  // snapshot on main thread
  CharacterSaveData data = createSnapshot(registry);
  std::string path = getSavePath(slotIndex);
  std::string tempPath = getTempPath(slotIndex);

  return m_executor->async([data, path, tempPath]() {
    try {
      // Ensure directory exists
      fs::path dir = fs::path(path).parent_path();
      if (!fs::exists(dir)) {
        fs::create_directories(dir);
      }
      fs::create_directories(fs::path(tempPath).parent_path());

      // Serialize to temp file
      nlohmann::json j = data;
      std::ofstream file(tempPath);
      if (!file.is_open())
        return false;

      file << j.dump(4);
      file.close();

      // Atomic rename
      fs::rename(tempPath, path);

      LOG_INFO("SaveManager: Successfully saved to {}", path);
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR("SaveManager: Failed to save: {}", e.what());
      return false;
    }
  });
}

bool SaveManager::loadCharacter(entt::registry &registry, int slotIndex) {
  std::string path = getSavePath(slotIndex);
  if (!fs::exists(path)) {
    LOG_WARN("SaveManager: Save file {} does not exist.", path);
    return false;
  }

  try {
    std::ifstream file(path);
    if (!file.is_open())
      return false;

    nlohmann::json j;
    file >> j;
    CharacterSaveData data = j.get<CharacterSaveData>();

    restoreFromSnapshot(registry, data);
    return true;
  } catch (const std::exception &e) {
    LOG_ERROR("SaveManager: Failed to load: {}", e.what());
    return false;
  }
}

std::string SaveManager::getSavePath(int slotIndex) const {
  return "saves/slot_" + std::to_string(slotIndex) + ".json";
}

std::string SaveManager::getTempPath(int slotIndex) const {
  return "saves/temp/slot_" + std::to_string(slotIndex) + ".tmp";
}

} // namespace NoMoreDay
