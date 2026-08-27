#include "game/systems/item/storage/ItemStorageConverter.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/HashUtils.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <algorithm>

namespace NoMoreDay {

bool ItemComponentToInstance(
    const ItemComponent &comp, ItemInstance &outInst, ItemSideTableData *outSide,
    const std::unordered_map<entt::entity, ItemHandle> *socketMap) {
  outInst = ItemInstance{};
  outInst.instanceId = static_cast<uint64_t>(comp.id);
  outInst.baseId = comp.baseId;
  outInst.quantity = static_cast<uint32_t>(std::max(0, comp.quantity));
  outInst.itemLevel = static_cast<uint16_t>(std::max(0, comp.itemLevel));
  outInst.rarity = static_cast<uint8_t>(comp.rarity);
  outInst.setLocked(comp.isLocked);
  outInst.setTwoHanded(comp.isTwoHanded);
  outInst.forgingPotential = static_cast<int16_t>(comp.forgingPotential);
  outInst.legendaryPotential =
      static_cast<uint8_t>(std::clamp(comp.legendaryPotential, 0, 255));
  outInst.socketCount =
      static_cast<uint8_t>(std::clamp(comp.socketCount, 0, 6));
  outInst.attack = comp.attack;
  outInst.defense = comp.defense;
  outInst.value = comp.value;
  outInst.activeRunewordId = comp.activeRunewordId;

  const size_t affixCount =
      std::min<size_t>(comp.affixes.size(), outInst.affixes.size());
  outInst.affixCount = static_cast<uint8_t>(affixCount);
  for (size_t i = 0; i < affixCount; ++i) {
    outInst.affixes[i].type = comp.affixes[i].type;
    outInst.affixes[i].tier =
        static_cast<uint8_t>(std::clamp(comp.affixes[i].tier, 0, 255));
    outInst.affixes[i].isPrefix = comp.affixes[i].isPrefix;
    outInst.affixes[i].value = comp.affixes[i].value;
  }
  for (size_t i = affixCount; i < outInst.affixes.size(); ++i) {
    outInst.affixes[i] = CompactAffix{};
  }

  outInst.sockets.fill(ItemHandle{0, 0});
  if (outInst.socketCount > 0) {
    const size_t srcCount =
        std::min<size_t>(outInst.socketCount, comp.sockets.size());
    bool hasLiveEntities = false;
    for (size_t i = 0; i < srcCount; ++i) {
      if (comp.sockets[i] == entt::null) {
        continue;
      }
      hasLiveEntities = true;
      if (socketMap != nullptr) {
        const auto it = socketMap->find(comp.sockets[i]);
        if (it != socketMap->end()) {
          outInst.sockets[i] = it->second;
        }
      }
    }
    if (hasLiveEntities && socketMap == nullptr) {
      LOG_WARN(
          "ItemComponentToInstance: instance {} carries {} sockets with live "
          "entities but no socketMap was provided; pool handles stay null and "
          "the caller must backfill them",
          comp.id, srcCount);
    }
  }

  if (outSide != nullptr) {
    outSide->conversions = comp.conversions;
    outSide->damage_modifiers = comp.damage_modifiers;
  }

  return true;
}

ItemInstance ItemComponentToInstance(
    const ItemComponent &comp, ItemSideTableData *outSide,
    const std::unordered_map<entt::entity, ItemHandle> *socketMap) {
  ItemInstance inst;
  ItemComponentToInstance(comp, inst, outSide, socketMap);
  return inst;
}

bool InstanceToItemComponent(
    const ItemInstance &inst, ItemComponent &outComp, const ItemSideTableData *side,
    const std::unordered_map<ItemHandle, entt::entity> *socketEntityMap) {
  outComp = ItemComponent{};
  outComp.id = static_cast<uint32_t>(inst.instanceId);
  outComp.baseId = inst.baseId;
  outComp.quantity = static_cast<int>(inst.quantity);
  outComp.itemLevel = static_cast<int>(inst.itemLevel);
  outComp.rarity = static_cast<Rarity>(inst.rarity);
  outComp.isLocked = inst.isLocked();
  outComp.isTwoHanded = inst.isTwoHanded();
  outComp.forgingPotential = inst.forgingPotential;
  outComp.legendaryPotential = inst.legendaryPotential;
  outComp.socketCount = inst.socketCount;
  outComp.attack = inst.attack;
  outComp.defense = inst.defense;
  outComp.value = inst.value;
  outComp.activeRunewordId = inst.activeRunewordId;

  outComp.sockets.assign(
      std::min<size_t>(inst.socketCount, inst.sockets.size()), entt::null);
  if (!outComp.sockets.empty()) {
    bool hasUnresolvedHandles = false;
    for (size_t i = 0; i < outComp.sockets.size(); ++i) {
      if (socketEntityMap != nullptr) {
        const auto it = socketEntityMap->find(inst.sockets[i]);
        if (it != socketEntityMap->end()) {
          outComp.sockets[i] = it->second;
          continue;
        }
      }
      // 插槽保持 entt::null 直到所属迁移逻辑完成回填。
      if (!(inst.sockets[i].index == 0 && inst.sockets[i].gen == 0)) {
        hasUnresolvedHandles = true;
      }
    }
    if (hasUnresolvedHandles) {
      LOG_WARN(
          "InstanceToItemComponent: instance {} carries {} socket slots with "
          "live pool handles but{} entity mapping; scene sockets stay null "
          "until backfilled",
          inst.instanceId, outComp.sockets.size(),
          socketEntityMap != nullptr ? " an incomplete" : " no");
    }
  }

  const size_t count =
      std::min<size_t>(inst.affixCount, inst.affixes.size());
  outComp.affixes.clear();
  outComp.affixes.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    Affix aff;
    aff.type = inst.affixes[i].type;
    aff.tier = inst.affixes[i].tier;
    aff.isPrefix = inst.affixes[i].isPrefix;
    aff.value = inst.affixes[i].value;
    outComp.affixes.push_back(aff);
  }

  if (side != nullptr) {
    outComp.conversions = side->conversions;
    outComp.damage_modifiers = side->damage_modifiers;
  } else {
    outComp.conversions.clear();
    outComp.damage_modifiers.clear();
  }

  const ItemTemplate *tmpl =
      ItemTemplateRegistry::Instance().find(inst.baseId);
  if (tmpl != nullptr) {
    outComp.name = tmpl->name;
    outComp.description = tmpl->description;
    outComp.type = tmpl->type;
    outComp.slot = tmpl->slot;
    outComp.weaponSubtype = tmpl->weaponSubtype;
    outComp.catalystKind = tmpl->catalystKind;
    outComp.maxStack = tmpl->maxStack;
    outComp.bagCapacity = tmpl->bagCapacity;
    outComp.setName = tmpl->setName;
    outComp.setNameHash = tmpl->setNameHash;
    outComp.setBonuses = tmpl->setBonuses;
    outComp.textureId = tmpl->textureId;
  } else {
    outComp.maxStack = 1;
  }

  if (outComp.setNameHash == 0 && !outComp.setName.empty()) {
    outComp.setNameHash = NoMoreDay::utils::Hash(outComp.setName);
  }

  return true;
}

ItemComponent InstanceToItemComponent(
    const ItemInstance &inst, const ItemSideTableData *side,
    const std::unordered_map<ItemHandle, entt::entity> *socketEntityMap) {
  ItemComponent comp;
  InstanceToItemComponent(inst, comp, side, socketEntityMap);
  return comp;
}

} // namespace NoMoreDay
