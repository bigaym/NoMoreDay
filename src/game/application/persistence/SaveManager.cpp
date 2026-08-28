#include "game/application/persistence/SaveManager.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/UUID.hpp"
#include "engine/resource/AssetRegistry.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerProfile.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/SharedStash.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageConverter.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemStorageTypes.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "raylib.h"
#include <tracy/Tracy.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;

namespace NoMoreDay {

ItemStorageService *SaveManager::GetItemStorageService(entt::registry *registry) noexcept {
  if (m_itemStorage) {
    return m_itemStorage;
  }
  if (registry) {
    if (auto *ctx = GetSharedContext(*registry)) {
      if (ctx->itemStorage) {
        return ctx->itemStorage;
      }
    }
    if (registry->ctx().contains<ItemStorageService *>()) {
      return registry->ctx().get<ItemStorageService *>();
    }
  }
  return nullptr;
}

nlohmann::json SaveManager::BuildProgressionJson(const CharacterSaveData &charData) {
  nlohmann::json progJson = {
      {"header", charData.header},
      {"primaryStats", charData.primaryStats},
      {"position", charData.position},
      {"mapId", charData.mapId},
      {"skills", charData.skills},
      {"skill_contract_runtime", charData.skill_contract_runtime},
      {"astrolabe", charData.astrolabe},
      {"combatHistory", charData.combatHistory}
  };
  if (charData.blade_mastery.has_value()) {
    progJson["blade_mastery"] = charData.blade_mastery.value();
  }
  if (charData.blade_resource.has_value()) {
    progJson["blade_resource"] = charData.blade_resource.value();
  }
  if (charData.blade_signature_skill.has_value()) {
    progJson["blade_signature_skill"] = charData.blade_signature_skill.value();
  }
  return progJson;
}

CharacterSaveData SaveManager::ParseProgressionJson(const nlohmann::json &progJson) {
  CharacterSaveData charData;
  charData.header = progJson.value("header", SaveHeader{});
  charData.primaryStats = progJson.value("primaryStats", PrimaryStats{});
  charData.position = progJson.value("position", Position{});
  charData.mapId = progJson.value("mapId", std::string("Town_01"));
  charData.skills = progJson.value("skills", ActiveSkillsComponent{});
  charData.skill_contract_runtime = progJson.value("skill_contract_runtime", SkillContractRuntimeSaveData{});
  charData.astrolabe = progJson.value("astrolabe", AstrolabeComponent{});
  charData.combatHistory = progJson.value("combatHistory", PlayerCombatHistory{});
  if (progJson.contains("blade_mastery")) {
    charData.blade_mastery = progJson["blade_mastery"].get<BladeMasteryComponent>();
  }
  if (progJson.contains("blade_resource")) {
    charData.blade_resource = progJson["blade_resource"].get<BladeResourceComponent>();
  }
  if (progJson.contains("blade_signature_skill")) {
    charData.blade_signature_skill = progJson["blade_signature_skill"].get<BladeSignatureSkillComponent>();
  }
  return charData;
}

CharacterSaveData SaveManager::createSnapshot(entt::registry &registry) {
  ZoneScopedN("SaveManager::createSnapshot");
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

  // Combat History
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
  ZoneScopedN("SaveManager::restoreFromSnapshot");

  // 重置世界状态
  registry.clear();

  auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  registry.emplace<IDComponent>(player, Utils::UUID::from("Player"));
  registry.emplace<Radius>(player, 5.0f);
  registry.emplace<GPUIndex>(player, -1);
  registry.emplace<Velocity>(player, 0.0f, 0.0f);
  registry.emplace<InputComponent>(player);
  auto &pStats = registry.emplace<PlayerStats>(player);
  pStats.level = data.header.level;
  registry.emplace<CombatStats>(player);
  registry.emplace<VisionComponent>(player, 600.0f);
  registry.emplace<StatsDirty>(player);
  registry.emplace<DashComponent>(player);
  registry.emplace<MovementStanceComponent>(player);
  registry.emplace<MovementAccumulator>(player);
  registry.emplace<AttackState>(player);
  registry.emplace<TextureIDComponent>(player, assets::textures::Player_Warrior.id);
  registry.emplace<PlayerName>(player, data.header.name.empty()
                                           ? std::string("玩家0")
                                           : data.header.name);
  registry.emplace<PlayerPlaytime>(
      player, (std::max)(int64_t{0}, data.header.playtime),
      static_cast<double>(GetTime()));
  registry.emplace<PlayerLevel>(player, data.header.level);
  registry.emplace<Position>(player, data.position);
  registry.emplace<PrimaryStats>(player, data.primaryStats);

  ItemStorageService *storage = GetItemStorageService(&registry);

  // 单轨模式：物品数据已由 ItemPersistenceCodec 直接解码灌入 ItemStorageService
  // 玩家实体挂载轻量 ECS 容器组件镜像标量，无需创建任何重复的 ECS Item 实体
  auto &inv = registry.emplace<InventoryComponent>(player);
  if (storage) {
    inv.gold = storage->getGold();
    inv.capacity = static_cast<int32_t>(storage->getInventorySlots().size());
  } else {
    inv.gold = 0;
    inv.capacity = InventoryComponent::BASE_CAPACITY;
  }
  inv.items.assign(inv.capacity, entt::null);
  inv.bag_slots.fill(entt::null);

  registry.emplace<MaterialBankComponent>(player);
  registry.emplace<EquipmentComponent>(player);
  auto &stash = registry.emplace<PersonalStashComponent>(player);
  if (storage) {
    stash.unlockedTabs = storage->getUnlockedPages(ContainerKind::PersonalStash);
    stash.tabs.resize(stash.unlockedTabs);
    const auto &metas = storage->getPersonalStashMeta();
    for (size_t i = 0; i < stash.tabs.size() && i < metas.size(); ++i) {
      stash.tabs[i].name = metas[i].name;
      stash.tabs[i].type = static_cast<StashTabType>(metas[i].type);
      stash.tabs[i].iconId = metas[i].iconId;
      stash.tabs[i].color = metas[i].color;
    }
  }

  // Skills & Astrolabe
  registry.emplace<ActiveSkillsComponent>(player, data.skills);

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
    registry.emplace<BladeSignatureSkillComponent>(
        player, data.blade_signature_skill.value());
  }
  systems::BladeMasteryService::RefreshPlayerState(registry, player);

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

  // 0. 防并发重入守卫：若已有异步存档在执行，直接拒绝避免文件与状态写竞争
  if (m_isSaving.exchange(true, std::memory_order_acq_rel)) {
    LOG_WARN("SaveManager: Save operation is already in-flight, rejecting concurrent save request.");
    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
  }

  ItemStorageService *storage = GetItemStorageService(&registry);
  if (!storage) {
    LOG_ERROR("SaveManager: ItemStorageService missing, saveCharacterAsync rejected to prevent silent data loss.");
    m_isSaving.store(false, std::memory_order_release);
    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
  }
  ItemStorageService storageSnapshot = *storage;

  // 1. 主线程快速构建非物品 Progression 数据与快照
  CharacterSaveData charData = createSnapshot(registry);
  nlohmann::json progJson = BuildProgressionJson(charData);
  std::string progressionPayload = progJson.dump();

  std::string savePath = getSavePath(slotIndex);
  std::string tempPath = getTempPath(slotIndex);

  return m_executor->async([storageSnapshot = std::move(storageSnapshot),
                            progressionPayload = std::move(progressionPayload),
                            savePath = std::move(savePath),
                            tempPath = std::move(tempPath),
                            &isSaving = m_isSaving]() {
    struct InFlightGuard {
      std::atomic<bool> &flag;
      ~InFlightGuard() { flag.store(false, std::memory_order_release); }
    } guard{isSaving};

    try {
      std::stringstream ss;
      // 传递 nullptr 禁用全局裸缓存，消除跨线程无锁并发写竞争 (High 17)
      if (!ItemPersistenceCodec::encode(storageSnapshot, ss, nullptr,
                                        ContainerDirtyFlags::All, progressionPayload)) {
        LOG_ERROR("SaveManager: Failed to encode binary save for {}", savePath);
        return false;
      }

      std::string str = ss.str();
      std::vector<uint8_t> binaryData(str.begin(), str.end());

      if (!ItemPersistenceCodec::SaveFileAtomic(savePath, tempPath, binaryData, true)) {
        LOG_ERROR("SaveManager: Atomic save failed for {}", savePath);
        return false;
      }

      LOG_INFO("SaveManager: Successfully saved character binary to {}", savePath);
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR("SaveManager: Exception during character save: {}", e.what());
      return false;
    }
  });
}

bool SaveManager::loadCharacter(entt::registry &registry, int slotIndex) {
  ItemStorageService *storage = GetItemStorageService(&registry);
  if (!storage) {
    LOG_ERROR("SaveManager: ItemStorageService missing, loadCharacter rejected");
    return false;
  }

  std::string binaryPath = getSavePath(slotIndex);
  std::string backupPath = getBackupPath(slotIndex);

  // 1. 优先读取主档 saves/slot_N.nmd
  if (fs::exists(binaryPath)) {
    std::ifstream file(binaryPath, std::ios::binary);
    std::string progressionPayload;
    ItemStorageService loadedStorage;
    if (file.is_open() && ItemPersistenceCodec::decode(file, loadedStorage, &progressionPayload)) {
      try {
        nlohmann::json progJson = nlohmann::json::parse(progressionPayload);
        CharacterSaveData charData = ParseProgressionJson(progJson);
        if (charData.header.version != CURRENT_CHARACTER_SAVE_VERSION) {
          LOG_ERROR("SaveManager: Save version {} does not match current version {}, rejecting save.",
                    charData.header.version, CURRENT_CHARACTER_SAVE_VERSION);
        } else {
          *storage = std::move(loadedStorage);
          restoreFromSnapshot(registry, charData);
          LOG_INFO("SaveManager: Successfully loaded character from binary {}", binaryPath);
          return true;
        }
      } catch (const std::exception &e) {
        LOG_ERROR("SaveManager: Failed to parse progression from {}: {}", binaryPath, e.what());
      }
    } else {
      LOG_WARN("SaveManager: Failed to decode binary save {}, attempting backup...", binaryPath);
    }
  }

  // 1b. 若主档损坏或读取失败，尝试读取备份档 saves/slot_N.nmd.bak
  if (fs::exists(backupPath)) {
    std::ifstream file(backupPath, std::ios::binary);
    std::string progressionPayload;
    ItemStorageService loadedStorage;
    if (file.is_open() && ItemPersistenceCodec::decode(file, loadedStorage, &progressionPayload)) {
      try {
        nlohmann::json progJson = nlohmann::json::parse(progressionPayload);
        CharacterSaveData charData = ParseProgressionJson(progJson);
        if (charData.header.version != CURRENT_CHARACTER_SAVE_VERSION) {
          LOG_ERROR("SaveManager: Backup save version {} does not match current version {}, rejecting save.",
                    charData.header.version, CURRENT_CHARACTER_SAVE_VERSION);
        } else {
          *storage = std::move(loadedStorage);
          restoreFromSnapshot(registry, charData);
          LOG_INFO("SaveManager: Successfully recovered character from backup {}", backupPath);
          return true;
        }
      } catch (const std::exception &e) {
        LOG_ERROR("SaveManager: Failed to parse progression from backup {}: {}", backupPath, e.what());
      }
    }
  }

  // 2. 检测旧版 JSON 存档并提示，不再执行兼容导入
  std::string oldJsonPath = (slotIndex == -1) ? "saves/quicksave.json" : "saves/slot_" + std::to_string(slotIndex) + ".json";
  if (fs::exists(oldJsonPath)) {
    LOG_WARN("SaveManager: Detected old JSON save {}, JSON import is removed. Starting new game.", oldJsonPath);
  }

  LOG_WARN("SaveManager: Save file {} (and backups) does not exist or failed to load.", binaryPath);
  return false;
}

std::string SaveManager::getSavePath(int slotIndex) const {
  if (slotIndex == -1) {
    return "saves/quicksave.nmd";
  }
  return "saves/slot_" + std::to_string(slotIndex) + ".nmd";
}

std::string SaveManager::getTempPath(int slotIndex) const {
  if (slotIndex == -1) {
    return "saves/temp/quicksave.tmp";
  }
  return "saves/temp/slot_" + std::to_string(slotIndex) + ".tmp";
}

std::string SaveManager::getBackupPath(int slotIndex) const {
  return getSavePath(slotIndex) + ".bak";
}

bool SaveManager::loadGlobal(entt::registry &registry) {
  std::string binaryPath = "saves/global.nmd";
  std::string backupPath = "saves/global.nmd.bak";

  ItemStorageService *storage = GetItemStorageService(&registry);

  // 1. 优先读取 saves/global.nmd
  if (fs::exists(binaryPath)) {
    std::ifstream file(binaryPath, std::ios::binary);
    ItemStorageService loadedStorage;
    if (file.is_open() && ItemPersistenceCodec::decode(file, loadedStorage)) {
      if (storage) {
        storage->setUnlockedPages(ContainerKind::SharedStash,
                                  loadedStorage.getUnlockedPages(ContainerKind::SharedStash));
        const auto &pages = loadedStorage.getSharedStashPages();
        for (size_t p = 0; p < pages.size(); ++p) {
          for (size_t s = 0; s < pages[p].size(); ++s) {
            ItemHandle h = pages[p][s];
            if (h) {
              const auto *inst = loadedStorage.getStore().get(h);
              if (inst) {
                const auto *side = loadedStorage.getStore().getSideTable(h);
                ItemHandle newH = storage->getStoreMutable().create(*inst);
                if (side && !side->empty()) {
                  storage->getStoreMutable().setSideTable(newH, *side);
                }
                storage->setSlotHandle(
                    SlotRef{ContainerKind::SharedStash, 0, static_cast<uint16_t>(p),
                            static_cast<uint16_t>(s)},
                    newH);
              }
            }
          }
        }
        storage->setSharedStashMeta(loadedStorage.getSharedStashMeta());
      }
      LOG_INFO("SaveManager: Global save loaded from binary {}", binaryPath);
      return true;
    }
  }

  // 1b. 备份档回退
  if (fs::exists(backupPath)) {
    std::ifstream file(backupPath, std::ios::binary);
    ItemStorageService loadedStorage;
    if (file.is_open() && ItemPersistenceCodec::decode(file, loadedStorage)) {
      if (storage) {
        storage->setUnlockedPages(ContainerKind::SharedStash,
                                  loadedStorage.getUnlockedPages(ContainerKind::SharedStash));
        const auto &pages = loadedStorage.getSharedStashPages();
        for (size_t p = 0; p < pages.size(); ++p) {
          for (size_t s = 0; s < pages[p].size(); ++s) {
            ItemHandle h = pages[p][s];
            if (h) {
              const auto *inst = loadedStorage.getStore().get(h);
              if (inst) {
                const auto *side = loadedStorage.getStore().getSideTable(h);
                ItemHandle newH = storage->getStoreMutable().create(*inst);
                if (side && !side->empty()) {
                  storage->getStoreMutable().setSideTable(newH, *side);
                }
                storage->setSlotHandle(
                    SlotRef{ContainerKind::SharedStash, 0, static_cast<uint16_t>(p),
                            static_cast<uint16_t>(s)},
                    newH);
              }
            }
          }
        }
        storage->setSharedStashMeta(loadedStorage.getSharedStashMeta());
      }
      LOG_INFO("SaveManager: Global save recovered from backup {}", backupPath);
      return true;
    }
  }

  auto *ctx = GetSharedContext(registry);
  SharedStash *sharedStash = ctx ? ctx->sharedStash : nullptr;

  // 2. 检测旧版 saves/global.json 并提示，不再执行兼容导入
  if (fs::exists("saves/global.json")) {
    LOG_WARN("SaveManager: Detected old global.json, JSON import is removed.");
  }

  if (sharedStash) {
    sharedStash->initialize();
  }
  return true;
}

std::future<bool> SaveManager::saveGlobalAsync(entt::registry &registry) {
  if (!m_executor) {
    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
  }

  auto *ctx = GetSharedContext(registry);
  SharedStash *sharedStash = ctx ? ctx->sharedStash : nullptr;

  ItemStorageService *storage = GetItemStorageService(&registry);
  if (!storage) {
    LOG_ERROR("SaveManager: ItemStorageService missing, saveGlobalAsync rejected to prevent silent data loss.");
    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
  }

  ItemStorageService snapshot;
  snapshot.getStoreMutable() = storage->getStore();
  snapshot.setUnlockedPages(ContainerKind::SharedStash,
                            storage->getUnlockedPages(ContainerKind::SharedStash));
  const auto &sharedPages = storage->getSharedStashPages();
  for (size_t p = 0; p < sharedPages.size(); ++p) {
    for (size_t s = 0; s < sharedPages[p].size(); ++s) {
      if (sharedPages[p][s]) {
        snapshot.setSlotHandle(
            SlotRef{ContainerKind::SharedStash, 0, static_cast<uint16_t>(p), static_cast<uint16_t>(s)},
            sharedPages[p][s]);
      }
    }
  }
  snapshot.setSharedStashMeta(storage->getSharedStashMeta());

  std::string targetPath = "saves/global.nmd";
  std::string tempPath = "saves/temp/global.tmp";

  return m_executor->async([snapshot = std::move(snapshot),
                            targetPath = std::move(targetPath),
                            tempPath = std::move(tempPath)]() {
    try {
      std::stringstream ss;
      uint32_t mask = ContainerDirtyFlags::TemplateFingerprint |
                      ContainerDirtyFlags::ItemInstances |
                      ContainerDirtyFlags::ItemSideTables |
                      ContainerDirtyFlags::SharedStash;
      if (!ItemPersistenceCodec::encode(snapshot, ss, nullptr, mask, "")) {
        LOG_ERROR("SaveManager: Failed to encode global binary save");
        return false;
      }

      std::string str = ss.str();
      std::vector<uint8_t> binaryData(str.begin(), str.end());

      if (!ItemPersistenceCodec::SaveFileAtomic(targetPath, tempPath, binaryData, true)) {
        LOG_ERROR("SaveManager: Atomic save failed for global save");
        return false;
      }

      LOG_INFO("SaveManager: Global save success (binary).");
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR("SaveManager: Global save failed: {}", e.what());
      return false;
    }
  });
}

} // namespace NoMoreDay
