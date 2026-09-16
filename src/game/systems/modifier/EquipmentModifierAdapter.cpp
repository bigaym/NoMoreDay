#include "game/systems/modifier/EquipmentModifierAdapter.hpp"

#include "core/logging/Logger.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/modifier/ModifierEvaluator.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"

#include <algorithm>
#include <atomic>
#include <span>

namespace NoMoreDay {
namespace {

// EquipmentSlot::Ring1(10) / Ring2(11) / Ring(12) 三者互为别名，ItemFactory 会把
// 戒指归一为 Ring。采集器把戒指槽位统一归一为该组合掩码，使记录无论按 Ring1、
// Ring2 还是通用 Ring 书写 equip_slot_mask，都能命中实际佩戴的任意戒指槽。
constexpr uint32_t kRingSlotMask =
    (1u << static_cast<uint32_t>(EquipmentSlot::Ring1)) |
    (1u << static_cast<uint32_t>(EquipmentSlot::Ring2)) |
    (1u << static_cast<uint32_t>(EquipmentSlot::Ring));

void CollectRecordRefsFromAffixes(const std::span<const Affix> affixes,
                                  const uint32_t slotBit,
                                  std::vector<EquippedRecordRef> &refs) {
  for (const auto &affix : affixes) {
    for (const uint32_t recordId : affix.modifier_record_ids) {
      refs.push_back(EquippedRecordRef{recordId, slotBit});
    }
  }
}

// 已装备记录引用的收集实现：写入调用方提供的缓冲，热路径可复用线程局部缓冲
// 以避免逐次堆分配。槽位按下标遍历，slot_bit 一般为 1u << 槽位下标（下标即
// EquipmentSlot 值），戒指槽归一为 kRingSlotMask；按 (slot_bit, record_id)
// 去重并排序，使求值顺序稳定，且同一槽位的记录连续排列，便于按槽分组求值。
void CollectEquippedRecordRefsInto(const entt::registry &registry,
                                   const entt::entity entity,
                                   std::vector<EquippedRecordRef> &out) {
  out.clear();
  const auto *equipment = registry.try_get<EquipmentComponent>(entity);
  if (equipment == nullptr) {
    return;
  }

  for (size_t slotIndex = 0; slotIndex < equipment->slots.size(); ++slotIndex) {
    const auto itemEntity = equipment->slots[slotIndex];
    if (!registry.valid(itemEntity) ||
        !registry.all_of<ItemComponent>(itemEntity)) {
      continue;
    }

    // 槽位掩码：戒指三槽互为别名，统一归一为 kRingSlotMask（10|11|12），
    // 其余槽位仍为 1u << 装备栏下标。
    uint32_t slotBit = 1u << static_cast<uint32_t>(slotIndex);
    const auto slotEnum = static_cast<EquipmentSlot>(slotIndex);
    if (slotEnum == EquipmentSlot::Ring1 || slotEnum == EquipmentSlot::Ring2 ||
        slotEnum == EquipmentSlot::Ring) {
      slotBit = kRingSlotMask;
    }
    const auto &item = registry.get<ItemComponent>(itemEntity);
    CollectRecordRefsFromAffixes(
        std::span<const Affix>(item.affixes.data(), item.affixes.size()), slotBit,
        out);
    CollectRecordRefsFromAffixes(
        std::span<const Affix>(item.implicits.data(), item.implicits.size()),
        slotBit, out);
  }

  const auto lessBySlotThenRecord = [](const EquippedRecordRef &lhs,
                                       const EquippedRecordRef &rhs) {
    if (lhs.slot_bit != rhs.slot_bit) {
      return lhs.slot_bit < rhs.slot_bit;
    }
    return lhs.record_id < rhs.record_id;
  };
  std::sort(out.begin(), out.end(), lessBySlotThenRecord);
  out.erase(std::unique(out.begin(), out.end(),
                        [](const EquippedRecordRef &lhs,
                           const EquippedRecordRef &rhs) {
                          return lhs.slot_bit == rhs.slot_bit &&
                                 lhs.record_id == rhs.record_id;
                        }),
            out.end());
}

// 一次性告警：运行时词缀表不可用时只报告一次，避免每帧刷屏。
std::atomic<bool> gWarnedRuntimeUnavailable{false};

// 已装备记录引用的统一求值入口：refs 已按 (slot_bit, record_id) 排序，同一槽位
// 的记录连续。逐槽复制基础上下文并设置 equip_slot_mask，每组只求值一次后合并；
// 空引用集或运行时表未加载时返回空 delta。两个消费点（技能等级加成 / 法力消耗
// 乘算）共用此守卫与求值路径，避免漂移。
[[nodiscard]] ModifierDelta
EvaluateEquippedRecordRefs(const std::span<const EquippedRecordRef> refs,
                           const ModifierEvalContext &baseCtx) {
  ModifierDelta total;
  if (refs.empty()) {
    return total;
  }

  auto &runtimeRegistry = ModifierRuntimeRegistry::Get();
  if (!runtimeRegistry.EnsureLoaded()) {
    if (!gWarnedRuntimeUnavailable.exchange(true)) {
      LOG_ERROR("EquipmentModifierAdapter: modifier runtime registry "
                "unavailable; equipped modifiers are inactive");
    }
    return total;
  }

  ModifierEvalContext slotCtx = baseCtx;
  // 逐槽记录 ID 分组缓冲：与 scratchRefs 同策略复用线程局部缓冲，避免逐帧堆分配。
  // 本函数不递归调用自身，单线程内每次调用 clear() 后用尽，重入安全。
  static thread_local std::vector<uint32_t> groupRecordIds;
  size_t begin = 0;
  while (begin < refs.size()) {
    const uint32_t slotBit = refs[begin].slot_bit;
    groupRecordIds.clear();
    size_t end = begin;
    while (end < refs.size() && refs[end].slot_bit == slotBit) {
      groupRecordIds.push_back(refs[end].record_id);
      ++end;
    }

    slotCtx.equip_slot_mask = slotBit;
    // 装备/天赋记录只应产生属性、怪物事件与行为算子；显式排除 SkillDelivery
    // (30..40)，避免非技能路径误施加技能交付算子（交付 op 由 SkillSpec 路径求值）。
    total.MergeFrom(ModifierEvaluator::Evaluate(
        runtimeRegistry,
        std::span<const uint32_t>(groupRecordIds.data(), groupRecordIds.size()),
        slotCtx, ModifierOpCategory::Stats | ModifierOpCategory::Events |
                     ModifierOpCategory::Behavior));
    begin = end;
  }
  return total;
}

} // namespace

ModifierEvalContext EquipmentModifierAdapter::BuildContextFromCharacter(
    const entt::registry &registry, const entt::entity entity,
    const uint32_t skillId, const Tag skillTags) {
  ModifierEvalContext ctx;
  ctx.skill_id = skillId;
  ctx.skill_tags = skillTags;

  // 职业：誓约后 mainProfession >= 0，转为掩码位编号。未誓约（kUnswornProfessionId
  // = -1，组件缺失时同取该默认值）与真实职业 0 目前同映射为 profession_id = 0
  // （设计显式接受），使 profession_mask 含 bit 0 的记录仍按既有数据生效。
  int mainProfession = kUnswornProfessionId;
  if (const auto *astrolabe = registry.try_get<AstrolabeComponent>(entity)) {
    mainProfession = astrolabe->mainProfession; // 拷贝 POD 后即释放指针
  }
  if (mainProfession >= 0) {
    ctx.profession_id = static_cast<uint32_t>(mainProfession);
  }

  // 武器类别：主/副手已装备武器的 WeaponSubtype 按位或。无真实武器时置位
  // WeaponSubtype::None（bit 0）表示空手，而非 0：过滤侧的 0 / 65535 / 0xFFFFFFFF
  // 仍判为通配，故既有装备记录不受影响；而把 weapon_class_mask 设为 1 的记录
  // 现在可精确限定为「仅徒手」。
  uint32_t weaponMask = 0u;
  if (const auto *equipment = registry.try_get<EquipmentComponent>(entity)) {
    const auto accumulateWeapon = [&registry, &weaponMask](
                                      const entt::entity itemEntity) {
      if (!registry.valid(itemEntity) ||
          !registry.all_of<ItemComponent>(itemEntity)) {
        return;
      }
      const auto &item = registry.get<ItemComponent>(itemEntity);
      if (item.weaponSubtype != WeaponSubtype::None) {
        weaponMask |= 1u << static_cast<uint32_t>(item.weaponSubtype);
      }
    };
    accumulateWeapon(
        equipment->slots[static_cast<size_t>(EquipmentSlot::MainHand)]);
    accumulateWeapon(
        equipment->slots[static_cast<size_t>(EquipmentSlot::OffHand)]);
  }
  const uint32_t unarmedMask = 1u << static_cast<uint32_t>(WeaponSubtype::None);
  ctx.weapon_class_mask = (weaponMask != 0u) ? weaponMask : unarmedMask;

  return ctx;
}

std::vector<uint32_t> EquipmentModifierAdapter::CollectEquippedRecordIds(
    const entt::registry &registry, const entt::entity entity) {
  std::vector<EquippedRecordRef> refs;
  CollectEquippedRecordRefsInto(registry, entity, refs);

  // 仅 ID 视图：跨槽去重后按 ID 升序返回，保持既有公开语义。
  std::vector<uint32_t> ids;
  ids.reserve(refs.size());
  for (const EquippedRecordRef &ref : refs) {
    ids.push_back(ref.record_id);
  }
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

std::vector<EquippedRecordRef> EquipmentModifierAdapter::CollectEquippedRecordRefs(
    const entt::registry &registry, const entt::entity entity) {
  std::vector<EquippedRecordRef> refs;
  CollectEquippedRecordRefsInto(registry, entity, refs);
  return refs;
}

void EquipmentModifierAdapter::ApplyEquippedSkillLevelBonuses(
    entt::registry &registry, const entt::entity entity) {
  auto *activeSkills = registry.try_get<ActiveSkillsComponent>(entity);
  if (activeSkills == nullptr) {
    return;
  }

  const auto refs = CollectEquippedRecordRefs(registry, entity);
  if (refs.empty()) {
    return;
  }
  const std::span<const EquippedRecordRef> refSpan(refs.data(), refs.size());

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
    const auto delta = EvaluateEquippedRecordRefs(refSpan, ctx);
    slot.bonus_levels += static_cast<int>(delta.GetSkillLevelBonus(slot.skill_id));
  }
}

float EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
    const entt::registry &registry, const entt::entity entity,
    const uint32_t skillId, const Tag skillTags) {
  // 本函数由技能烘焙调用，而烘焙可经战斗路径逐帧触发：复用线程局部缓冲避免
  // 逐次堆分配（每次调用内用尽，不跨调用持有）。
  thread_local std::vector<EquippedRecordRef> scratchRefs;
  CollectEquippedRecordRefsInto(registry, entity, scratchRefs);
  if (scratchRefs.empty()) {
    return 1.0f;
  }

  // 过滤条件取自记录自身的技能白名单与标签；上下文按槽位分组施加 equip_slot_mask，
  // 并填充职业/武器类别，故记录上的槽位、职业与武器过滤均生效。
  const auto ctx =
      BuildContextFromCharacter(registry, entity, skillId, skillTags);
  const auto delta = EvaluateEquippedRecordRefs(
      std::span<const EquippedRecordRef>(scratchRefs.data(), scratchRefs.size()),
      ctx);
  return delta.GetManaCostMultiplier(skillId);
}

} // namespace NoMoreDay
