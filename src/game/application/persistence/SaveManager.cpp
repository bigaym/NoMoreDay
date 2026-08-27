#include "game/application/persistence/SaveManager.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/UUID.hpp"
#include "engine/resource/AssetRegistry.hpp"
#include "game/application/persistence/GlobalSaveData.hpp"
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

namespace {

void MigrateLegacySpecializedSlots(const uint32_t saveVersion,
                                   ActiveSkillsComponent &skills) {
  if (saveVersion >= 3) {
    return;
  }

  for (auto &slot : skills.specialized_slots) {
    if (slot.skill_id == 0) {
      slot.skill_id = INVALID_SKILL_ID;
    }
  }
}

SerializedItem SerializeItemComponentHelper(const ItemComponent &item) {
  SerializedItem dto;
  dto.itemId = item.id;
  dto.name = item.name;
  dto.type = item.type;
  dto.textureId = item.textureId;
  dto.quantity = item.quantity;
  dto.baseId = item.baseId;
  dto.weaponSubtype = item.weaponSubtype;
  dto.catalystKind = item.catalystKind;
  dto.isLocked = item.isLocked;
  dto.activeRunewordId = item.activeRunewordId;
  dto.socketCount = item.socketCount;
  dto.setName = item.setName;
  dto.bagCapacity = item.bagCapacity;
  dto.conversions = item.conversions;
  dto.damageModifiers = item.damage_modifiers;

  dto.stats.rarity = item.rarity;
  dto.stats.level = item.itemLevel;
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
    sAff.required_tags = aff.required_tags;
    sAff.modifier_record_ids = aff.modifier_record_ids;
    dto.affixes.push_back(sAff);
  }

  for (const auto &aff : item.implicits) {
    SerializedItem::SavedAffix sAff;
    sAff.type = aff.type;
    sAff.tier = aff.tier;
    sAff.value = aff.value;
    sAff.isPrefix = aff.isPrefix;
    sAff.isLegendary = aff.isLegendary;
    sAff.required_tags = aff.required_tags;
    sAff.modifier_record_ids = aff.modifier_record_ids;
    dto.implicits.push_back(sAff);
  }
  return dto;
}

SerializedItem SerializeHandleToDto(ItemHandle h, const ItemStore &store) {
  const auto *inst = store.get(h);
  if (!inst) {
    return SerializedItem{};
  }
  const auto *side = store.getSideTable(h);
  ItemComponent comp = InstanceToItemComponent(*inst, side);
  auto dto = SerializeItemComponentHelper(comp);
  for (size_t i = 0; i < inst->socketCount && i < inst->sockets.size(); ++i) {
    if (inst->sockets[i]) {
      dto.socketedItems.push_back(
          {static_cast<uint8_t>(i), SerializeHandleToDto(inst->sockets[i], store)});
    }
  }
  return dto;
}

ItemHandle RestoreDtoToHandle(const SerializedItem &dto, ItemStore &store) {
  ItemComponent comp;
  comp.id = dto.itemId;
  comp.baseId = dto.baseId;
  comp.quantity = dto.quantity;
  comp.itemLevel = dto.stats.level;
  comp.rarity = dto.stats.rarity;
  comp.isLocked = dto.isLocked;
  comp.forgingPotential = dto.stats.forgingPotential;
  comp.legendaryPotential = dto.stats.legendaryPotential;
  comp.socketCount = dto.socketCount;
  comp.attack = dto.stats.attack;
  comp.defense = dto.stats.defense;
  comp.value = dto.stats.value;
  comp.activeRunewordId = dto.activeRunewordId;
  comp.conversions = dto.conversions;
  comp.damage_modifiers = dto.damageModifiers;

  for (const auto &sAff : dto.affixes) {
    Affix aff;
    aff.type = sAff.type;
    aff.tier = sAff.tier;
    aff.value = sAff.value;
    aff.isPrefix = sAff.isPrefix;
    aff.isLegendary = sAff.isLegendary;
    aff.required_tags = sAff.required_tags;
    aff.modifier_record_ids = sAff.modifier_record_ids;
    comp.affixes.push_back(aff);
  }

  ItemSideTableData side;
  ItemInstance inst = ItemComponentToInstance(comp, &side);
  for (const auto &sock : dto.socketedItems) {
    if (sock.socketIndex < inst.sockets.size()) {
      ItemHandle subH = RestoreDtoToHandle(sock.item, store);
      inst.sockets[sock.socketIndex] = subH;
    }
  }
  ItemHandle h = store.create(inst);
  if (!side.empty()) {
    store.setSideTable(h, side);
  }
  return h;
}

} // namespace

ItemStorageService *SaveManager::GetItemStorageService(entt::registry *registry) noexcept {
  if (m_itemStorage) {
    return m_itemStorage;
  }
  if (registry) {
    if (registry->ctx().contains<SharedContext *>()) {
      auto *ctx = registry->ctx().get<SharedContext *>();
      if (ctx && ctx->itemStorage) {
        return ctx->itemStorage;
      }
    }
    if (registry->ctx().contains<ItemStorageService *>()) {
      return registry->ctx().get<ItemStorageService *>();
    }
  }
  return nullptr;
}

void SaveManager::MigrateSaveDataV3toV4(CharacterSaveData &data) {
  if (data.header.version >= CURRENT_CHARACTER_SAVE_VERSION) {
    return;
  }
  if (data.inventoryCapacity <= 0) {
    data.inventoryCapacity = InventoryComponent::BASE_CAPACITY;
  }
  data.header.version = CURRENT_CHARACTER_SAVE_VERSION;
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

  ItemStorageService *storage = GetItemStorageService(&registry);

  if (storage) {
    // 优先直接从 ItemStorageService 读取只读数据（单轨模式）
    data.gold = storage->getGold();
    data.inventoryCapacity = static_cast<int32_t>(storage->getInventorySlots().size());

    const auto &invSlots = storage->getInventorySlots();
    for (size_t i = 0; i < invSlots.size(); ++i) {
      if (invSlots[i]) {
        data.inventory.push_back(
            {static_cast<int>(i), SerializeHandleToDto(invSlots[i], storage->getStore())});
      }
    }

    const auto &bagSlots = storage->getBagSlots();
    for (size_t i = 0; i < bagSlots.size(); ++i) {
      if (bagSlots[i]) {
        data.bagSlots.push_back(
            {static_cast<uint8_t>(i), SerializeHandleToDto(bagSlots[i], storage->getStore())});
      }
    }

    const auto &eqSlots = storage->getEquipmentSlots();
    for (size_t i = 0; i < eqSlots.size(); ++i) {
      if (eqSlots[i]) {
        data.equipment.push_back(SerializeHandleToDto(eqSlots[i], storage->getStore()));
      }
    }

    for (const auto &[id, count] : storage->getMaterials()) {
      data.materialBank.push_back({id, count});
    }

    const auto &stashPages = storage->getPersonalStashPages();
    const auto &stashMeta = storage->getPersonalStashMeta();
    SerializedStash sStash;
    sStash.unlockedTabs = storage->getUnlockedPages(ContainerKind::PersonalStash);
    for (size_t p = 0; p < stashPages.size() && p < static_cast<size_t>(sStash.unlockedTabs); ++p) {
      SerializedStashTab sTab;
      if (p < stashMeta.size()) {
        sTab.name = stashMeta[p].name;
        sTab.type = static_cast<StashTabType>(stashMeta[p].type);
        sTab.iconId = stashMeta[p].iconId;
        sTab.color = stashMeta[p].color;
      } else {
        sTab.name = "Stash " + std::to_string(p + 1);
      }
      for (size_t s = 0; s < stashPages[p].size(); ++s) {
        if (stashPages[p][s]) {
          sTab.items.push_back(
              {static_cast<int>(s), SerializeHandleToDto(stashPages[p][s], storage->getStore())});
        }
      }
      sStash.tabs.push_back(sTab);
    }
    data.personalStash = sStash;

  } else {
    // 兼容回退：从 ECS 组件获取（无 ItemStorageService 上下文）
    if (registry.all_of<InventoryComponent>(playerEntity)) {
      const auto &inv = registry.get<InventoryComponent>(playerEntity);
      data.gold = inv.gold;
      data.inventoryCapacity = inv.capacity;

      for (size_t i = 0; i < inv.items.size(); ++i) {
        if (registry.valid(inv.items[i])) {
          data.inventory.push_back({static_cast<int>(i), ItemFactory::serializeItem(registry, inv.items[i])});
        }
      }

      for (size_t i = 0; i < inv.bag_slots.size(); ++i) {
        if (registry.valid(inv.bag_slots[i])) {
          data.bagSlots.push_back({static_cast<uint8_t>(i), ItemFactory::serializeItem(registry, inv.bag_slots[i])});
        }
      }
    }

    if (registry.all_of<EquipmentComponent>(playerEntity)) {
      const auto &eq = registry.get<EquipmentComponent>(playerEntity);
      for (auto itemEntity : eq.slots) {
        if (registry.valid(itemEntity)) {
          data.equipment.push_back(ItemFactory::serializeItem(registry, itemEntity));
        }
      }
    }

    if (registry.all_of<MaterialBankComponent>(playerEntity)) {
      const auto &bank = registry.get<MaterialBankComponent>(playerEntity);
      for (const auto &entry : bank.materials) {
        data.materialBank.push_back({entry.id, entry.count});
      }
    }

    if (registry.all_of<PersonalStashComponent>(playerEntity)) {
      const auto &stash = registry.get<PersonalStashComponent>(playerEntity);
      SerializedStash sStash;
      sStash.unlockedTabs = stash.unlockedTabs;

      for (const auto &tab : stash.tabs) {
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
                                      const CharacterSaveData &data,
                                      bool restoreItems) {
  ZoneScopedN("SaveManager::restoreFromSnapshot");
  CharacterSaveData snapshotData = data;
  const uint32_t originalVersion = snapshotData.header.version;
  if (snapshotData.header.version < CURRENT_CHARACTER_SAVE_VERSION) {
    MigrateSaveDataV3toV4(snapshotData);
  }

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
  pStats.level = snapshotData.header.level;
  registry.emplace<CombatStats>(player);
  registry.emplace<VisionComponent>(player, 600.0f);
  registry.emplace<StatsDirty>(player);
  registry.emplace<DashComponent>(player);
  registry.emplace<MovementStanceComponent>(player);
  registry.emplace<MovementAccumulator>(player);
  registry.emplace<AttackState>(player);
  registry.emplace<TextureIDComponent>(player, assets::textures::Player_Warrior.id);
  registry.emplace<PlayerName>(player, snapshotData.header.name.empty()
                                           ? std::string("玩家0")
                                           : snapshotData.header.name);
  registry.emplace<PlayerPlaytime>(
      player, (std::max)(int64_t{0}, snapshotData.header.playtime),
      static_cast<double>(GetTime()));
  registry.emplace<PlayerLevel>(player, snapshotData.header.level);
  registry.emplace<Position>(player, snapshotData.position);
  registry.emplace<PrimaryStats>(player, snapshotData.primaryStats);

  ItemStorageService *storage = GetItemStorageService(&registry);

  if (restoreItems) {
    // 仅在从旧版 JSON 导入时，使用 snapshotData 完整填充 ItemStorageService
    if (storage) {
      storage->clearAll();
      storage->setGold(snapshotData.gold);

      for (const auto &entry : snapshotData.materialBank) {
        storage->addMaterial(entry.id, entry.count);
      }

      for (const auto &entry : snapshotData.inventory) {
        ItemHandle h = RestoreDtoToHandle(entry.item, storage->getStoreMutable());
        storage->setSlotHandle(
            SlotRef{ContainerKind::Inventory, 0, 0, static_cast<uint16_t>(entry.slotIndex)}, h);
      }

      for (const auto &entry : snapshotData.bagSlots) {
        ItemHandle h = RestoreDtoToHandle(entry.bag, storage->getStoreMutable());
        storage->setSlotHandle(SlotRef{ContainerKind::BagSlots, entry.index, 0, 0}, h);
      }

      for (const auto &itemDto : snapshotData.equipment) {
        ItemHandle h = RestoreDtoToHandle(itemDto, storage->getStoreMutable());
        storage->setSlotHandle(
            SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(itemDto.stats.slot), 0, 0}, h);
      }

      if (snapshotData.personalStash.has_value()) {
        storage->setUnlockedPages(ContainerKind::PersonalStash,
                                  snapshotData.personalStash->unlockedTabs);
        const auto &tabs = snapshotData.personalStash->tabs;
        std::vector<StashTabMeta> metas;
        for (size_t t = 0; t < tabs.size(); ++t) {
          metas.push_back({tabs[t].name, static_cast<uint8_t>(tabs[t].type), tabs[t].iconId, tabs[t].color});
          for (const auto &slot : tabs[t].items) {
            ItemHandle h = RestoreDtoToHandle(slot.item, storage->getStoreMutable());
            storage->setSlotHandle(
                SlotRef{ContainerKind::PersonalStash, 0, static_cast<uint16_t>(t),
                        static_cast<uint16_t>(slot.slotIndex)},
                h);
          }
        }
        storage->setPersonalStashMeta(std::move(metas));
      }
    }

    // 兼容性填充 ECS 容器组件 (用于旧版测试)
    auto &inv = registry.emplace<InventoryComponent>(player);
    inv.gold = snapshotData.gold;
    inv.capacity = (snapshotData.inventoryCapacity > 0) ? snapshotData.inventoryCapacity
                                                        : InventoryComponent::BASE_CAPACITY;
    inv.items.assign(inv.capacity, entt::null);
    for (const auto &entry : snapshotData.inventory) {
      auto itemEntity = ItemFactory::restoreItem(registry, entry.item);
      if (entry.slotIndex >= 0 && static_cast<size_t>(entry.slotIndex) < inv.items.size()) {
        inv.items[entry.slotIndex] = itemEntity;
      } else {
        inv.items.push_back(itemEntity);
      }
    }

    inv.bag_slots.fill(entt::null);
    for (const auto &entry : snapshotData.bagSlots) {
      if (entry.index < inv.bag_slots.size()) {
        inv.bag_slots[entry.index] = ItemFactory::restoreItem(registry, entry.bag);
      }
    }

    auto &bank = registry.emplace<MaterialBankComponent>(player);
    for (const auto &entry : snapshotData.materialBank) {
      bank.Add(entry.id, entry.count);
    }

    auto &eq = registry.emplace<EquipmentComponent>(player);
    for (const auto &itemDto : snapshotData.equipment) {
      auto itemEntity = ItemFactory::restoreItem(registry, itemDto);
      eq.set(itemDto.stats.slot, itemEntity);
    }

    if (snapshotData.personalStash.has_value()) {
      auto &stash = registry.emplace<PersonalStashComponent>(player);
      stash.unlockedTabs = snapshotData.personalStash->unlockedTabs;
      stash.tabs.resize(stash.unlockedTabs);

      const auto &sTabs = snapshotData.personalStash->tabs;
      for (size_t i = 0; i < sTabs.size(); ++i) {
        if (i >= stash.tabs.size()) break;
        auto &t = stash.tabs[i];
        const auto &sT = sTabs[i];

        t.name = sT.name;
        t.type = sT.type;
        t.iconId = sT.iconId;
        t.color = sT.color;

        for (const auto &slot : sT.items) {
          if (slot.slotIndex >= 0 && slot.slotIndex < StashTab::CAPACITY) {
            t.items[slot.slotIndex] = ItemFactory::restoreItem(registry, slot.item);
          }
        }
      }
    } else {
      registry.emplace<PersonalStashComponent>(player);
    }

  } else {
    // 单轨模式：物品数据已由 ItemPersistenceCodec 直接解码灌入 ItemStorageService
    // 玩家实体挂载轻量 ECS 容器组件镜像标量，无需创建任何重复的 ECS Item 实体
    auto &inv = registry.emplace<InventoryComponent>(player);
    if (storage) {
      inv.gold = storage->getGold();
      inv.capacity = static_cast<int32_t>(storage->getInventorySlots().size());
    } else {
      inv.gold = snapshotData.gold;
      inv.capacity = snapshotData.inventoryCapacity;
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
  }

  // Skills & Astrolabe
  ActiveSkillsComponent restoredSkills = snapshotData.skills;
  MigrateLegacySpecializedSlots(originalVersion, restoredSkills);
  registry.emplace<ActiveSkillsComponent>(player, restoredSkills);

  auto &runtime = registry.emplace<SkillContractRuntimeComponent>(player);
  runtime.version = snapshotData.skill_contract_runtime.version;
  runtime.active_transmuter_node_by_skill.clear();
  runtime.trigger_cooldowns.clear();
  for (const auto &entry : snapshotData.skill_contract_runtime.skills) {
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
  registry.emplace<AstrolabeComponent>(player, snapshotData.astrolabe);
  registry.emplace<PlayerCombatHistory>(player, snapshotData.combatHistory);
  if (snapshotData.blade_mastery.has_value()) {
    registry.emplace<BladeMasteryComponent>(player, snapshotData.blade_mastery.value());
  }
  if (snapshotData.blade_resource.has_value()) {
    auto resource = snapshotData.blade_resource.value();
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
  if (snapshotData.blade_signature_skill.has_value()) {
    registry.emplace<BladeSignatureSkillComponent>(
        player, snapshotData.blade_signature_skill.value());
  }
  systems::BladeMasteryService::RefreshPlayerState(registry, player);

  LOG_INFO("SaveManager: Character restored from snapshot (restoreItems={}).", restoreItems);
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

  // 1. 主线程快速构建非物品 Progression 数据与快照
  CharacterSaveData charData = createSnapshot(registry);
  nlohmann::json progJson = BuildProgressionJson(charData);
  std::string progressionPayload = progJson.dump();

  ItemStorageService *storage = GetItemStorageService(&registry);
  ItemStorageService storageSnapshot;
  if (storage) {
    storageSnapshot = *storage;
  } else {
    // 兼容回退：从 charData 导入临时 storageSnapshot
    storageSnapshot.setGold(charData.gold);
    for (const auto &mat : charData.materialBank) {
      storageSnapshot.addMaterial(mat.id, mat.count);
    }
    for (const auto &entry : charData.inventory) {
      ItemHandle h = RestoreDtoToHandle(entry.item, storageSnapshot.getStoreMutable());
      storageSnapshot.setSlotHandle(
          SlotRef{ContainerKind::Inventory, 0, 0, static_cast<uint16_t>(entry.slotIndex)}, h);
    }
    for (const auto &eq : charData.equipment) {
      ItemHandle h = RestoreDtoToHandle(eq, storageSnapshot.getStoreMutable());
      storageSnapshot.setSlotHandle(
          SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(eq.stats.slot), 0, 0}, h);
    }
    for (const auto &b : charData.bagSlots) {
      ItemHandle h = RestoreDtoToHandle(b.bag, storageSnapshot.getStoreMutable());
      storageSnapshot.setSlotHandle(SlotRef{ContainerKind::BagSlots, b.index, 0, 0}, h);
    }
    if (charData.personalStash.has_value()) {
      storageSnapshot.setUnlockedPages(ContainerKind::PersonalStash, charData.personalStash->unlockedTabs);
      std::vector<StashTabMeta> metas;
      for (size_t t = 0; t < charData.personalStash->tabs.size(); ++t) {
        const auto &tab = charData.personalStash->tabs[t];
        metas.push_back({tab.name, static_cast<uint8_t>(tab.type), tab.iconId, tab.color});
        for (const auto &slot : tab.items) {
          ItemHandle h = RestoreDtoToHandle(slot.item, storageSnapshot.getStoreMutable());
          storageSnapshot.setSlotHandle(
              SlotRef{ContainerKind::PersonalStash, 0, static_cast<uint16_t>(t),
                      static_cast<uint16_t>(slot.slotIndex)},
              h);
        }
      }
      storageSnapshot.setPersonalStashMeta(std::move(metas));
    }
  }

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
  std::string binaryPath = getSavePath(slotIndex);
  std::string backupPath = getBackupPath(slotIndex);
  std::string jsonPath = getJsonSavePath(slotIndex);

  ItemStorageService *storage = GetItemStorageService(&registry);

  // 1. 优先读取主档 saves/slot_N.nmd
  if (fs::exists(binaryPath)) {
    std::ifstream file(binaryPath, std::ios::binary);
    std::string progressionPayload;
    ItemStorageService loadedStorage;
    if (file.is_open() && ItemPersistenceCodec::decode(file, loadedStorage, &progressionPayload)) {
      if (storage) {
        *storage = std::move(loadedStorage);
      }
      try {
        nlohmann::json progJson = nlohmann::json::parse(progressionPayload);
        CharacterSaveData charData = ParseProgressionJson(progJson);
        restoreFromSnapshot(registry, charData, /*restoreItems=*/false);
        LOG_INFO("SaveManager: Successfully loaded character from binary {}", binaryPath);
        return true;
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
      if (storage) {
        *storage = std::move(loadedStorage);
      }
      try {
        nlohmann::json progJson = nlohmann::json::parse(progressionPayload);
        CharacterSaveData charData = ParseProgressionJson(progJson);
        restoreFromSnapshot(registry, charData, /*restoreItems=*/false);
        LOG_INFO("SaveManager: Successfully recovered character from backup {}", backupPath);
        return true;
      } catch (const std::exception &e) {
        LOG_ERROR("SaveManager: Failed to parse progression from backup {}: {}", backupPath, e.what());
      }
    }
  }

  // 2. 若 .nmd 不存在，检查旧版 saves/slot_N.json (v3/v4 格式)，反序列化并自动升级
  if (fs::exists(jsonPath)) {
    try {
      std::ifstream file(jsonPath);
      if (file.is_open()) {
        nlohmann::json j;
        file >> j;
        CharacterSaveData data = j.get<CharacterSaveData>();
        restoreFromSnapshot(registry, data, /*restoreItems=*/true);
        LOG_INFO("SaveManager: Loaded JSON save {}, will upgrade to .nmd on next save.", jsonPath);
        return true;
      }
    } catch (const std::exception &e) {
      LOG_ERROR("SaveManager: Failed to load JSON save {}: {}", jsonPath, e.what());
    }
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

std::string SaveManager::getJsonSavePath(int slotIndex) const {
  if (slotIndex == -1) {
    return "saves/quicksave.json";
  }
  return "saves/slot_" + std::to_string(slotIndex) + ".json";
}

bool SaveManager::loadGlobal(entt::registry &registry) {
  std::string binaryPath = "saves/global.nmd";
  std::string backupPath = "saves/global.nmd.bak";
  std::string jsonPath = "saves/global.json";

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

  // 2. 兼容回退旧版 saves/global.json
  if (fs::exists(jsonPath)) {
    try {
      std::ifstream file(jsonPath);
      if (file.is_open()) {
        nlohmann::json j;
        file >> j;
        GlobalSaveData data = j.get<GlobalSaveData>();
        nlohmann::json jStash = data.sharedStash;
        SharedStash::Get().fromJson(jStash, registry);

        if (storage) {
          storage->setUnlockedPages(ContainerKind::SharedStash, data.sharedStash.unlockedTabs);
          std::vector<StashTabMeta> metas;
          for (size_t t = 0; t < data.sharedStash.tabs.size(); ++t) {
            const auto &tab = data.sharedStash.tabs[t];
            metas.push_back({tab.name, static_cast<uint8_t>(tab.type), tab.iconId, tab.color});
            for (const auto &slot : tab.items) {
              ItemHandle h = RestoreDtoToHandle(slot.item, storage->getStoreMutable());
              storage->setSlotHandle(
                  SlotRef{ContainerKind::SharedStash, 0, static_cast<uint16_t>(t),
                          static_cast<uint16_t>(slot.slotIndex)},
                  h);
            }
          }
          storage->setSharedStashMeta(std::move(metas));
        }
        LOG_INFO("SaveManager: Global save loaded from JSON {}", jsonPath);
        return true;
      }
    } catch (const std::exception &e) {
      LOG_ERROR("SaveManager: Failed to load global JSON save: {}", e.what());
    }
  }

  SharedStash::Get().initialize();
  return true;
}

std::future<bool> SaveManager::saveGlobalAsync(entt::registry &registry) {
  if (!m_executor) {
    std::promise<bool> p;
    p.set_value(false);
    return p.get_future();
  }

  ItemStorageService *storage = GetItemStorageService(&registry);
  ItemStorageService snapshot;
  if (storage) {
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
  } else {
    nlohmann::json jStash = SharedStash::Get().toJson(registry);
    SerializedStash sStash = jStash.get<SerializedStash>();
    snapshot.setUnlockedPages(ContainerKind::SharedStash, sStash.unlockedTabs);
    std::vector<StashTabMeta> metas;
    for (size_t t = 0; t < sStash.tabs.size(); ++t) {
      const auto &tab = sStash.tabs[t];
      metas.push_back({tab.name, static_cast<uint8_t>(tab.type), tab.iconId, tab.color});
      for (const auto &slot : tab.items) {
        ItemHandle h = RestoreDtoToHandle(slot.item, snapshot.getStoreMutable());
        snapshot.setSlotHandle(
            SlotRef{ContainerKind::SharedStash, 0, static_cast<uint16_t>(t),
                    static_cast<uint16_t>(slot.slotIndex)},
            h);
      }
    }
    snapshot.setSharedStashMeta(std::move(metas));
  }

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
