#include "game/systems/item/storage/ItemStorageConverter.hpp"
#include "core/utils/HashUtils.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include <algorithm>

namespace NoMoreDay {

bool ItemComponentToInstance(const ItemComponent &comp, ItemInstance &outInst,
                             ItemSideTableData *outSide) {
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

  if (outSide != nullptr) {
    outSide->conversions = comp.conversions;
    outSide->damage_modifiers = comp.damage_modifiers;
  }

  return true;
}

ItemInstance ItemComponentToInstance(const ItemComponent &comp,
                                     ItemSideTableData *outSide) {
  ItemInstance inst;
  ItemComponentToInstance(comp, inst, outSide);
  return inst;
}

bool InstanceToItemComponent(const ItemInstance &inst, ItemComponent &outComp,
                             const ItemSideTableData *side) {
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

  outComp.sockets.assign(inst.socketCount, entt::null);

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

ItemComponent InstanceToItemComponent(const ItemInstance &inst,
                                      const ItemSideTableData *side) {
  ItemComponent comp;
  InstanceToItemComponent(inst, comp, side);
  return comp;
}

} // namespace NoMoreDay
