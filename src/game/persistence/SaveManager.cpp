#include "game/persistence/SaveManager.hpp"
#include "core/logging/Logger.hpp"
#include "game/persistence/GlobalSaveData.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/PlayerProfile.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "raylib.h"
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace NoMoreDay {

namespace {

void MigrateLegacySpecializedSlots(const uint32_t saveVersion,
                                   ActiveSkillsComponent& skills) {
  if (saveVersion >= CURRENT_CHARACTER_SAVE_VERSION) {
    return;
  }

  for (auto& slot : skills.specialized_slots) {
    if (slot.skill_id == 0) {
      slot.skill_id = INVALID_SKILL_ID;
    }
  }
}

} // namespace

CharacterSaveData SaveManager::createSnapshot(entt::registry &registry) {
  CharacterSaveData data;
  data.header.version = CURRENT_CHARACTER_SAVE_VERSION;

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
        data.inventory.push_back(ItemFactory::serializeItem(registry, itemEntity));
      }
    }
  }

  // Equipment
  if (registry.all_of<EquipmentComponent>(playerEntity)) {
    const auto &eq = registry.get<EquipmentComponent>(playerEntity);
    for (auto itemEntity : eq.slots) {
      if (registry.valid(itemEntity)) {
        data.equipment.push_back(ItemFactory::serializeItem(registry, itemEntity));
      }
    }
  }

  // Progression
  if (registry.all_of<ActiveSkillsComponent>(playerEntity))
    data.skills = registry.get<ActiveSkillsComponent>(playerEntity);
  if (registry.all_of<SkillContractRuntimeComponent>(playerEntity)) {
    const auto &runtime =
        registry.get<SkillContractRuntimeComponent>(playerEntity);
    data.skill_contract_runtime.version = runtime.version;

    std::unordered_map<uint32_t, SkillContractRuntimeSkillSaveData> by_skill;
    for (const auto &[skill_id, transmuter_node] :
         runtime.active_transmuter_node_by_skill) {
      by_skill[skill_id].skill_id = skill_id;
      by_skill[skill_id].active_transmuter_node = transmuter_node;
    }
    for (const auto &[node_id, remaining] : runtime.trigger_cooldowns) {
      const uint32_t skill_id = node_id / 100;
      auto &entry = by_skill[skill_id];
      entry.skill_id = skill_id;
      entry.trigger_cooldowns.push_back({.node_id = node_id, .remaining = remaining});
    }
    data.skill_contract_runtime.skills.reserve(by_skill.size());
    for (auto &[_, entry] : by_skill) {
      data.skill_contract_runtime.skills.push_back(std::move(entry));
    }
  }
  if (registry.all_of<AstrolabeComponent>(playerEntity))
    data.astrolabe = registry.get<AstrolabeComponent>(playerEntity);
  if (registry.all_of<BladeMasteryComponent>(playerEntity)) {
    data.blade_mastery = registry.get<BladeMasteryComponent>(playerEntity);
  }
  if (registry.all_of<BladeResourceComponent>(playerEntity)) {
    data.blade_resource = registry.get<BladeResourceComponent>(playerEntity);
  }
  if (registry.all_of<BladeSignatureSkillComponent>(playerEntity)) {
    data.blade_signature_skill =
        registry.get<BladeSignatureSkillComponent>(playerEntity);
  }

  // Stash
  if (registry.all_of<PersonalStashComponent>(playerEntity)) {
      const auto& stash = registry.get<PersonalStashComponent>(playerEntity);
      SerializedStash sStash;
      sStash.unlockedTabs = stash.unlockedTabs;
      
      for (const auto& tab : stash.tabs) {
          SerializedStashTab sTab;
          sTab.name = tab.name;
          sTab.type = tab.type;
          sTab.iconId = tab.iconId;
          sTab.color = tab.color;
          
          for (int i = 0; i < StashTab::CAPACITY; ++i) {
              if (registry.valid(tab.items[i])) {
                  SerializedStashSlot slot;
                  slot.slotIndex = i;
                  slot.item = ItemFactory::serializeItem(registry, tab.items[i]);
                  sTab.items.push_back(slot);
              }
          }
          sStash.tabs.push_back(sTab);
      }
      data.personalStash = sStash;
  }

  // Combat History (Nemesis System)
  if (registry.all_of<PlayerCombatHistory>(playerEntity)) {
    data.combatHistory = registry.get<PlayerCombatHistory>(playerEntity);
  }

  // Header
  data.header.name = "玩家0";
  if (const auto *playerName = registry.try_get<PlayerName>(playerEntity);
      playerName && !playerName->value.empty()) {
    data.header.name = playerName->value;
  }
  data.header.characterClass = "SwordCultivator";
  if (const auto *stats = registry.try_get<PlayerStats>(playerEntity)) {
    data.header.level = stats->level;
  } else if (const auto *level = registry.try_get<PlayerLevel>(playerEntity)) {
    data.header.level = level->value;
  }
  data.header.playtime = 0;
  if (const auto *playtime = registry.try_get<PlayerPlaytime>(playerEntity)) {
    const double now = static_cast<double>(GetTime());
    const double elapsed = (std::max)(0.0, now - playtime->session_start_time);
    data.header.playtime = playtime->NonNegativeAccumulated() +
                           static_cast<int64_t>(std::floor(elapsed));
  }
  data.header.timestamp = std::time(nullptr);
  return data;
}

void SaveManager::restoreFromSnapshot(entt::registry &registry,
                                      const CharacterSaveData &data) {
  // Suspend Global State (SharedStash) as its entities are about to be destroyed
  SharedStash::Get().suspend(registry);

  registry.clear();

  // Resume Global State (Re-create entities)
  SharedStash::Get().resume(registry);

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<PlayerName>(player, data.header.name.empty()
                                           ? std::string("玩家0")
                                           : data.header.name);
  registry.emplace<PlayerPlaytime>(
      player, (std::max)(int64_t{0}, data.header.playtime),
      static_cast<double>(GetTime()));
  registry.emplace<PlayerLevel>(player, data.header.level);
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
  ActiveSkillsComponent restoredSkills = data.skills;
  MigrateLegacySpecializedSlots(data.header.version, restoredSkills);
  registry.emplace<ActiveSkillsComponent>(player, restoredSkills);
  auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
  runtime.version = data.skill_contract_runtime.version;
  runtime.active_transmuter_node_by_skill.clear();
  runtime.trigger_cooldowns.clear();
  for (const auto &entry : data.skill_contract_runtime.skills) {
    if (entry.active_transmuter_node != 0) {
      runtime.active_transmuter_node_by_skill[entry.skill_id] =
          entry.active_transmuter_node;
    }
    for (const auto &cd : entry.trigger_cooldowns) {
      if (cd.remaining > 0.0f) {
        runtime.trigger_cooldowns[cd.node_id] = cd.remaining;
      }
    }
  }
  registry.emplace<AstrolabeComponent>(player, data.astrolabe);
  registry.emplace<PlayerCombatHistory>(player, data.combatHistory);
  if (data.blade_mastery.has_value()) {
    registry.emplace<BladeMasteryComponent>(player, data.blade_mastery.value());
  }
  if (data.blade_resource.has_value()) {
    auto resource = data.blade_resource.value();
    resource.time_since_last_gain = 0.0f;
    resource.last_crit_bonus_time = -999.0f;
    resource.crit_bonus_feedback_timer = 0.0f;
    resource.restart_window_timer = 0.0f;
    resource.restart_window_ready = false;
    resource.decay_tick_timer = 0.0f;
    resource.hit_tracking.clear();
    registry.emplace<BladeResourceComponent>(player, resource);
    systems::BladeResourceService::SyncLegacySwordIntent(registry, player);
  }
  if (data.blade_signature_skill.has_value()) {
    registry.emplace<BladeSignatureSkillComponent>(player,
                                                   data.blade_signature_skill.value());
  }
  systems::BladeMasteryService::RefreshPlayerState(registry, player);

  // Stash
  if (data.personalStash.has_value()) {
      auto& stash = registry.emplace<PersonalStashComponent>(player);
      stash.unlockedTabs = data.personalStash->unlockedTabs;
      stash.tabs.resize(stash.unlockedTabs); 
      
      const auto& sTabs = data.personalStash->tabs;
      for (size_t i = 0; i < sTabs.size(); ++i) {
          if (i >= stash.tabs.size()) break; 
          auto& t = stash.tabs[i];
          const auto& sT = sTabs[i];
          
          t.name = sT.name;
          t.type = sT.type;
          t.iconId = sT.iconId;
          t.color = sT.color;
          
          for (const auto& slot : sT.items) {
              if (slot.slotIndex >= 0 && slot.slotIndex < StashTab::CAPACITY) {
                  t.items[slot.slotIndex] = ItemFactory::restoreItem(registry, slot.item);
              }
          }
      }
  } else {
      registry.emplace<PersonalStashComponent>(player);
  }

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

bool SaveManager::loadGlobal(entt::registry& registry) {
    std::string path = "saves/global.json";
    if (!fs::exists(path)) {
        SharedStash::Get().initialize(); // Ensure initialized
        return true; 
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        
        nlohmann::json j;
        file >> j;
        GlobalSaveData data = j.get<GlobalSaveData>();
        
        nlohmann::json jStash = data.sharedStash;
        SharedStash::Get().fromJson(jStash, registry);
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("SaveManager: Failed to load global save: {}", e.what());
        SharedStash::Get().initialize(); // Fallback
        return false;
    }
}

std::future<bool> SaveManager::saveGlobalAsync(entt::registry& registry) {
    if (!m_executor) {
        std::promise<bool> p; p.set_value(false); return p.get_future();
    }
    
    // Create snapshot on main thread
    GlobalSaveData data;
    data.sharedStash = SharedStash::Get().toJson(registry).get<SerializedStash>();
    
    return m_executor->async([data]() {
        try {
            fs::create_directories("saves");
            std::string path = "saves/global.json";
            std::string temp = "saves/global.tmp";
            
            nlohmann::json j = data;
            std::ofstream file(temp);
            if (!file.is_open()) return false;
            
            file << j.dump(4);
            file.close();
            
            fs::rename(temp, path);
            LOG_INFO("SaveManager: Global save success.");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("SaveManager: Global save failed: {}", e.what());
            return false;
        }
    });
}

} // namespace NoMoreDay
