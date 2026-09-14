#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/EndgameModifierContract.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "game/contracts/CombatEvents.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include <atomic>
#include <algorithm>
#include <fstream>
#include <limits>
#include <type_traits>
#include <utility>

namespace NoMoreDay::systems {
namespace {

std::atomic<uint64_t> g_ailmentInstanceCounter{1};

constexpr std::string_view kPrefix = "ailment:";

// BuffEffect stores the managed-ailment identity as a uint8_t (see Buff.hpp)
// so that game/foundation/components does not depend on game/contracts.
// Pin the AilmentType layout here: any change to the enum's underlying type
// or to the None value breaks the storage contract and must be re-examined.
static_assert(sizeof(AilmentType) == sizeof(uint8_t),
              "AilmentType must stay uint8_t-backed to match BuffEffect::ailment_type");
static_assert(static_cast<uint8_t>(AilmentType::None) == 0,
              "AilmentType::None must remain 0 to match BuffEffect::ailment_type default");

AilmentType AilmentTypeFromStorage(uint8_t value) {
  if (value >= static_cast<uint8_t>(AilmentType::Count)) {
    return AilmentType::None;
  }
  return static_cast<AilmentType>(value);
}

uint8_t AilmentTypeToStorage(AilmentType value) {
  return static_cast<uint8_t>(value);
}

// 153 饮血刃（流云刺技能 1 节点）：判断 entity 的流云刺专精是否分配了节点 153。
// 流血 DoT 结算后据此决定是否按实际流血伤害治疗施加者。
bool HasFlowingThrustBloodDrinker(entt::registry &registry, entt::entity entity) {
  if (!registry.valid(entity)) {
    return false;
  }
  const auto *act = registry.try_get<ActiveSkillsComponent>(entity);
  if (!act) {
    return false;
  }
  for (const auto &spec : act->specialized_slots) {
    if (spec.skill_id == 1) {
      return skills::HasNode(spec, 153);
    }
  }
  return false;
}

std::string_view AilmentTypeToString(AilmentType value) {
  switch (value) {
  case AilmentType::Ignite:
    return "Ignite";
  case AilmentType::Chill:
    return "Chill";
  case AilmentType::Freeze:
    return "Freeze";
  case AilmentType::Shock:
    return "Shock";
  case AilmentType::Poison:
    return "Poison";
  case AilmentType::Bleed:
    return "Bleed";
  case AilmentType::Stun:
    return "Stun";
  case AilmentType::Slow:
    return "Slow";
  case AilmentType::Blind:
    return "Blind";
  case AilmentType::None:
  case AilmentType::Count:
    return "None";
  }
  return "None";
}

std::optional<AilmentType> AilmentTypeFromString(std::string_view value) {
  if (value == "Ignite")
    return AilmentType::Ignite;
  if (value == "Chill")
    return AilmentType::Chill;
  if (value == "Freeze")
    return AilmentType::Freeze;
  if (value == "Shock")
    return AilmentType::Shock;
  if (value == "Poison")
    return AilmentType::Poison;
  if (value == "Bleed")
    return AilmentType::Bleed;
  if (value == "Stun")
    return AilmentType::Stun;
  if (value == "Slow")
    return AilmentType::Slow;
  if (value == "Blind")
    return AilmentType::Blind;
  return std::nullopt;
}

RefreshPolicy RefreshPolicyFromString(std::string_view value) {
  if (value == "Extend")
    return RefreshPolicy::Extend;
  if (value == "Independent")
    return RefreshPolicy::Independent;
  return RefreshPolicy::Refresh;
}

OverwritePolicy OverwritePolicyFromString(std::string_view value) {
  if (value == "Newest")
    return OverwritePolicy::Newest;
  if (value == "Additive")
    return OverwritePolicy::Additive;
  return OverwritePolicy::Strongest;
}

DamagePoolPolicy DamagePoolPolicyFromString(std::string_view value) {
  if (value == "Consolidated")
    return DamagePoolPolicy::Consolidated;
  return DamagePoolPolicy::PerStack;
}

BuffType BuffTypeFromString(std::string_view value, BuffType fallback) {
  if (value == "Burn")
    return BuffType::Burn;
  if (value == "Poison")
    return BuffType::Poison;
  if (value == "Bleed")
    return BuffType::Bleed;
  if (value == "Freeze")
    return BuffType::Freeze;
  if (value == "Shock")
    return BuffType::Shock;
  if (value == "SpeedDown" || value == "Slow" || value == "Chill")
    return BuffType::SpeedDown;
  if (value == "DamageOverTime")
    return BuffType::DamageOverTime;
  return fallback;
}

Tag DefaultDamageTag(AilmentType ailment) {
  switch (ailment) {
  case AilmentType::Ignite:
    return Tag::Fire;
  case AilmentType::Bleed:
    return Tag::Physical;
  case AilmentType::Poison:
    return Tag::Poison;
  case AilmentType::Freeze:
  case AilmentType::Chill:
    return Tag::Cold;
  case AilmentType::Shock:
    return Tag::Lightning;
  case AilmentType::Slow:
    // B2-18：Slow 为纯移速减益身份，无伤害元素；Tag::None 使
    // SyncAilmentSnapshot 与 Tick 跳过伤害结算。
    return Tag::None;
  default:
    return Tag::Poison;
  }
}

void SetRefresh(BuffEffect &effect, RefreshPolicy policy, float duration) {
  switch (policy) {
  case RefreshPolicy::Refresh:
    effect.duration = duration;
    effect.remaining = duration;
    return;
  case RefreshPolicy::Extend:
    effect.remaining += duration;
    effect.duration = std::max(effect.duration, effect.remaining);
    return;
  case RefreshPolicy::Independent:
    return;
  }
}

void ApplyOverwrite(BuffEffect &effect, OverwritePolicy policy, float magnitude) {
  switch (policy) {
  case OverwritePolicy::Strongest:
    effect.tick_damage = std::max(effect.tick_damage, magnitude);
    return;
  case OverwritePolicy::Newest:
    effect.tick_damage = magnitude;
    return;
  case OverwritePolicy::Additive:
    effect.tick_damage += magnitude;
    return;
  }
}

std::vector<size_t> CollectAilmentIndices(const ActiveEffectsComponent &activeEffects,
                                          AilmentType ailment) {
  std::vector<size_t> indices;
  indices.reserve(activeEffects.effects.size());
  for (size_t i = 0; i < activeEffects.effects.size(); ++i) {
    auto mapped = AilmentAdapter::TryMapLegacyBuff(activeEffects.effects[i]);
    if (mapped && *mapped == ailment) {
      indices.push_back(i);
    }
  }
  return indices;
}

// 异常类别到数值类别 (BuffKind) 的映射：技能1 破阵流按元素异常状态触发衍生
// 效果，热路径需要整数比较。AilmentType 中未被战斗热路径消费的类别
// (Shock/Poison/Stun/Blind) 映射为 None。
BuffKind BuffKindFromAilment(AilmentType ailment) {
  switch (ailment) {
  case AilmentType::Bleed:
    return BuffKind::Bleed;
  case AilmentType::Ignite:
    return BuffKind::Ignite;
  case AilmentType::Chill:
    return BuffKind::Chill;
  case AilmentType::Freeze:
    return BuffKind::Freeze;
  case AilmentType::Slow:
    return BuffKind::Slow;
  default:
    return BuffKind::None;
  }
}

BuffEffect BuildAilmentEffect(const AilmentContract &contract,
                              const AilmentApplyRequest &request,
                              float duration, float magnitude, uint64_t instance) {
  BuffEffect effect;
  // The runtime id string is kept for save-file and log compatibility, but the
  // structured fields below are the authoritative identity on the hot path.
  effect.id = AilmentAdapter::BuildRuntimeId(request.ailment, instance);
  effect.managed_ailment = true;
  effect.ailment_type = AilmentTypeToStorage(request.ailment);
  effect.ailment_power = std::max(0.0f, magnitude);
  effect.name = std::string(AilmentTypeToString(request.ailment));
  effect.description = "Managed ailment";
  effect.type = contract.legacy_buff_type;
  effect.kind = BuffKindFromAilment(request.ailment);
  effect.duration = duration;
  effect.remaining = duration;
  effect.stacks = std::max<int>(1, request.stacks);
  effect.max_stacks = std::max<int>(1, contract.max_stacks);
  effect.tick_interval = std::max(0.01f, contract.tick_interval);
  effect.tick_damage = std::max(0.0f, magnitude);
  effect.tick_timer = 0.0f;
  effect.tick_damage_tag = contract.damage_tag;
  effect.is_debuff = true;
  effect.source = request.source;
  // 记录施加来源技能，供 153 饮血刃等按来源技能门控的结算逻辑使用。
  effect.source_skill_id = request.source_skill_id;
  return effect;
}

size_t SelectOverwriteIndex(const std::vector<size_t> &indices,
                            const ActiveEffectsComponent &activeEffects,
                            OverwritePolicy policy) {
  size_t selected = indices.front();
  switch (policy) {
  case OverwritePolicy::Strongest: {
    float weakest = std::numeric_limits<float>::max();
    for (const size_t index : indices) {
      const float current = activeEffects.effects[index].tick_damage;
      if (current < weakest) {
        weakest = current;
        selected = index;
      }
    }
    break;
  }
  case OverwritePolicy::Newest: {
    float shortestRemaining = std::numeric_limits<float>::max();
    for (const size_t index : indices) {
      const float remaining = activeEffects.effects[index].remaining;
      if (remaining < shortestRemaining) {
        shortestRemaining = remaining;
        selected = index;
      }
    }
    break;
  }
  case OverwritePolicy::Additive: {
    float longestRemaining = -1.0f;
    for (const size_t index : indices) {
      const float remaining = activeEffects.effects[index].remaining;
      if (remaining > longestRemaining) {
        longestRemaining = remaining;
        selected = index;
      }
    }
    break;
  }
  }
  return selected;
}

} // namespace

AilmentRegistry &AilmentRegistry::Get() {
  static AilmentRegistry instance;
  return instance;
}

const AilmentContract *AilmentRegistry::Find(AilmentType ailment) const {
  const auto iter = m_contracts.find(ailment);
  if (iter != m_contracts.end()) {
    return &iter->second;
  }
  return nullptr;
}

bool AilmentRegistry::EnsureLoaded() {
  if (m_loaded) {
    return true;
  }
  (void)LoadFromFile();
  return m_loaded;
}

bool AilmentRegistry::LoadFromFile(const std::string &path) {
  std::unordered_map<AilmentType, AilmentContract, AilmentTypeHash> loaded;
  m_contracts.clear();
  LoadBuiltins();

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_WARN("AilmentRegistry: failed to open {}, fallback to builtins.", path);
    m_loaded = true;
    return false;
  }

  nlohmann::json root;
  try {
    file >> root;
  } catch (const std::exception &e) {
    LOG_ERROR("AilmentRegistry: invalid json in {}: {}", path, e.what());
    m_loaded = true;
    return false;
  }

  const auto it = root.find("ailments");
  if (it == root.end() || !it->is_array()) {
    LOG_WARN("AilmentRegistry: {} missing ailments array, keep builtins.", path);
    m_loaded = true;
    return false;
  }

  for (const auto &entry : *it) {
    const std::string typeText = entry.value("type", "None");
    const auto maybeType = AilmentTypeFromString(typeText);
    if (!maybeType || *maybeType == AilmentType::None) {
      LOG_WARN("AilmentRegistry: skip unknown ailment type '{}'.", typeText);
      continue;
    }

    AilmentContract contract;
    contract.ailment = *maybeType;
    contract.max_stacks = static_cast<uint8_t>(
        std::clamp(entry.value("max_stacks", 1), 1, 255));
    contract.refresh_policy =
        RefreshPolicyFromString(entry.value("refresh_policy", "Refresh"));
    contract.overwrite_policy =
        OverwritePolicyFromString(entry.value("overwrite_policy", "Strongest"));
    contract.immunity_and_resistance =
        std::clamp(entry.value("immunity_and_resistance", 1.0f), 0.0f, 1.0f);
    contract.tick_interval = std::max(0.01f, entry.value("tick_interval", 1.0f));
    contract.damage_pool_policy =
        DamagePoolPolicyFromString(entry.value("damage_pool_policy", "PerStack"));
    contract.base_duration = std::max(0.01f, entry.value("base_duration", 1.0f));

    const std::string damageTagText =
        entry.value("damage_tag", std::string(AilmentTypeToString(*maybeType)));
    if (const auto parsedTag = TagFromString(damageTagText); parsedTag) {
      contract.damage_tag = *parsedTag;
    } else {
      contract.damage_tag = DefaultDamageTag(*maybeType);
    }

    const BuffType fallbackLegacy = AilmentAdapter::ToLegacyBuffType(*maybeType);
    contract.legacy_buff_type = BuffTypeFromString(
        entry.value("legacy_buff_type", "DamageOverTime"), fallbackLegacy);
    loaded[contract.ailment] = contract;
  }

  if (!loaded.empty()) {
    for (const auto &[ailment, contract] : loaded) {
      m_contracts[ailment] = contract;
    }
  }

  m_loaded = true;
  return true;
}

void AilmentRegistry::ResetForTests() {
  m_contracts.clear();
  m_loaded = false;
}

void AilmentRegistry::LoadBuiltins() {
  auto setContract = [this](AilmentType ailment, uint8_t maxStacks,
                            RefreshPolicy refresh, OverwritePolicy overwrite,
                            float interval, DamagePoolPolicy poolPolicy,
                            float baseDuration, Tag tag, BuffType legacyType) {
    AilmentContract contract;
    contract.ailment = ailment;
    contract.max_stacks = maxStacks;
    contract.refresh_policy = refresh;
    contract.overwrite_policy = overwrite;
    contract.immunity_and_resistance = 1.0f;
    contract.tick_interval = interval;
    contract.damage_pool_policy = poolPolicy;
    contract.base_duration = baseDuration;
    contract.damage_tag = tag;
    contract.legacy_buff_type = legacyType;
    m_contracts[ailment] = contract;
  };

  setContract(AilmentType::Poison, 5, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 0.5f, DamagePoolPolicy::PerStack, 4.0f,
              Tag::Poison, BuffType::Poison);
  setContract(AilmentType::Ignite, 4, RefreshPolicy::Extend,
              OverwritePolicy::Newest, 0.5f, DamagePoolPolicy::Consolidated, 3.0f,
              Tag::Fire, BuffType::Burn);
  setContract(AilmentType::Bleed, 2, RefreshPolicy::Independent,
              OverwritePolicy::Additive, 0.5f, DamagePoolPolicy::PerStack, 2.5f,
              Tag::Physical, BuffType::Bleed);
  setContract(AilmentType::Chill, 3, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 3.0f,
              Tag::Cold, BuffType::SpeedDown);
  setContract(AilmentType::Freeze, 1, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 1.0f,
              Tag::Cold, BuffType::Freeze);
  setContract(AilmentType::Shock, 1, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 3.0f,
              Tag::Lightning, BuffType::Shock);
  // B2-18：Slow 身份型契约（172/175 与 HazardSystem 减速载体共用的异常类别）。
  // damage_tag=Tag::None 表示不产生 tick 伤害——移速修正由 BuffEffect::modifiers
  // 承载，见 AilmentAdapter::BuildMoveSpeedDebuff。base_duration 与
  // skill_mechanics.json 的 172.slow_duration 等价映射。
  setContract(AilmentType::Slow, 1, RefreshPolicy::Refresh,
              OverwritePolicy::Strongest, 1.0f, DamagePoolPolicy::PerStack, 2.5f,
              Tag::None, BuffType::SpeedDown);
}

std::optional<AilmentType> AilmentAdapter::TryMapLegacyBuff(
    const BuffEffect &effect) {
  // Fast path: structured identity written by AilmentEngine. No string work.
  if (effect.managed_ailment) {
    const AilmentType stored = AilmentTypeFromStorage(effect.ailment_type);
    if (stored != AilmentType::None) {
      return stored;
    }
  }

  // Fallback: save files written before the structured-identity change carry
  // the identity inside the "ailment:" id string only. Keep parsing it so old
  // saves keep ticking exactly as before.
  AilmentType parsedFromId = AilmentType::None;
  if (IsManagedAilmentId(effect.id, &parsedFromId) &&
      parsedFromId != AilmentType::None) {
    return parsedFromId;
  }

  switch (effect.type) {
  case BuffType::Burn:
    return AilmentType::Ignite;
  case BuffType::Poison:
    return AilmentType::Poison;
  case BuffType::Bleed:
    return AilmentType::Bleed;
  case BuffType::Freeze:
    return AilmentType::Freeze;
  case BuffType::Shock:
    return AilmentType::Shock;
  case BuffType::SpeedDown:
    return AilmentType::Chill;
  case BuffType::DamageOverTime:
    if (HasTag(effect.tick_damage_tag, Tag::Fire)) {
      return AilmentType::Ignite;
    }
    if (HasTag(effect.tick_damage_tag, Tag::Physical)) {
      return AilmentType::Bleed;
    }
    return AilmentType::Poison;
  default:
    break;
  }
  return std::nullopt;
}

BuffType AilmentAdapter::ToLegacyBuffType(AilmentType ailment) {
  switch (ailment) {
  case AilmentType::Ignite:
    return BuffType::Burn;
  case AilmentType::Poison:
    return BuffType::Poison;
  case AilmentType::Bleed:
    return BuffType::Bleed;
  case AilmentType::Freeze:
    return BuffType::Freeze;
  case AilmentType::Shock:
    return BuffType::Shock;
  case AilmentType::Chill:
  case AilmentType::Slow:
    return BuffType::SpeedDown;
  default:
    return BuffType::DamageOverTime;
  }
}

BuffEffect AilmentAdapter::BuildMoveSpeedDebuff(
    AilmentType identity, std::string id, std::string name,
    std::string description, BuffKind kind, float slowFraction,
    float duration) {
  // B2-18：移速减速载体的唯一构建口。ailment 契约的 magnitude 语义是每 tick
  // 伤害、不承载 StatModifier，故移速修正在此统一构造（而非复写 AilmentApplier），
  // 供流云刺 172/175 与 HazardSystem 冰冻球减速共用，避免同一 id 在多创建点
  // 出现 type/kind/modifiers 漂移（见 2026-09-12 评审 F1/F7）。
  // 本函数不设置 tick_damage：减速为纯移速减益，不产生 tick 伤害。
  BuffEffect effect;
  effect.id = std::move(id);
  effect.name = std::move(name);
  effect.description = std::move(description);
  effect.type = ToLegacyBuffType(identity);
  effect.kind = kind;
  effect.is_debuff = true;
  effect.duration = duration;
  effect.remaining = duration;
  effect.modifiers.push_back({.value = -slowFraction * 100.0f,
                              .type = StatType::MoveSpeed,
                              .mode = ModifierMode::PercentAdd});
  return effect;
}

Tag AilmentAdapter::ResolveDamageTag(AilmentType ailment, const BuffEffect &effect) {
  if (effect.tick_damage_tag != Tag::None) {
    return effect.tick_damage_tag;
  }
  return DefaultDamageTag(ailment);
}

std::string AilmentAdapter::BuildRuntimeId(AilmentType ailment, uint64_t instance) {
  std::string id = std::string(kPrefix);
  id.append(AilmentTypeToString(ailment));
  if (instance != 0) {
    id.push_back(':');
    id.append(std::to_string(instance));
  }
  return id;
}

bool AilmentAdapter::IsManagedAilmentId(std::string_view id,
                                        AilmentType *parsed) {
  if (!id.starts_with(kPrefix)) {
    return false;
  }

  const std::string_view payload = id.substr(kPrefix.size());
  const size_t split = payload.find(':');
  const std::string_view ailmentText =
      split == std::string_view::npos ? payload : payload.substr(0, split);

  const auto maybeType = AilmentTypeFromString(ailmentText);
  if (!maybeType) {
    return false;
  }
  if (parsed) {
    *parsed = *maybeType;
  }
  return true;
}

namespace {

// P3-2: 为产生伤害的异常创建/刷新冻结快照载体。载体实体挂
// DamageSnapshotComponent，tick 结算时作为 source_entity 被 DamagePipeline
// 直接消费；因此施加之后再给攻方叠加增益，也不会改变该异常后续 tick 的数值。
void SyncAilmentSnapshot(entt::registry &registry, entt::entity target,
                         BuffEffect &effect, const AilmentContract &contract,
                         entt::entity source) {
  if (!registry.valid(source) || effect.tick_damage <= 0.0f ||
      contract.damage_tag == Tag::None) {
    return;
  }

  entt::entity holder = effect.snapshot_source;
  if (!registry.valid(holder)) {
    holder = registry.create();
    damage::AilmentSnapshotHolderComponent marker;
    marker.target = target;
    registry.emplace<damage::AilmentSnapshotHolderComponent>(holder, marker);
    effect.snapshot_source = holder;
  }

  // 将施加时点的叠层一并烘焙，保证 PerStack 口径与 tick 侧一致。
  const float tickBase =
      contract.damage_pool_policy == DamagePoolPolicy::PerStack
          ? effect.tick_damage * static_cast<float>(std::max(1, effect.stacks))
          : effect.tick_damage;
  DamagePool pool;
  pool.Add(contract.damage_tag, tickBase);
  DamagePipeline::AttachSnapshotComponent(
      registry, holder, source, 0, pool, Tag::DamageOverTime, holder, nullptr,
      DamageOrigin::AilmentTick);
}

} // namespace

bool AilmentApplier::Apply(entt::registry &registry, entt::entity target,
                           const AilmentApplyRequest &request) {
  if (!registry.valid(target) || request.ailment == AilmentType::None) {
    return false;
  }

  // 989 天山雪隐: 持有 Cold 转质绝影形态的实体免疫冰冻
  if (request.ailment == AilmentType::Freeze) {
    const auto *pt = registry.try_get<PhantomTranceComponent>(target);
    if (pt != nullptr && pt->remaining > 0.0f &&
        pt->params.transmuter_tag == Tag::Cold) {
      return false;
    }
  }

  auto &contracts = AilmentRegistry::Get();
  if (!contracts.EnsureLoaded()) {
    return false;
  }

  const AilmentContract *contract = contracts.Find(request.ailment);
  if (!contract || contract->immunity_and_resistance <= 0.0f) {
    return false;
  }
  if (registry.valid(request.source) &&
      !ProcBudgetManager::Get().RequestProc(
          request.source, ProcBudgetType::AilmentProc, 1.0f)) {
    return false;
  }

  auto &activeEffects = registry.get_or_emplace<ActiveEffectsComponent>(target);
  auto matchingIndices = CollectAilmentIndices(activeEffects, request.ailment);

  auto &endgameRegistry = EndgameModifierRegistry::Get();
  (void)endgameRegistry.EnsureLoaded();
  const auto endgame =
      endgameRegistry.ResolveForEntities(registry, request.source, target)
          .aggregate;

  const float ailmentMagnitudeMultiplier = std::max(
      0.0f, 1.0f + endgame.outgoing_ailment_magnitude_more +
                endgame.incoming_ailment_taken_more);
  const float ailmentDurationMultiplier = std::max(
      0.05f, 1.0f + endgame.outgoing_ailment_duration_more +
                 endgame.incoming_ailment_duration_bonus);

  const float duration =
      (request.duration > 0.0f ? request.duration : contract->base_duration) *
      ailmentDurationMultiplier;
  const float magnitude = std::max(
      0.0f, request.magnitude * contract->immunity_and_resistance *
                ailmentMagnitudeMultiplier);
  const uint8_t incomingStacks = std::max<uint8_t>(1, request.stacks);

  if (contract->refresh_policy == RefreshPolicy::Independent) {
    if (matchingIndices.size() < contract->max_stacks) {
      const uint64_t instance = g_ailmentInstanceCounter.fetch_add(1);
      auto effect = BuildAilmentEffect(*contract, request, duration, magnitude,
                                       instance);
      effect.stacks = 1;
      activeEffects.effects.push_back(effect);
      SyncAilmentSnapshot(registry, target, activeEffects.effects.back(),
                          *contract, request.source);
      return true;
    }

    const size_t selected =
        SelectOverwriteIndex(matchingIndices, activeEffects,
                             contract->overwrite_policy);
    auto &slot = activeEffects.effects[selected];

    if (contract->overwrite_policy == OverwritePolicy::Additive) {
      slot.tick_damage += magnitude;
      SetRefresh(slot, RefreshPolicy::Refresh, duration);
      slot.source = request.source;
      // 叠加合并后必须同步来源技能归属：153 回血门控按 source_skill_id==1 判定，
      // 仅同步 source 会让归属停留在旧施加者，导致跨来源叠加后回血误判。
      slot.source_skill_id = request.source_skill_id;
      // Keep the structured identity in sync with the in-place refresh.
      slot.managed_ailment = true;
      slot.ailment_type = AilmentTypeToStorage(request.ailment);
      slot.ailment_power = slot.tick_damage;
      SyncAilmentSnapshot(registry, target, slot, *contract, request.source);
      return true;
    }

    if (contract->overwrite_policy == OverwritePolicy::Strongest &&
        magnitude <= slot.tick_damage) {
      SetRefresh(slot, RefreshPolicy::Refresh, duration);
      SyncAilmentSnapshot(registry, target, slot, *contract, request.source);
      return true;
    }

    const uint64_t instance = g_ailmentInstanceCounter.fetch_add(1);
    auto replacement =
        BuildAilmentEffect(*contract, request, duration, magnitude, instance);
    replacement.stacks = 1;
    slot = replacement;
    SyncAilmentSnapshot(registry, target, slot, *contract, request.source);
    return true;
  }

  if (matchingIndices.empty()) {
    auto effect = BuildAilmentEffect(*contract, request, duration, magnitude, 0);
    effect.stacks = std::min(static_cast<int>(incomingStacks),
                             static_cast<int>(contract->max_stacks));
    activeEffects.effects.push_back(effect);
    SyncAilmentSnapshot(registry, target, activeEffects.effects.back(),
                        *contract, request.source);
    return true;
  }

  auto &effect = activeEffects.effects[matchingIndices.front()];
  effect.max_stacks = std::max<int>(1, contract->max_stacks);
  effect.tick_interval = std::max(0.01f, contract->tick_interval);
  effect.tick_damage_tag = contract->damage_tag;
  effect.type = contract->legacy_buff_type;
  effect.is_debuff = true;
  effect.source = request.source;
  // 叠加合并后必须同步来源技能归属：153 回血门控按 source_skill_id==1 判定，
  // 仅同步 source 会让归属停留在旧施加者，导致跨来源叠加后回血误判。
  effect.source_skill_id = request.source_skill_id;
  // Promote to the structured identity: the id string (or legacy BuffType
  // mapping) already identified this slot as request.ailment, so recording
  // the same type in the fields is behavior-preserving and upgrades old saves
  // to the fast path on their next refresh.
  effect.managed_ailment = true;
  effect.ailment_type = AilmentTypeToStorage(request.ailment);

  const int mergedStacks =
      std::min(effect.max_stacks, effect.stacks + static_cast<int>(incomingStacks));
  effect.stacks = std::max(1, mergedStacks);
  ApplyOverwrite(effect, contract->overwrite_policy, magnitude);
  effect.ailment_power = effect.tick_damage;
  SetRefresh(effect, contract->refresh_policy, duration);
  SyncAilmentSnapshot(registry, target, effect, *contract, request.source);

  if (matchingIndices.size() > 1) {
    for (size_t i = matchingIndices.size(); i > 1; --i) {
      activeEffects.effects.erase(activeEffects.effects.begin() +
                                  static_cast<std::ptrdiff_t>(
                                      matchingIndices[i - 1]));
    }
  }
  return true;
}

void AilmentTickDriver::Tick(entt::registry &registry, float dt) {
  auto &contracts = AilmentRegistry::Get();
  if (!contracts.EnsureLoaded()) {
    return;
  }

  // P3-2: 回收不再对应任何存活异常的 DoT 快照载体，避免实体句柄泄漏。
  // EffectSystem 已先一步清理过期 BuffEffect，故此处以"载体句柄是否仍被
  // 目标实体的某个 effect 引用"为唯一存活判据。
  {
    auto holderView = registry.view<damage::AilmentSnapshotHolderComponent>();
    std::vector<entt::entity> staleHolders;
    for (auto holder : holderView) {
      const entt::entity holderTarget =
          holderView.get<damage::AilmentSnapshotHolderComponent>(holder).target;
      bool stillReferenced = false;
      if (registry.valid(holderTarget)) {
        if (auto *effects = registry.try_get<ActiveEffectsComponent>(
                holderTarget)) {
          for (const auto &effect : effects->effects) {
            if (effect.snapshot_source == holder) {
              stillReferenced = true;
              break;
            }
          }
        }
      }
      if (!stillReferenced) {
        staleHolders.push_back(holder);
      }
    }
    for (entt::entity holder : staleHolders) {
      if (registry.valid(holder)) {
        registry.destroy(holder);
      }
    }
  }

  auto view = registry.view<ActiveEffectsComponent, HealthComponent, Position>();
  for (auto entity : view) {
    auto &activeEffects = view.get<ActiveEffectsComponent>(entity);
    const auto &position = view.get<Position>(entity);

    for (auto &effect : activeEffects.effects) {
      if (effect.remaining <= 0.0f) {
        continue;
      }

      const auto ailment = AilmentAdapter::TryMapLegacyBuff(effect);
      if (!ailment) {
        continue;
      }
      const AilmentContract *contract = contracts.Find(*ailment);
      // B2-18：damage_tag==Tag::None 的身份型契约（AilmentType::Slow）不产生
      // tick 伤害。注册 Slow 后若有调用方误走 AilmentApplier，必须在此跳过，
      // 避免把减速幅度当成 DoT 底数结算。
      if (!contract || contract->damage_tag == Tag::None ||
          effect.tick_damage <= 0.0f) {
        continue;
      }

      // Structured field is the hot-path check; the string parse remains as a
      // fallback so save files written before the structured-identity change
      // (id string only, managed_ailment=false) keep the same tick behavior.
      const bool managedAilment =
          effect.managed_ailment || AilmentAdapter::IsManagedAilmentId(effect.id);
      const float tickInterval = managedAilment
                                     ? std::max(0.01f, contract->tick_interval)
                                     : std::max(0.01f, effect.tick_interval);
      if (managedAilment) {
        effect.tick_interval = tickInterval;
      }
      effect.tick_timer += dt;

      int tickCount = 0;
      while (effect.tick_timer >= tickInterval && tickCount < 5) {
        effect.tick_timer -= tickInterval;
        tickCount++;

        float tickDamage = effect.tick_damage;
        if (managedAilment &&
            contract->damage_pool_policy == DamagePoolPolicy::PerStack) {
          tickDamage *= static_cast<float>(std::max(1, effect.stacks));
        }
        if (tickDamage <= 0.0f) {
          continue;
        }

        const Tag damageTag = AilmentAdapter::ResolveDamageTag(*ailment, effect);
        DamagePool basePool;
        basePool.Add(damageTag, tickDamage);

        DamageRequest request;
        request.origin = DamageOrigin::AilmentTick;
        request.attacker = effect.source;
        request.defender = entity;
        request.skill_id = 0;
        request.base_pool = basePool;
        request.additional_tags = Tag::DamageOverTime;
        // P3-2: 优先消费施加时冻结的快照载体；caster 已销毁时快照仍可结算。
        request.source_entity = effect.snapshot_source;
        const auto result =
            DamagePipeline::Execute(registry, request, effect.source, false);

        // 153 饮血刃（流云刺节点）：治疗量 = 该次实际流血伤害（结算后扣血值）
        // × skill_mechanics.json 的 lifesteal_ratio（代码内默认值仅在数据缺失时
        // 兜底）。吸血只归属于流云刺（技能1）自身造成的流血：effect.source_skill_id
        // 由施加时写入，只有其等于 1 的流血 tick 才会治疗施加者；血海、回旋、
        // 引导等其他来源施加的流血即使施加者分配了 153 也不回血。
        // 981 逆脉: 锁血禁疗期间禁止任何来源的治疗
        const auto *sourceTrance =
            registry.try_get<PhantomTranceComponent>(effect.source);
        const bool sourceDeathSeal =
            sourceTrance != nullptr && IsDeathSealActive(*sourceTrance);

        if (!sourceDeathSeal && *ailment == AilmentType::Bleed &&
            effect.source_skill_id == 1 &&
            result.damage.total_damage > 0.0f &&
            HasFlowingThrustBloodDrinker(registry, effect.source)) {
          const float lifestealRatio =
              data::SkillMechanicsRegistry::Get().GetFloat(1, 153, "lifesteal_ratio", 1.0f);
          auto *healStats = registry.try_get<CombatStats>(effect.source);
          if (healStats) {
            const float before = healStats->health;
            healStats->health =
                std::min(healStats->max_health, healStats->health + result.damage.total_damage * lifestealRatio);
            const float actualHeal = healStats->health - before;
            if (actualHeal > 0.0f) {
              if (auto *hp = registry.try_get<HealthComponent>(effect.source)) {
                hp->current = healStats->health;
              }
              registry.emplace_or_replace<StatsDirty>(effect.source);
              CombatEventDispatcher::Dispatch(
                  registry, CombatEventFactory::CreateOnHeal(effect.source, effect.source, actualHeal));
            }
          }
        }

        EffectSystem::EmitDamagePopup(registry, {position.x, position.y - 20.0f},
                                      result.damage.total_damage,
                                      result.damage.is_crit,
                                      damageTag);
      }
    }
  }
}

} // namespace NoMoreDay::systems
