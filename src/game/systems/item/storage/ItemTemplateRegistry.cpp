#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/HashUtils.hpp"
#include <algorithm>

namespace NoMoreDay {

ItemTemplateRegistry &ItemTemplateRegistry::Instance() noexcept {
  static ItemTemplateRegistry instance;
  return instance;
}

void ItemTemplateRegistry::registerTemplate(ItemTemplate t) {
  if (t.baseId == 0) {
    LOG_WARN("ItemTemplateRegistry: attempted to register template with baseId 0 ({})", t.name);
    return;
  }

  // Calculate setNameHash if setName is provided but hash is 0
  if (!t.setName.empty() && t.setNameHash == 0) {
    t.setNameHash = NoMoreDay::utils::Hash(t.setName);
  }

  // Derive high-level kind if not specified
  if (t.kind == ItemKind::None) {
    switch (t.type) {
    case ItemType::Weapon:
      t.kind = ItemKind::Weapon;
      break;
    case ItemType::Armor:
      t.kind = ItemKind::Armor;
      break;
    case ItemType::Shield:
      t.kind = ItemKind::Shield;
      break;
    case ItemType::Jewelry:
      t.kind = ItemKind::Jewelry;
      break;
    case ItemType::Consumable:
      t.kind = ItemKind::Consumable;
      break;
    case ItemType::Material:
      t.kind = ItemKind::Material;
      break;
    case ItemType::Quest:
      t.kind = ItemKind::Quest;
      break;
    case ItemType::Bag:
      t.kind = ItemKind::Bag;
      break;
    }
  }

  const uint32_t baseId = t.baseId;
  const std::string name = t.name;
  m_templates[baseId] = std::move(t);
  if (!name.empty()) {
    m_nameIndex[name] = baseId;
  }
}

const ItemTemplate *ItemTemplateRegistry::find(uint32_t baseId) const noexcept {
  auto it = m_templates.find(baseId);
  if (it != m_templates.end()) {
    return &it->second;
  }
  return nullptr;
}

const ItemTemplate *
ItemTemplateRegistry::findByName(std::string_view name) const noexcept {
  auto it = m_nameIndex.find(std::string(name));
  if (it != m_nameIndex.end()) {
    return find(it->second);
  }
  return nullptr;
}

void ItemTemplateRegistry::clear() noexcept {
  m_templates.clear();
  m_nameIndex.clear();
}

std::vector<const ItemTemplate *>
ItemTemplateRegistry::getTemplatesByWeaponSubtype(
    WeaponSubtype subtype) const {
  std::vector<const ItemTemplate *> results;
  for (const auto &[id, tmpl] : m_templates) {
    if (tmpl.type == ItemType::Weapon && tmpl.weaponSubtype == subtype) {
      results.push_back(&tmpl);
    }
  }
  std::sort(results.begin(), results.end(),
            [](const ItemTemplate *a, const ItemTemplate *b) {
              return a->minLevel < b->minLevel;
            });
  return results;
}

std::vector<const ItemTemplate *>
ItemTemplateRegistry::getTemplatesBySlot(EquipmentSlot slot) const {
  std::vector<const ItemTemplate *> results;
  for (const auto &[id, tmpl] : m_templates) {
    if (tmpl.slot == slot) {
      results.push_back(&tmpl);
    }
  }
  std::sort(results.begin(), results.end(),
            [](const ItemTemplate *a, const ItemTemplate *b) {
              return a->minLevel < b->minLevel;
            });
  return results;
}

std::vector<const ItemTemplate *>
ItemTemplateRegistry::getTemplatesByKind(ItemKind kind) const {
  std::vector<const ItemTemplate *> results;
  for (const auto &[id, tmpl] : m_templates) {
    if (tmpl.kind == kind) {
      results.push_back(&tmpl);
    }
  }
  std::sort(results.begin(), results.end(),
            [](const ItemTemplate *a, const ItemTemplate *b) {
              return a->minLevel < b->minLevel;
            });
  return results;
}

std::vector<const ItemTemplate *>
ItemTemplateRegistry::getTemplatesByType(ItemType type) const {
  std::vector<const ItemTemplate *> results;
  for (const auto &[id, tmpl] : m_templates) {
    if (tmpl.type == type) {
      results.push_back(&tmpl);
    }
  }
  std::sort(results.begin(), results.end(),
            [](const ItemTemplate *a, const ItemTemplate *b) {
              return a->minLevel < b->minLevel;
            });
  return results;
}

void ItemTemplateRegistry::initializeDefaults() {
  clear();

  // ---------------------------------------------------------------------------
  // 1. Weapon Templates (1001 - 1066)
  // ---------------------------------------------------------------------------

  // Swords (1001 - 1006)
  registerTemplate({.baseId = 1001, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Sword, .isTwoHanded = false, .name = "锈蚀铁剑", .description = "一把布满铁锈的旧剑，勉强还能劈砍。", .minLevel = 1, .maxStack = 1, .baseStatMin = 5.0f, .baseStatMax = 8.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1002, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Sword, .isTwoHanded = false, .name = "精铁长剑", .description = "精工打造的长剑，重心平稳。", .minLevel = 10, .maxStack = 1, .baseStatMin = 12.0f, .baseStatMax = 18.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1003, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Sword, .isTwoHanded = false, .name = "骑士阔剑", .description = "骑士团制式阔剑，剑身厚重锋利。", .minLevel = 25, .maxStack = 1, .baseStatMin = 25.0f, .baseStatMax = 35.0f, .implicitType = AffixType::CritChance});
  registerTemplate({.baseId = 1004, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Sword, .isTwoHanded = false, .name = "秘银长剑", .description = "秘银轻盈而坚韧，挥砍迅捷如风。", .minLevel = 45, .maxStack = 1, .baseStatMin = 45.0f, .baseStatMax = 60.0f, .implicitType = AffixType::AttackSpeed});
  registerTemplate({.baseId = 1005, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Sword, .isTwoHanded = false, .name = "符文剑", .description = "刻满烈焰符文的长剑，刃口隐隐发红。", .minLevel = 60, .maxStack = 1, .baseStatMin = 70.0f, .baseStatMax = 90.0f, .implicitType = AffixType::PercentFireDamage});
  registerTemplate({.baseId = 1006, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Sword, .isTwoHanded = false, .name = "龙牙剑", .description = "取上古巨龙之牙铸造的神兵，能撕裂一切护甲。", .minLevel = 75, .maxStack = 1, .baseStatMin = 100.0f, .baseStatMax = 130.0f, .implicitType = AffixType::CritDamage});

  // Axes (1011 - 1016)
  registerTemplate({.baseId = 1011, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Axe, .isTwoHanded = false, .name = "伐木斧", .description = "普通伐木斧，也能当防身武器。", .minLevel = 1, .maxStack = 1, .baseStatMin = 6.0f, .baseStatMax = 10.0f, .implicitType = AffixType::FlatPhysicalDamage});
  registerTemplate({.baseId = 1012, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Axe, .isTwoHanded = false, .name = "铁手斧", .description = "便于挥舞的精铁手斧。", .minLevel = 10, .maxStack = 1, .baseStatMin = 14.0f, .baseStatMax = 20.0f, .implicitType = AffixType::FlatPhysicalDamage});
  registerTemplate({.baseId = 1013, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Axe, .isTwoHanded = false, .name = "战斗斧", .description = "专为战场劈砍设计的锋利双面战斧。", .minLevel = 25, .maxStack = 1, .baseStatMin = 28.0f, .baseStatMax = 38.0f, .implicitType = AffixType::CritDamage});
  registerTemplate({.baseId = 1014, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Axe, .isTwoHanded = false, .name = "狂战士斧", .description = "浸染狂暴战意的凶猛战斧。", .minLevel = 45, .maxStack = 1, .baseStatMin = 50.0f, .baseStatMax = 65.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1015, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Axe, .isTwoHanded = false, .name = "斩首斧", .description = "沉重阴森的行刑斧，饮血自愈。", .minLevel = 60, .maxStack = 1, .baseStatMin = 75.0f, .baseStatMax = 95.0f, .implicitType = AffixType::LifeSteal});
  registerTemplate({.baseId = 1016, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Axe, .isTwoHanded = false, .name = "毁灭者", .description = "具有毁灭性威能的巨斧，重击毁天灭地。", .minLevel = 75, .maxStack = 1, .baseStatMin = 110.0f, .baseStatMax = 140.0f, .implicitType = AffixType::CritDamage});

  // Daggers (1021 - 1026)
  registerTemplate({.baseId = 1021, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Dagger, .isTwoHanded = false, .name = "磨损匕首", .description = "刀刃略有磨损的小巧短刀。", .minLevel = 1, .maxStack = 1, .baseStatMin = 3.0f, .baseStatMax = 6.0f, .implicitType = AffixType::CritChance});
  registerTemplate({.baseId = 1022, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Dagger, .isTwoHanded = false, .name = "猎人短刀", .description = "猎人用于剥皮和刺杀的敏捷短刃。", .minLevel = 10, .maxStack = 1, .baseStatMin = 8.0f, .baseStatMax = 14.0f, .implicitType = AffixType::CritChance});
  registerTemplate({.baseId = 1023, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Dagger, .isTwoHanded = false, .name = "刺客匕首", .description = "淬有暗影的刺杀利刃，专攻要害。", .minLevel = 25, .maxStack = 1, .baseStatMin = 18.0f, .baseStatMax = 26.0f, .implicitType = AffixType::CritDamage});
  registerTemplate({.baseId = 1024, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Dagger, .isTwoHanded = false, .name = "锯齿刃", .description = "带有剧毒锯齿的凶残短刃。", .minLevel = 45, .maxStack = 1, .baseStatMin = 35.0f, .baseStatMax = 48.0f, .implicitType = AffixType::FlatPoisonDamage});
  registerTemplate({.baseId = 1025, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Dagger, .isTwoHanded = false, .name = "幽冥匕首", .description = "徘徊在阴阳边缘的幽冥毒刃。", .minLevel = 60, .maxStack = 1, .baseStatMin = 55.0f, .baseStatMax = 75.0f, .implicitType = AffixType::PercentPoisonDamage});
  registerTemplate({.baseId = 1026, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Dagger, .isTwoHanded = false, .name = "龙骨匕首", .description = "龙骨雕琢的绝命短刃，极具爆发力。", .minLevel = 75, .maxStack = 1, .baseStatMin = 80.0f, .baseStatMax = 100.0f, .implicitType = AffixType::CritChance});

  // Hammers / Maces (1031 - 1036)
  registerTemplate({.baseId = 1031, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Mace, .isTwoHanded = false, .name = "木锤", .description = "结实的硬木锤。", .minLevel = 1, .maxStack = 1, .baseStatMin = 7.0f, .baseStatMax = 11.0f, .implicitType = AffixType::FlatPhysicalDamage});
  registerTemplate({.baseId = 1032, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Mace, .isTwoHanded = false, .name = "铁战锤", .description = "沉重的铁铸战锤，能砸碎铠甲。", .minLevel = 10, .maxStack = 1, .baseStatMin = 16.0f, .baseStatMax = 24.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1033, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Mace, .isTwoHanded = false, .name = "碎骨锤", .description = "专为粉碎骨骼设计的重型钝器。", .minLevel = 25, .maxStack = 1, .baseStatMin = 32.0f, .baseStatMax = 45.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1034, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Mace, .isTwoHanded = false, .name = "重型战锤", .description = "附带雷电轰鸣的重型战锤。", .minLevel = 45, .maxStack = 1, .baseStatMin = 55.0f, .baseStatMax = 75.0f, .implicitType = AffixType::FlatLightningDamage});
  registerTemplate({.baseId = 1035, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Mace, .isTwoHanded = false, .name = "雷神之锤", .description = "引动九天落雷的雷霆战锤。", .minLevel = 60, .maxStack = 1, .baseStatMin = 85.0f, .baseStatMax = 110.0f, .implicitType = AffixType::PercentLightningDamage});
  registerTemplate({.baseId = 1036, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Mace, .isTwoHanded = false, .name = "泰坦之锤", .description = "巨人泰坦使用的撼地巨锤。", .minLevel = 75, .maxStack = 1, .baseStatMin = 120.0f, .baseStatMax = 160.0f, .implicitType = AffixType::PercentPhysicalDamage});

  // Greatswords (1041 - 1046, Two-Handed)
  registerTemplate({.baseId = 1041, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Greatsword, .isTwoHanded = true, .name = "训练大剑", .description = "新兵训练用的大剑，需双手持握。", .minLevel = 1, .maxStack = 1, .baseStatMin = 8.0f, .baseStatMax = 12.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1042, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Greatsword, .isTwoHanded = true, .name = "铁大剑", .description = "质朴沉重的铁制大剑。", .minLevel = 10, .maxStack = 1, .baseStatMin = 18.0f, .baseStatMax = 26.0f, .implicitType = AffixType::PercentPhysicalDamage});
  registerTemplate({.baseId = 1043, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Greatsword, .isTwoHanded = true, .name = "巨剑", .description = "巨大的双手双手大剑，势不可挡。", .minLevel = 25, .maxStack = 1, .baseStatMin = 35.0f, .baseStatMax = 50.0f, .implicitType = AffixType::FlatPhysicalDamage});
  registerTemplate({.baseId = 1044, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Greatsword, .isTwoHanded = true, .name = "斩马刀", .description = "骑兵克星，一击可断长枪巨盾。", .minLevel = 45, .maxStack = 1, .baseStatMin = 60.0f, .baseStatMax = 80.0f, .implicitType = AffixType::CritDamage});
  registerTemplate({.baseId = 1045, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Greatsword, .isTwoHanded = true, .name = "处刑者", .description = "斩杀无数强敌的大剑，汲取生命。", .minLevel = 60, .maxStack = 1, .baseStatMin = 90.0f, .baseStatMax = 120.0f, .implicitType = AffixType::LifeOnHit});
  registerTemplate({.baseId = 1046, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Greatsword, .isTwoHanded = true, .name = "诸神黄昏", .description = "见证诸神陨落的终极灭世巨剑。", .minLevel = 75, .maxStack = 1, .baseStatMin = 130.0f, .baseStatMax = 170.0f, .implicitType = AffixType::PercentPhysicalDamage});

  // Staves (1051 - 1056, Two-Handed)
  registerTemplate({.baseId = 1051, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Staff, .isTwoHanded = true, .name = "枯木法杖", .description = "粗糙的枯木杖，能微弱凝聚法力。", .minLevel = 1, .maxStack = 1, .baseStatMin = 4.0f, .baseStatMax = 8.0f, .implicitType = AffixType::FlatMana});
  registerTemplate({.baseId = 1052, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Staff, .isTwoHanded = true, .name = "橡木法杖", .description = "坚硬橡木制成的法杖，蕴含火元素。", .minLevel = 10, .maxStack = 1, .baseStatMin = 10.0f, .baseStatMax = 16.0f, .implicitType = AffixType::PercentFireDamage});
  registerTemplate({.baseId = 1053, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Staff, .isTwoHanded = true, .name = "宝石法杖", .description = "镶嵌冰霜宝石的法杖，寒气逼人。", .minLevel = 25, .maxStack = 1, .baseStatMin = 22.0f, .baseStatMax = 32.0f, .implicitType = AffixType::PercentColdDamage});
  registerTemplate({.baseId = 1054, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Staff, .isTwoHanded = true, .name = "元素法杖", .description = "调和全系元素的均衡长杖。", .minLevel = 45, .maxStack = 1, .baseStatMin = 40.0f, .baseStatMax = 55.0f, .implicitType = AffixType::ResistAll});
  registerTemplate({.baseId = 1055, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Staff, .isTwoHanded = true, .name = "贤者法杖", .description = "古老贤者的传承法杖，雷霆万钧。", .minLevel = 60, .maxStack = 1, .baseStatMin = 65.0f, .baseStatMax = 85.0f, .implicitType = AffixType::PercentLightningDamage});
  registerTemplate({.baseId = 1056, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Staff, .isTwoHanded = true, .name = "世界树枝", .description = "截取自世界之树的圣枝，蕴藏无穷智慧。", .minLevel = 75, .maxStack = 1, .baseStatMin = 95.0f, .baseStatMax = 125.0f, .implicitType = AffixType::Intelligence});

  // Wands (1061 - 1066)
  registerTemplate({.baseId = 1061, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Wand, .isTwoHanded = false, .name = "学徒魔杖", .description = "魔法学徒练习用的轻巧魔杖。", .minLevel = 1, .maxStack = 1, .baseStatMin = 3.0f, .baseStatMax = 7.0f, .implicitType = AffixType::FlatMana});
  registerTemplate({.baseId = 1062, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Wand, .isTwoHanded = false, .name = "骨魔杖", .description = "白骨打磨的暗影魔杖。", .minLevel = 10, .maxStack = 1, .baseStatMin = 9.0f, .baseStatMax = 15.0f, .implicitType = AffixType::FlatShadowDamage});
  registerTemplate({.baseId = 1063, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Wand, .isTwoHanded = false, .name = "水晶魔杖", .description = "通体晶莹的水晶魔杖，聚集暗影法能。", .minLevel = 25, .maxStack = 1, .baseStatMin = 20.0f, .baseStatMax = 30.0f, .implicitType = AffixType::PercentShadowDamage});
  registerTemplate({.baseId = 1064, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Wand, .isTwoHanded = false, .name = "秘法魔杖", .description = "大幅缩短施法时间的秘法手杖。", .minLevel = 45, .maxStack = 1, .baseStatMin = 38.0f, .baseStatMax = 52.0f, .implicitType = AffixType::CastSpeed});
  registerTemplate({.baseId = 1065, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Wand, .isTwoHanded = false, .name = "虚空魔杖", .description = "沟通虚空裂隙的幽暗魔杖。", .minLevel = 60, .maxStack = 1, .baseStatMin = 60.0f, .baseStatMax = 80.0f, .implicitType = AffixType::PercentShadowDamage});
  registerTemplate({.baseId = 1066, .kind = ItemKind::Weapon, .type = ItemType::Weapon, .slot = EquipmentSlot::MainHand, .weaponSubtype = WeaponSubtype::Wand, .isTwoHanded = false, .name = "星辰魔杖", .description = "引动群星辉光的崇高魔杖，提升智力。", .minLevel = 75, .maxStack = 1, .baseStatMin = 90.0f, .baseStatMax = 115.0f, .implicitType = AffixType::Intelligence});

  // ---------------------------------------------------------------------------
  // 2. Armor & Shield Templates (2001 - 2065)
  // ---------------------------------------------------------------------------

  // Head (2001 - 2005)
  registerTemplate({.baseId = 2001, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Head, .name = "皮帽", .description = "柔软的皮质帽子。", .minLevel = 1, .maxStack = 1, .baseStatMin = 2.0f, .baseStatMax = 4.0f, .implicitType = AffixType::FlatMana});
  registerTemplate({.baseId = 2002, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Head, .name = "铁盔", .description = "坚实的铁制头盔。", .minLevel = 10, .maxStack = 1, .baseStatMin = 6.0f, .baseStatMax = 10.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 2003, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Head, .name = "骑士头盔", .description = "全封闭式骑士护面盔。", .minLevel = 25, .maxStack = 1, .baseStatMin = 15.0f, .baseStatMax = 22.0f, .implicitType = AffixType::FlatArmor});
  registerTemplate({.baseId = 2004, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Head, .name = "统帅头盔", .description = "军团统帅佩戴的坚固战盔。", .minLevel = 45, .maxStack = 1, .baseStatMin = 30.0f, .baseStatMax = 40.0f, .implicitType = AffixType::PercentArmor});
  registerTemplate({.baseId = 2005, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Head, .name = "龙盔", .description = "龙骨打造的威严头盔，强化生命体魄。", .minLevel = 60, .maxStack = 1, .baseStatMin = 50.0f, .baseStatMax = 70.0f, .implicitType = AffixType::Vitality});

  // Chest (2011 - 2015)
  registerTemplate({.baseId = 2011, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Chest, .name = "破旧法袍", .description = "一件磨损严重的布质法袍。", .minLevel = 1, .maxStack = 1, .baseStatMin = 3.0f, .baseStatMax = 6.0f, .implicitType = AffixType::FlatMana});
  registerTemplate({.baseId = 2012, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Chest, .name = "硬皮上衣", .description = "熟皮缝制的贴身上衣，提供基础保护。", .minLevel = 10, .maxStack = 1, .baseStatMin = 10.0f, .baseStatMax = 15.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 2013, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Chest, .name = "锁子甲", .description = "铁环相扣的锁子甲，兼具灵活性与防护。", .minLevel = 25, .maxStack = 1, .baseStatMin = 25.0f, .baseStatMax = 35.0f, .implicitType = AffixType::ResistAll});
  registerTemplate({.baseId = 2014, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Chest, .name = "板甲", .description = "整块精钢打造的重型胸甲。", .minLevel = 45, .maxStack = 1, .baseStatMin = 50.0f, .baseStatMax = 65.0f, .implicitType = AffixType::PercentArmor});
  registerTemplate({.baseId = 2015, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Chest, .name = "龙鳞甲", .description = "火龙鳞片制成的绝品重甲，坚不可摧。", .minLevel = 70, .maxStack = 1, .baseStatMin = 80.0f, .baseStatMax = 100.0f, .implicitType = AffixType::FlatHealth});

  // Shoulder (2021 - 2025)
  registerTemplate({.baseId = 2021, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Shoulder, .name = "皮护肩", .description = "简易的皮质护肩。", .minLevel = 1, .maxStack = 1, .baseStatMin = 2.0f, .baseStatMax = 4.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 2022, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Shoulder, .name = "铁护肩", .description = "坚固的铁铸护肩。", .minLevel = 10, .maxStack = 1, .baseStatMin = 5.0f, .baseStatMax = 9.0f, .implicitType = AffixType::Strength});
  registerTemplate({.baseId = 2023, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Shoulder, .name = "钢护肩", .description = "精钢锻造的厚重护肩。", .minLevel = 25, .maxStack = 1, .baseStatMin = 12.0f, .baseStatMax = 18.0f, .implicitType = AffixType::FlatArmor});
  registerTemplate({.baseId = 2024, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Shoulder, .name = "刺客护肩", .description = "轻巧贴合的暗影护肩，提升敏捷。", .minLevel = 45, .maxStack = 1, .baseStatMin = 25.0f, .baseStatMax = 35.0f, .implicitType = AffixType::Dexterity});
  registerTemplate({.baseId = 2025, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Shoulder, .name = "泰坦护肩", .description = "如巍峨山岳般的泰坦巨肩。", .minLevel = 60, .maxStack = 1, .baseStatMin = 45.0f, .baseStatMax = 60.0f, .implicitType = AffixType::PercentArmor});

  // Hands (2031 - 2035)
  registerTemplate({.baseId = 2031, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Hands, .name = "皮手套", .description = "薄皮手套，利于握持武器。", .minLevel = 1, .maxStack = 1, .baseStatMin = 1.0f, .baseStatMax = 3.0f, .implicitType = AffixType::AttackSpeed});
  registerTemplate({.baseId = 2032, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Hands, .name = "铁手套", .description = "铁片包覆的防护手套。", .minLevel = 10, .maxStack = 1, .baseStatMin = 4.0f, .baseStatMax = 7.0f, .implicitType = AffixType::FlatArmor});
  registerTemplate({.baseId = 2033, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Hands, .name = "钢手套", .description = "精钢臂铠，提高致命打击率。", .minLevel = 25, .maxStack = 1, .baseStatMin = 10.0f, .baseStatMax = 15.0f, .implicitType = AffixType::CritChance});
  registerTemplate({.baseId = 2034, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Hands, .name = "符文手套", .description = "铭刻施法符文的丝织手套。", .minLevel = 45, .maxStack = 1, .baseStatMin = 20.0f, .baseStatMax = 30.0f, .implicitType = AffixType::CastSpeed});
  registerTemplate({.baseId = 2035, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Hands, .name = "龙爪手套", .description = "龙爪外形的凶暴铁腕，重创对手。", .minLevel = 60, .maxStack = 1, .baseStatMin = 35.0f, .baseStatMax = 50.0f, .implicitType = AffixType::CritDamage});

  // Legs (2041 - 2045)
  registerTemplate({.baseId = 2041, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Legs, .name = "布裤", .description = "轻便的亚麻布长裤。", .minLevel = 1, .maxStack = 1, .baseStatMin = 2.0f, .baseStatMax = 4.0f, .implicitType = AffixType::MoveSpeed});
  registerTemplate({.baseId = 2042, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Legs, .name = "皮护腿", .description = "加厚皮质护腿。", .minLevel = 10, .maxStack = 1, .baseStatMin = 6.0f, .baseStatMax = 10.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 2043, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Legs, .name = "锁甲护腿", .description = "铁环密织的锁甲护腿。", .minLevel = 25, .maxStack = 1, .baseStatMin = 15.0f, .baseStatMax = 22.0f, .implicitType = AffixType::FlatArmor});
  registerTemplate({.baseId = 2044, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Legs, .name = "板甲护腿", .description = "沉重坚挺的精钢护腿。", .minLevel = 45, .maxStack = 1, .baseStatMin = 30.0f, .baseStatMax = 42.0f, .implicitType = AffixType::PercentArmor});
  registerTemplate({.baseId = 2045, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Legs, .name = "龙鳞护腿", .description = "龙鳞织造的坚毅护甲，增强生机。", .minLevel = 60, .maxStack = 1, .baseStatMin = 55.0f, .baseStatMax = 75.0f, .implicitType = AffixType::Vitality});

  // Feet (2051 - 2055)
  registerTemplate({.baseId = 2051, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Feet, .name = "破旧靴子", .description = "磨损破旧的皮靴。", .minLevel = 1, .maxStack = 1, .baseStatMin = 1.0f, .baseStatMax = 3.0f, .implicitType = AffixType::MoveSpeed});
  registerTemplate({.baseId = 2052, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Feet, .name = "皮靴", .description = "舒适耐磨的旅行皮靴。", .minLevel = 10, .maxStack = 1, .baseStatMin = 4.0f, .baseStatMax = 8.0f, .implicitType = AffixType::MoveSpeed});
  registerTemplate({.baseId = 2053, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Feet, .name = "铁靴", .description = "带铁底铁面的防卫长靴。", .minLevel = 25, .maxStack = 1, .baseStatMin = 10.0f, .baseStatMax = 16.0f, .implicitType = AffixType::FlatArmor});
  registerTemplate({.baseId = 2054, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Feet, .name = "战靴", .description = "军旅精制战靴，步履如磐石。", .minLevel = 45, .maxStack = 1, .baseStatMin = 22.0f, .baseStatMax = 32.0f, .implicitType = AffixType::Strength});
  registerTemplate({.baseId = 2055, .kind = ItemKind::Armor, .type = ItemType::Armor, .slot = EquipmentSlot::Feet, .name = "飞翼靴", .description = "附有御风魔法的疾行神靴。", .minLevel = 60, .maxStack = 1, .baseStatMin = 40.0f, .baseStatMax = 55.0f, .implicitType = AffixType::MoveSpeed});

  // Shields / OffHand (2061 - 2065)
  registerTemplate({.baseId = 2061, .kind = ItemKind::Shield, .type = ItemType::Shield, .slot = EquipmentSlot::OffHand, .name = "圆盾", .description = "木铁结合的简易小圆盾。", .minLevel = 1, .maxStack = 1, .baseStatMin = 5.0f, .baseStatMax = 10.0f, .implicitType = AffixType::FlatArmor});
  registerTemplate({.baseId = 2062, .kind = ItemKind::Shield, .type = ItemType::Shield, .slot = EquipmentSlot::OffHand, .name = "鸢盾", .description = "中世纪经典水滴形鸢盾。", .minLevel = 10, .maxStack = 1, .baseStatMin = 15.0f, .baseStatMax = 25.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 2063, .kind = ItemKind::Shield, .type = ItemType::Shield, .slot = EquipmentSlot::OffHand, .name = "塔盾", .description = "掩蔽全身的巨型重盾，全抗极高。", .minLevel = 25, .maxStack = 1, .baseStatMin = 35.0f, .baseStatMax = 50.0f, .implicitType = AffixType::ResistAll});
  registerTemplate({.baseId = 2064, .kind = ItemKind::Shield, .type = ItemType::Shield, .slot = EquipmentSlot::OffHand, .name = "圣盾", .description = "圣光祝福的坚实壁垒。", .minLevel = 45, .maxStack = 1, .baseStatMin = 60.0f, .baseStatMax = 80.0f, .implicitType = AffixType::PercentArmor});
  registerTemplate({.baseId = 2065, .kind = ItemKind::Shield, .type = ItemType::Shield, .slot = EquipmentSlot::OffHand, .name = "埃癸斯", .description = "神话中的不朽之盾，化解一切冲击。", .minLevel = 60, .maxStack = 1, .baseStatMin = 90.0f, .baseStatMax = 120.0f, .implicitType = AffixType::DamageReduction});

  // ---------------------------------------------------------------------------
  // 3. Jewelry Templates (3001 - 3015)
  // ---------------------------------------------------------------------------

  // Necklaces (3001 - 3005)
  registerTemplate({.baseId = 3001, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Neck, .name = "铜项链", .description = "简单的黄铜吊坠。", .minLevel = 1, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 3002, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Neck, .name = "银项链", .description = "银光闪烁的项链，驱散寒气。", .minLevel = 15, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::ResistCold});
  registerTemplate({.baseId = 3003, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Neck, .name = "金项链", .description = "纯金打造的优雅链坠，御火辟邪。", .minLevel = 30, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::ResistFire});
  registerTemplate({.baseId = 3004, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Neck, .name = "红宝石项链", .description = "镶嵌璀璨红宝石的饰品，蕴含烈火之力。", .minLevel = 50, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::FlatFireDamage});
  registerTemplate({.baseId = 3005, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Neck, .name = "龙骨项链", .description = "远古巨龙之骨穿成的项链，充满野性狂暴。", .minLevel = 70, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::CritDamage});

  // Rings (3011 - 3015)
  registerTemplate({.baseId = 3011, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Ring, .name = "铁戒指", .description = "质朴的粗铁指环。", .minLevel = 1, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::FlatHealth});
  registerTemplate({.baseId = 3012, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Ring, .name = "银戒指", .description = "闪亮的纯银戒指，绝缘避雷。", .minLevel = 15, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::ResistLightning});
  registerTemplate({.baseId = 3013, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Ring, .name = "金戒指", .description = "高纯度金戒，平抑万物元素阻力。", .minLevel = 30, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::ResistAll});
  registerTemplate({.baseId = 3014, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Ring, .name = "蓝宝石戒指", .description = "湛蓝宝石散发清凉魔力气息。", .minLevel = 50, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::FlatMana});
  registerTemplate({.baseId = 3015, .kind = ItemKind::Jewelry, .type = ItemType::Jewelry, .slot = EquipmentSlot::Ring, .name = "钻石戒指", .description = "硬度极高的永恒钻石，带来敏锐直觉。", .minLevel = 70, .maxStack = 1, .baseStatMin = 0.0f, .baseStatMax = 0.0f, .implicitType = AffixType::CritChance});

  // ---------------------------------------------------------------------------
  // 4. Consumables & Potions (101 - 102)
  // ---------------------------------------------------------------------------
  registerTemplate({.baseId = 101, .kind = ItemKind::Consumable, .type = ItemType::Consumable, .slot = EquipmentSlot::None, .name = "生命药水", .description = "使用: 恢复 50 点生命值", .minLevel = 1, .maxStack = 99});
  registerTemplate({.baseId = 102, .kind = ItemKind::Consumable, .type = ItemType::Consumable, .slot = EquipmentSlot::None, .name = "法力药水", .description = "使用: 恢复 50 点法力值", .minLevel = 1, .maxStack = 99});

  // ---------------------------------------------------------------------------
  // 5. Bags (5001 - 5002)
  // ---------------------------------------------------------------------------
  registerTemplate({.baseId = 5001, .kind = ItemKind::Bag, .type = ItemType::Bag, .slot = EquipmentSlot::None, .name = "亚麻背包", .description = "增加一个背包页面 (56 格)。", .minLevel = 1, .maxStack = 1, .bagCapacity = 56});
  registerTemplate({.baseId = 5002, .kind = ItemKind::Bag, .type = ItemType::Bag, .slot = EquipmentSlot::None, .name = "魔法背包", .description = "增加一个背包页面 (56 格)。", .minLevel = 1, .maxStack = 1, .bagCapacity = 56});

  // ---------------------------------------------------------------------------
  // 6. Catalysts & Materials (10001)
  // ---------------------------------------------------------------------------
  registerTemplate({.baseId = 10001, .kind = ItemKind::Consumable, .type = ItemType::Consumable, .slot = EquipmentSlot::None, .catalystKind = CatalystKind::LegendaryCore, .name = "Legendary Core", .description = "Used in forging to influence outcomes.", .minLevel = 1, .maxStack = 99});

  LOG_INFO("ItemTemplateRegistry: initialized {} default templates.", m_templates.size());
}

} // namespace NoMoreDay
