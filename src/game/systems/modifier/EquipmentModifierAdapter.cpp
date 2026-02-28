#include "game/systems/modifier/EquipmentModifierAdapter.hpp"

#include "game/components/EquipmentComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <algorithm>
#include <span>

namespace NoMoreDay {
namespace {

void CollectRecordIdsFromAffixes(const std::span<const Affix> affixes,
                                 std::vector<uint32_t> &ids) {
  for (const auto &affix : affixes) {
    ids.insert(ids.end(), affix.modifier_record_ids.begin(),
               affix.modifier_record_ids.end());
  }
}

} // namespace

ModifierEvalContext EquipmentModifierAdapter::BuildContextFromCharacter(
    const entt::registry &registry, const entt::entity entity,
    const uint32_t skillId, const Tag skillTags) {
  (void)registry;
  (void)entity;

  ModifierEvalContext ctx;
  ctx.skill_id = skillId;
  ctx.skill_tags = skillTags;
  return ctx;
}

std::vector<uint32_t> EquipmentModifierAdapter::CollectEquippedRecordIds(
    const entt::registry &registry, const entt::entity entity) {
  std::vector<uint32_t> ids;
  const auto *equipment = registry.try_get<EquipmentComponent>(entity);
  if (equipment == nullptr) {
    return ids;
  }

  for (const auto itemEntity : equipment->slots) {
    if (!registry.valid(itemEntity) ||
        !registry.all_of<ItemComponent>(itemEntity)) {
      continue;
    }

    const auto &item = registry.get<ItemComponent>(itemEntity);
    CollectRecordIdsFromAffixes(
        std::span<const Affix>(item.affixes.data(), item.affixes.size()), ids);
    CollectRecordIdsFromAffixes(
        std::span<const Affix>(item.implicits.data(), item.implicits.size()), ids);
  }

  std::sort(ids.begin(), ids.end());
  return ids;
}

void EquipmentModifierAdapter::ApplyEquippedSkillLevelBonuses(
    entt::registry &registry, const entt::entity entity) {
  auto *activeSkills = registry.try_get<ActiveSkillsComponent>(entity);
  if (activeSkills == nullptr) {
    return;
  }

  const auto recordIds = CollectEquippedRecordIds(registry, entity);
  if (recordIds.empty()) {
    return;
  }

  auto &runtimeRegistry = ModifierRuntimeRegistry::Get();

  for (auto &slot : activeSkills->specialized_slots) {
    if (slot.skill_id == 0u) {
      continue;
    }

    Tag skillTags = Tag::None;
    if (const auto *skill = SkillRegistry::Get().GetSkill(slot.skill_id)) {
      skillTags = skill->tags;
    }

    const auto ctx =
        BuildContextFromCharacter(registry, entity, slot.skill_id, skillTags);
    const auto delta = ModifierEvaluator::Evaluate(
        runtimeRegistry,
        std::span<const uint32_t>(recordIds.data(), recordIds.size()), ctx);
    slot.bonus_levels += static_cast<int>(delta.GetSkillLevelBonus(slot.skill_id));
  }
}

} // namespace NoMoreDay
