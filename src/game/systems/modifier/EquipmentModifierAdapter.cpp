#include "game/systems/modifier/EquipmentModifierAdapter.hpp"

#include "core/logging/Logger.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <algorithm>
#include <atomic>
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

// 已装备记录 ID 的收集实现：写入调用方提供的缓冲，热路径可复用线程局部缓冲
// 以避免逐次堆分配。排序使记录求值顺序稳定，与词缀在物品上的排列无关。
void CollectEquippedRecordIdsInto(const entt::registry &registry,
                                  const entt::entity entity,
                                  std::vector<uint32_t> &out) {
  out.clear();
  const auto *equipment = registry.try_get<EquipmentComponent>(entity);
  if (equipment == nullptr) {
    return;
  }

  for (const auto itemEntity : equipment->slots) {
    if (!registry.valid(itemEntity) ||
        !registry.all_of<ItemComponent>(itemEntity)) {
      continue;
    }

    const auto &item = registry.get<ItemComponent>(itemEntity);
    CollectRecordIdsFromAffixes(
        std::span<const Affix>(item.affixes.data(), item.affixes.size()), out);
    CollectRecordIdsFromAffixes(
        std::span<const Affix>(item.implicits.data(), item.implicits.size()),
        out);
  }

  std::sort(out.begin(), out.end());
}

// 一次性告警：运行时词缀表不可用时只报告一次，避免每帧刷屏。
std::atomic<bool> gWarnedRuntimeUnavailable{false};

// 已装备记录集的统一求值入口：空记录集或运行时表未加载时返回空 delta。
// 两个消费点（技能等级加成 / 法力消耗乘算）共用此守卫与求值路径，避免漂移。
[[nodiscard]] ModifierDelta
EvaluateEquippedRecords(const std::span<const uint32_t> recordIds,
                        const ModifierEvalContext &ctx) {
  ModifierDelta delta;
  if (recordIds.empty()) {
    return delta;
  }

  auto &runtimeRegistry = ModifierRuntimeRegistry::Get();
  if (!runtimeRegistry.EnsureLoaded()) {
    if (!gWarnedRuntimeUnavailable.exchange(true)) {
      LOG_ERROR("EquipmentModifierAdapter: modifier runtime registry "
                "unavailable; equipped modifiers are inactive");
    }
    return delta;
  }

  return ModifierEvaluator::Evaluate(runtimeRegistry, recordIds, ctx);
}

} // namespace

ModifierEvalContext EquipmentModifierAdapter::BuildContextFromCharacter(
    const entt::registry &registry, const entt::entity entity,
    const uint32_t skillId, const Tag skillTags) {
  // 当前仅填充技能维度：职业、武器类别与装备槽位保持默认通配值，
  // 因此记录上这三类过滤条件目前不生效。若后续需要按槽位过滤，
  // 需连同记录来源槽一并携带并在下方填充 equip_slot_mask。
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
  CollectEquippedRecordIdsInto(registry, entity, ids);
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

  for (auto &slot : activeSkills->specialized_slots) {
    if (slot.skill_id == INVALID_SKILL_ID) {
      continue;
    }

    Tag skillTags = Tag::None;
    if (const auto *skill = SkillRegistry::Get().GetSkill(slot.skill_id)) {
      skillTags = skill->tags;
    }

    const auto ctx =
        BuildContextFromCharacter(registry, entity, slot.skill_id, skillTags);
    const auto delta = EvaluateEquippedRecords(
        std::span<const uint32_t>(recordIds.data(), recordIds.size()), ctx);
    slot.bonus_levels += static_cast<int>(delta.GetSkillLevelBonus(slot.skill_id));
  }
}

float EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
    const entt::registry &registry, const entt::entity entity,
    const uint32_t skillId, const Tag skillTags) {
  // 本函数由技能烘焙调用，而烘焙可经战斗路径逐帧触发：复用线程局部缓冲避免
  // 逐次堆分配（每次调用内用尽，不跨调用持有）。
  thread_local std::vector<uint32_t> scratchRecordIds;
  CollectEquippedRecordIdsInto(registry, entity, scratchRecordIds);
  if (scratchRecordIds.empty()) {
    return 1.0f;
  }

  // 过滤条件取自记录自身的技能白名单与标签；上下文未填充槽位/职业/武器类别，
  // 这些过滤字段现等效通配（见 BuildContextFromCharacter）。
  const auto ctx =
      BuildContextFromCharacter(registry, entity, skillId, skillTags);
  const auto delta = EvaluateEquippedRecords(
      std::span<const uint32_t>(scratchRecordIds.data(), scratchRecordIds.size()),
      ctx);
  return delta.GetManaCostMultiplier(skillId);
}

} // namespace NoMoreDay
