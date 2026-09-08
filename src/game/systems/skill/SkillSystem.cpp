#include "game/systems/skill/SkillSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FrameRateUtils.hpp" // Frame-rate independent utilities
#include "game/systems/physics/SpatialGrid.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "game/foundation/components/AIComponent.hpp" // For EnemyTag
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp" // For Position
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "game/foundation/components/PlayerState.hpp" // For DashComponent
#include "game/foundation/components/Projectile.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/CombatTelemetry.hpp"
#include "game/contracts/DamageResolutionHooks.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"
#include "game/systems/skill/AreaFieldDeliverySystem.hpp"
#include "game/systems/skill/BeamChannelDeliverySystem.hpp"
#include "game/systems/skill/MobilityDeliverySystem.hpp"
#include "game/systems/skill/BoomerangDeliverySystem.hpp"
#include "game/systems/skill/OrbitingSentinelDeliverySystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/systems/skill/SkillCastConstraintService.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/SummonCombatBridge.hpp"
#include "game/systems/skill/behaviors/FlowingThrust.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/MindBlade.hpp"
#include "game/systems/skill/behaviors/PhantomFlash.hpp" // Added
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/SwordArray.hpp"
#include "game/foundation/components/TriggerRuleComponent.hpp"
#include "game/systems/skill/ProcEngine.hpp"
#include "game/systems/skill/ShadowDuplicationHook.hpp"
#include "game/systems/skill/SkillSpecializationBaker.hpp"
#include "raymath.h"
#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>


namespace NoMoreDay {

// Static scratch buffers to avoid per-frame allocations in hot paths
// (Replaced by local/thread_local buffers for safety and performance)

namespace {

static std::vector<std::pair<CombatEventType, uint32_t>> s_procHandlerIds;

constexpr uint8_t kMaxTriggerDepth = 2;
constexpr size_t kCastDepthRetention = 4096;
constexpr const char *kDiagTriggerCooldown = "SKILL_GUARD_TRIGGER_CD";
constexpr const char *kDiagTriggerDepth = "SKILL_GUARD_TRIGGER_DEPTH";
constexpr const char *kDiagScopePolicy = "SKILL_GUARD_SCOPE_POLICY";
constexpr const char *kDiagTriggerSkillUnavailable =
    "SKILL_GUARD_TRIGGER_SKILL_UNAVAILABLE";
constexpr const char *kDiagTriggerManaBlocked = "SKILL_GUARD_TRIGGER_MANA";

uint8_t ResolveCurrentQualityTier() {
  auto &qualityManager = render::core::QualityTierManager::Get();
  if (!qualityManager.IsInitialized()) {
    return static_cast<uint8_t>(render::core::QualityTier::Medium);
  }
  return static_cast<uint8_t>(qualityManager.GetTier());
}

Vector2 ResolveEntityWorldPosition(const entt::registry &registry,
                                   entt::entity entity,
                                   Vector2 fallback = {0.0f, 0.0f}) {
  if (!registry.valid(entity) || !registry.all_of<Position>(entity)) {
    return fallback;
  }
  const auto &position = registry.get<Position>(entity);
  return {position.x, position.y};
}

const SpecializedSkill *FindSpecializedSkillContext(
    const ActiveSkillsComponent *active, const uint32_t skill_id) {
  if (!active) {
    return nullptr;
  }
  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id == skill_id) {
      return &spec;
    }
  }
  return nullptr;
}

uint8_t EncodeResistDebuffType(const ResistModel model) {
  switch (model) {
  case ResistModel::TypeA_Penetration:
    return static_cast<uint8_t>(SkillVfxResistDebuffType::TypeA);
  case ResistModel::TypeB_Shred:
    return static_cast<uint8_t>(SkillVfxResistDebuffType::TypeB);
  case ResistModel::TypeC_Exposure:
    return static_cast<uint8_t>(SkillVfxResistDebuffType::TypeC);
  case ResistModel::TypeD_StatToPenetration:
    return static_cast<uint8_t>(SkillVfxResistDebuffType::TypeD);
  case ResistModel::TypeE_CapSuppression:
    return static_cast<uint8_t>(SkillVfxResistDebuffType::TypeE);
  case ResistModel::None:
  default:
    return static_cast<uint8_t>(SkillVfxResistDebuffType::None);
  }
}

uint32_t BuildSkillVfxNodeRoleMask(const entt::registry &registry,
                                   entt::entity caster,
                                   const uint32_t skill_id) {
  if (!registry.valid(caster)) {
    return SkillVfxNodeRoleMask::None;
  }
  const auto *active = registry.try_get<ActiveSkillsComponent>(caster);
  const SpecializedSkill *specialized =
      FindSpecializedSkillContext(active, skill_id);
  if (!specialized) {
    return SkillVfxNodeRoleMask::None;
  }

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, caster, skill_id);
  uint32_t mask = SkillVfxNodeRoleMask::None;
  for (const auto &[node_id, points] : specialized->allocated_points) {
    if (points <= 0) {
      continue;
    }
    const auto *node_contract = SkillRegistry::Get().GetNodeContract(skill_id, node_id);
    if (!node_contract) {
      continue;
    }
    switch (node_contract->role) {
    case SpecNodeRole::Keystone:
      mask |= SkillVfxNodeRoleMask::Keystone;
      break;
    case SpecNodeRole::Trigger:
      mask |= SkillVfxNodeRoleMask::Trigger;
      break;
    case SpecNodeRole::Synergy:
      mask |= SkillVfxNodeRoleMask::Synergy;
      break;
    case SpecNodeRole::Transmuter:
      if (activeTransmuter == 0 || node_id == activeTransmuter) {
        mask |= SkillVfxNodeRoleMask::Transmuter;
      }
      break;
    case SpecNodeRole::Passive:
    default:
      break;
    }
  }
  return mask;
}

uint8_t ResolveSkillVfxElementType(const entt::registry &registry,
                                   entt::entity caster,
                                   const uint32_t skill_id,
                                   const Tag effective_tags) {
  if (!registry.valid(caster)) {
    return SkillSystem::EncodeSkillVfxElementType(effective_tags);
  }

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, caster, skill_id);
  if (activeTransmuter != 0) {
    const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
    if (tree) {
      if (auto it = tree->nodes.find(activeTransmuter); it != tree->nodes.end()) {
        const uint8_t nodeElement =
            SkillSystem::ResolveSkillVfxElementTypeFromTags(
                effective_tags, it->second.add_tags);
        if (nodeElement != static_cast<uint8_t>(SkillVfxElementType::Physical)) {
          return nodeElement;
        }
      }
    }
  }
  return SkillSystem::EncodeSkillVfxElementType(effective_tags);
}

uint8_t ResolveSkillVfxResistDebuffType(const entt::registry &registry,
                                        entt::entity caster,
                                        const uint32_t skill_id) {
  if (!registry.valid(caster)) {
    return static_cast<uint8_t>(SkillVfxResistDebuffType::None);
  }
  const auto *active = registry.try_get<ActiveSkillsComponent>(caster);
  const SpecializedSkill *specialized =
      FindSpecializedSkillContext(active, skill_id);
  if (!specialized) {
    return static_cast<uint8_t>(SkillVfxResistDebuffType::None);
  }

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, caster, skill_id);
  if (activeTransmuter != 0) {
    const auto *node_contract =
        SkillRegistry::Get().GetNodeContract(skill_id, activeTransmuter);
    if (node_contract) {
      const uint8_t encoded = EncodeResistDebuffType(node_contract->resist_model);
      if (encoded != static_cast<uint8_t>(SkillVfxResistDebuffType::None)) {
        return encoded;
      }
    }
  }

  for (const auto &[node_id, points] : specialized->allocated_points) {
    if (points <= 0) {
      continue;
    }
    const auto *node_contract = SkillRegistry::Get().GetNodeContract(skill_id, node_id);
    if (!node_contract) {
      continue;
    }
    const uint8_t encoded = EncodeResistDebuffType(node_contract->resist_model);
    if (encoded != static_cast<uint8_t>(SkillVfxResistDebuffType::None)) {
      return encoded;
    }
  }

  return static_cast<uint8_t>(SkillVfxResistDebuffType::None);
}

SkillExecutionContext BuildSkillVfxContext(entt::registry &registry,
                                           const SkillExecution &exec) {
  SkillExecutionContext context = {};
  context.skill_id = exec.skill_id;
  context.cast_id = exec.cast_id;
  context.is_empowered = exec.is_empowered;
  context.is_shadow_cast =
      registry.valid(exec.owner) && registry.any_of<ShadowCastTag>(exec.owner);
  context.caster = exec.owner;
  context.origin = ResolveEntityWorldPosition(registry, exec.owner, exec.target_pos);
  context.target = exec.target_pos;
  if (context.target.x == 0.0f && context.target.y == 0.0f) {
    context.target = context.origin;
  }
  context.effective_tags =
      (registry.valid(exec.owner))
          ? SkillSystem::GetEffectiveSkillTags(registry, exec.owner, exec.skill_id)
          : Tag::None;
  context.node_role_mask =
      BuildSkillVfxNodeRoleMask(registry, exec.owner, exec.skill_id);
  context.element_type = ResolveSkillVfxElementType(
      registry, exec.owner, exec.skill_id, context.effective_tags);
  context.resist_debuff_type =
      ResolveSkillVfxResistDebuffType(registry, exec.owner, exec.skill_id);
  return context;
}

SkillExecutionContext BuildSkillVfxContextFromEvent(entt::registry &registry,
                                                    entt::entity caster,
                                                    uint32_t skill_id,
                                                    uint64_t cast_id,
                                                    Vector2 target,
                                                    Tag tags = Tag::None) {
  SkillExecutionContext context = {};
  context.skill_id = skill_id;
  context.cast_id = cast_id;
  context.caster = caster;
  context.origin = ResolveEntityWorldPosition(registry, caster, target);
  context.target = target;
  context.effective_tags = (tags != Tag::None)
                               ? tags
                               : (registry.valid(caster)
                                      ? SkillSystem::GetEffectiveSkillTags(
                                            registry, caster, skill_id)
                                      : Tag::None);
  context.node_role_mask = BuildSkillVfxNodeRoleMask(registry, caster, skill_id);
  context.element_type =
      ResolveSkillVfxElementType(registry, caster, skill_id, context.effective_tags);
  context.resist_debuff_type =
      ResolveSkillVfxResistDebuffType(registry, caster, skill_id);
  return context;
}

void EmitSkillVfxEvent(const SkillExecutionContext &context,
                       const SkillVfxEventType type,
                       const float intensity = 1.0f,
                       const uint32_t nodeRoleMask = 0u) {
  if (context.skill_id == 0) {
    return;
  }

  SkillVfxEvent event = {};
  event.skillId = context.skill_id;
  event.castId = context.cast_id;
  event.type = type;
  event.origin = context.origin;
  event.target = context.target;
  event.nodeRoleMask = context.node_role_mask | nodeRoleMask;
  event.qualityTier = ResolveCurrentQualityTier();
  event.intensity = std::clamp(intensity, 0.25f, 3.0f);
  event.elementType = context.element_type;
  event.resistDebuffType = context.resist_debuff_type;
  systems::GPUSkillEffectSystem::Get().SubmitSkillEvent(event);

  if (type == SkillVfxEventType::CastStart) {
    if (HasSkillVfxNodeRole(event.nodeRoleMask, SkillVfxNodeRoleMask::Transmuter)) {
      SkillVfxEvent transmuterEvent = event;
      transmuterEvent.type = SkillVfxEventType::TransmuterSwitch;
      systems::GPUSkillEffectSystem::Get().SubmitSkillEvent(transmuterEvent);
    }
    if (HasSkillVfxNodeRole(event.nodeRoleMask, SkillVfxNodeRoleMask::Keystone)) {
      SkillVfxEvent keystoneEvent = event;
      keystoneEvent.type = SkillVfxEventType::KeystoneActivate;
      systems::GPUSkillEffectSystem::Get().SubmitSkillEvent(keystoneEvent);
    }
  }
}

const SpecializedSkill *FindSpecializedSkill(const ActiveSkillsComponent &active,
                                             uint32_t skill_id,
                                             int preferred_slot) {
  if (preferred_slot >= 0 &&
      preferred_slot < static_cast<int>(active.specialized_slots.size())) {
    const auto &spec = active.specialized_slots[preferred_slot];
    if (spec.skill_id == skill_id) {
      return &spec;
    }
  }
  for (const auto &spec : active.specialized_slots) {
    if (spec.skill_id == skill_id) {
      return &spec;
    }
  }
  return nullptr;
}

int FindSkillSlotById(const ActiveSkillsComponent &active, uint32_t skill_id) {
  for (int i = 0; i < static_cast<int>(active.slots.size()); ++i) {
    if (active.slots[i].id == skill_id) {
      return i;
    }
  }
  return -1;
}

uint32_t ResolveActiveKeystoneByGroup(const SpecializedSkill &specialized,
                                      uint32_t skill_id,
                                      uint8_t exclusion_group) {
  if (exclusion_group == 0) {
    return 0;
  }

  uint32_t selected_node = 0;
  for (const auto &[candidate_node_id, points] : specialized.allocated_points) {
    if (points <= 0) {
      continue;
    }
    const auto *candidate_contract =
        SkillRegistry::Get().GetNodeContract(skill_id, candidate_node_id);
    if (!candidate_contract) {
      continue;
    }
    if (candidate_contract->keystone_exclusion_group != exclusion_group) {
      continue;
    }
    if (selected_node == 0 || candidate_node_id < selected_node) {
      selected_node = candidate_node_id;
    }
  }
  return selected_node;
}

void PopulateActiveNodesFromSpecialized(const SpecializedSkill *specialized,
                                        SkillExecution &exec) {
  if (!specialized) {
    return;
  }
  for (auto const &[node_id, points] : specialized->allocated_points) {
    if (points <= 0) {
      continue;
    }
    const uint32_t bit_idx = node_id % 100;
    if (bit_idx < 128) {
      exec.active_nodes.set(bit_idx);
    }
  }
}

void LogGuardBlocked(const char *code, uint32_t skill_id, uint32_t node_id,
                     entt::entity caster, const char *reason) {
  LOG_WARN("[{}] skill={} node={} caster={} reason={}", code, skill_id, node_id,
           static_cast<uint32_t>(caster), reason ? reason : "");
}

void TickTriggerCooldowns(SkillContractRuntimeComponent &runtime, float dt) {
  for (auto it = runtime.trigger_cooldowns.begin();
       it != runtime.trigger_cooldowns.end();) {
    it->second -= dt;
    if (it->second <= 0.0f) {
      it = runtime.trigger_cooldowns.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace

uint8_t SkillSystem::EncodeSkillVfxElementType(const Tag tags) {
  if (HasTag(tags, Tag::Void)) {
    return static_cast<uint8_t>(SkillVfxElementType::Void);
  }
  if (HasTag(tags, Tag::Lightning)) {
    return static_cast<uint8_t>(SkillVfxElementType::Lightning);
  }
  if (HasTag(tags, Tag::Cold)) {
    return static_cast<uint8_t>(SkillVfxElementType::Cold);
  }
  if (HasTag(tags, Tag::Fire)) {
    return static_cast<uint8_t>(SkillVfxElementType::Fire);
  }
  return static_cast<uint8_t>(SkillVfxElementType::Physical);
}

uint8_t SkillSystem::ResolveSkillVfxElementTypeFromTags(
    const Tag effectiveTags, const Tag transmuterTags) {
  const uint8_t transmuterElement = EncodeSkillVfxElementType(transmuterTags);
  if (transmuterElement !=
      static_cast<uint8_t>(SkillVfxElementType::Physical)) {
    return transmuterElement;
  }
  return EncodeSkillVfxElementType(effectiveTags);
}

struct SkillSystem::CastTrackingContext {
  mutable std::shared_mutex mutex;
  std::unordered_map<uint64_t, uint8_t> cast_depth;
  std::unordered_map<uint64_t, float> cast_trigger_effectiveness;
  std::deque<uint64_t> cast_depth_order;
  std::atomic<uint64_t> next_cast_id{1};
};

SkillSystem::CastTrackingContext &SkillSystem::GetCastTrackingContext() {
  static CastTrackingContext context;
  return context;
}

void SkillSystem::RememberCastDepth(uint64_t cast_id, uint8_t depth,
                                    float trigger_effectiveness) {
  if (cast_id == 0) {
    return;
  }

  auto &tracking = GetCastTrackingContext();
  std::unique_lock<std::shared_mutex> lock(tracking.mutex);
  tracking.cast_depth[cast_id] = depth;
  if (trigger_effectiveness >= 0.0f) {
    tracking.cast_trigger_effectiveness[cast_id] =
        (std::max)(0.0f, trigger_effectiveness);
  } else if (!tracking.cast_trigger_effectiveness.contains(cast_id)) {
    tracking.cast_trigger_effectiveness[cast_id] = 1.0f;
  }

  if (auto existing = std::find(tracking.cast_depth_order.begin(),
                                tracking.cast_depth_order.end(), cast_id);
      existing != tracking.cast_depth_order.end()) {
    tracking.cast_depth_order.erase(existing);
  }
  tracking.cast_depth_order.push_back(cast_id);
  while (tracking.cast_depth_order.size() > kCastDepthRetention) {
    const uint64_t stale = tracking.cast_depth_order.front();
    tracking.cast_depth_order.pop_front();
    tracking.cast_depth.erase(stale);
    tracking.cast_trigger_effectiveness.erase(stale);
  }
}

uint8_t SkillSystem::QueryCastDepth(uint64_t cast_id) {
  if (cast_id == 0) {
    return 0;
  }

  const auto &tracking = GetCastTrackingContext();
  std::shared_lock<std::shared_mutex> lock(tracking.mutex);
  if (auto it = tracking.cast_depth.find(cast_id);
      it != tracking.cast_depth.end()) {
    return it->second;
  }
  return 0;
}

float SkillSystem::QueryTriggerEffectiveness(uint64_t cast_id) {
  if (cast_id == 0) {
    return 1.0f;
  }

  const auto &tracking = GetCastTrackingContext();
  std::shared_lock<std::shared_mutex> lock(tracking.mutex);
  if (auto it = tracking.cast_trigger_effectiveness.find(cast_id);
      it != tracking.cast_trigger_effectiveness.end()) {
    return it->second;
  }
  return 1.0f;
}

uint64_t SkillSystem::NextCastId() {
  return GetCastTrackingContext().next_cast_id.fetch_add(
      1, std::memory_order_relaxed);
}

void SkillSystem::InitHooks() {
  if (s_hooksInitialized) {
    if (s_onSkillHitHandlerId != 0) {
      CombatEventDispatcher::Unregister(CombatEventType::OnSkillHit,
                                        s_onSkillHitHandlerId);
      s_onSkillHitHandlerId = 0;
    }
    if (s_onTakeDamageHandlerId != 0) {
      CombatEventDispatcher::Unregister(CombatEventType::OnTakeDamage,
                                        s_onTakeDamageHandlerId);
      s_onTakeDamageHandlerId = 0;
    }
  }

  LOG_INFO("Initializing Skill Hooks...");
  SkillBehaviorRegistry::Initialize();
  ClearHooks();

  BehaviorInjectionRegistry::Init();

  // 0. Generic Behavior Injection
  AddPreCastHook([](entt::registry &registry, entt::entity execution_ent,
                    SkillExecution &exec) {
    if (exec.active_nodes.none())
      return;
    if (!registry.valid(exec.owner))
      return;

    const auto *tree = SkillRegistry::Get().GetSkillTree(exec.skill_id);
    if (!tree) {
      // Log warning only if strictly needed, otherwise silent return helps
      // robustness for dummy skills
      return;
    }

    for (size_t i = 0; i < exec.active_nodes.size(); ++i) {
      if (exec.active_nodes.test(i)) {
        uint32_t node_id = (exec.skill_id * 100) + (uint32_t)i;
        auto it = tree->nodes.find(node_id);
        if (it != tree->nodes.end()) {
          const auto &node = it->second;
          if (const auto behavior = SkillBehaviorIdFromString(node.behavior_id);
              behavior.has_value() && *behavior != SkillBehaviorId::None) {
            BehaviorInjectionRegistry::Apply(*behavior, registry, exec.owner);
          }
        }
      }
    }
  });

  // 1. Sword Intent & Empowered Logic
  AddPreCastHook([](entt::registry &registry, entt::entity execution_ent,
                    SkillExecution &exec) {
    entt::entity caster = exec.owner;
    if (!registry.valid(caster))
      return;

    if (registry.any_of<ShadowCastTag>(execution_ent))
      return;

    if (systems::BladeResourceService::ShouldAutoEmpowerOnCast(registry, caster) &&
        SkillSystem::ConsumeSwordIntent(
            registry, caster, SkillConstants::DEFAULT_MAX_SWORD_INTENT,
            exec.skill_id)) {
      exec.is_empowered = true;
      LOG_INFO("Skill {} empowered by Sword Intent for entity {}", exec.skill_id,
               static_cast<uint32_t>(caster));

      // Spawn Sword Intent Burst Visual Effect
      if (auto *pos = registry.try_get<Position>(caster)) {
        auto vfxEntity = registry.create();
        registry.emplace<Position>(vfxEntity, *pos);
        registry.emplace<VisualEffect>(
            vfxEntity,
            VisualEffect{
                .type = VisualEffectType::SwordIntentBurst,
                .timer = 0.0f,
                .lifeTime = 0.4f,
                .startScale = 0.2f,
                .endScale = 1.8f,
                .color = NoMoreDay::components::Colors::BLADE_CYAN});
      }
    }
  });

  // 2. Sword Intent Gain on Hit
  s_onSkillHitHandlerId = CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [](entt::registry &registry, const CombatEvent &evt) {
        // evt.source is the actual caster (fixed in DamagePipeline)
        entt::entity caster = evt.source;

        if (!registry.valid(caster)) {
          return;
        }

        // 集中号令 (Node 314)：记录最近一次命中目标 (排除灵剑自身的命中)
        if (evt.skill_id != 3 && evt.summon_source_skill != 3 && registry.valid(evt.target)) {
          if (auto *bf = registry.try_get<BladeFormationComponent>(caster)) {
            if (bf->has_concentrate) {
              bf->last_skill_hit_target = evt.target;
            }
          }
        }

        // ProcEngine unified trigger dispatch on skill hit
        ProcEngine::DispatchEvent(registry, caster, evt);

        if (evt.skill_id != 0) {
          Vector2 impact = ResolveEntityWorldPosition(
              registry, evt.target, ResolveEntityWorldPosition(registry, caster));
          SkillExecutionContext hitContext = BuildSkillVfxContextFromEvent(
              registry, caster, evt.skill_id, evt.castId, impact, evt.tags);
          EmitSkillVfxEvent(hitContext, SkillVfxEventType::TriggerProc,
                            evt.isCrit ? 1.15f : 1.0f);
        }

        BladeResourceComponent *resource = registry.try_get<BladeResourceComponent>(caster);
        SwordIntentComponent *intent = registry.try_get<SwordIntentComponent>(caster);
        // 二次命中（冰爆/连锁等衍生伤害，带 SecondaryHit 标记）不参与命中回资源，
        // 防止单次施放的衍生伤害重复累积剑意/剑流层数。
        if ((resource || intent) && HasTag(evt.tags, Tag::Hit) &&
            !HasTag(evt.tags, Tag::SecondaryHit)) {
          bool gain_stack = false;
          const float current_time = static_cast<float>(GetTime());
          const bool is_continuous =
              HasTag(evt.tags, Tag::Channeled) || HasTag(evt.tags, Tag::Aura);

          // Use cast_id if available, otherwise fallback to skill_id.
          const uint64_t tracking_key =
              (evt.castId != 0) ? evt.castId : static_cast<uint64_t>(evt.skill_id);
          auto &tracking = resource != nullptr ? resource->hit_tracking[tracking_key]
                                              : intent->hit_tracking[tracking_key];

          if (is_continuous) {
            // Continuous Skills: Max 1 stack per second per cast.
            const float time_since_last_gain =
                current_time - tracking.last_gain_time;
            if (time_since_last_gain >= 1.0f) {
              gain_stack = true;
              tracking.last_gain_time = current_time;
              tracking.stacks_gained++;
            }
          } else if (tracking.stacks_gained == 0) {
            // Instant/Hit Skills: One stack per cast.
            gain_stack = true;
            tracking.last_gain_time = current_time;
            tracking.stacks_gained++;
          }

          if (resource != nullptr &&
              resource->kind == BladeResourceKind::SwordFlow &&
              evt.skill_id == 10) {
            gain_stack = false;
          }

          if (gain_stack) {
            SkillSystem::GainSwordIntent(registry, caster, 1, evt.skill_id);
          }

          skills::HeavenlySwordDescent::HandleLinkedHit(registry, evt);
          skills::BloodSea::HandleLinkedHit(registry, evt);

          if (resource != nullptr && resource->kind == BladeResourceKind::Bloodthirst &&
              HasTag(evt.tags, Tag::Melee)) {
            const uint64_t tracking_key =
                (evt.castId != 0) ? evt.castId : static_cast<uint64_t>(evt.skill_id);
            (void)systems::BladeResourceService::TryGainBloodthirstOnLowLifeMeleeHit(
                registry, caster, tracking_key, current_time, evt.skill_id);
          }

          if (evt.isCrit && resource != nullptr &&
              resource->kind == BladeResourceKind::SwordFlow) {
            const float proc_roll =
                static_cast<float>(GetRandomValue(0, 1000)) / 1000.0f;
            if (systems::BladeResourceService::TryGrantSwordFlowCritBonus(
                    registry, caster, evt.skill_id, current_time, proc_roll)) {
              SkillExecutionContext procContext = BuildSkillVfxContextFromEvent(
                  registry, caster, evt.skill_id, evt.castId,
                  ResolveEntityWorldPosition(registry, caster), evt.tags);
              EmitSkillVfxEvent(procContext, SkillVfxEventType::TriggerProc,
                               1.25f);
            }
          }
        }

        // Unified Sword Step linkage: on-hit mana return and crit extension.
        // 二次命中（SecondaryHit）不触发御剑步回蓝，与主命中资源口径保持一致。
        if (HasTag(evt.tags, Tag::Hit) && !HasTag(evt.tags, Tag::SecondaryHit) &&
            registry.any_of<PhaseTag>(caster)) {
          auto *effects = registry.try_get<ActiveEffectsComponent>(caster);
          BuffEffect *swift =
              effects ? effects->Get(BuffId::SwordStep) : nullptr;
          if (swift != nullptr && swift->remaining > 0.0f) {
            if (auto *stats = registry.try_get<CombatStats>(caster)) {
              stats->mana = std::min(stats->max_mana, stats->mana + 1.0f);
              registry.get_or_emplace<StatsDirty>(caster);
            }
            if (evt.isCrit) {
              swift->remaining =
                  std::min(swift->duration + 0.5f, swift->remaining + 0.2f);
            }
          }
        }

        // Contract-driven trigger handling with guard rails.
        if (evt.skill_id != 0) {
          auto *active = registry.try_get<ActiveSkillsComponent>(caster);
          const SpecializedSkill *specialized =
              active ? FindSpecializedSkill(*active, evt.skill_id, -1) : nullptr;
          if (specialized) {
            auto &runtime =
                registry.get_or_emplace<SkillContractRuntimeComponent>(caster);
            runtime.version = kSkillContractRuntimeVersion;

            uint8_t parent_depth = SkillSystem::QueryCastDepth(evt.castId);
            if (evt.castId != 0 && parent_depth == 0) {
              auto exec_view = registry.view<SkillExecution>();
              for (auto exec_entity : exec_view) {
                const auto &exec = exec_view.get<SkillExecution>(exec_entity);
                if (exec.cast_id == evt.castId) {
                  parent_depth = exec.trigger_depth;
                  SkillSystem::RememberCastDepth(evt.castId, parent_depth);
                  break;
                }
              }
            }

            Vector2 trigger_target = {0.0f, 0.0f};
            if (registry.valid(evt.target) && registry.all_of<Position>(evt.target)) {
              const auto &pos = registry.get<Position>(evt.target);
              trigger_target = {pos.x, pos.y};
            } else if (registry.valid(caster) && registry.all_of<Position>(caster)) {
              const auto &pos = registry.get<Position>(caster);
              trigger_target = {pos.x, pos.y};
            }

#if COMBAT_TELEMETRY_ENABLED
            CombatTelemetry &telemetry = CombatTelemetry::Get();
            const bool telemetryEnabled = telemetry.IsRuntimeEnabled();
            auto recordTriggerAttempt = [&](uint8_t depth) {
              if (telemetryEnabled) {
                telemetry.RecordTriggerAttempt(depth);
              }
            };
            auto recordTriggerBlocked = [&](uint8_t depth) {
              if (telemetryEnabled) {
                telemetry.RecordTriggerBlocked(depth);
              }
            };
            auto recordTriggerDispatched = [&](uint8_t depth) {
              if (telemetryEnabled) {
                telemetry.RecordTriggerDispatched(depth);
              }
            };
#endif

            for (const auto &[node_id, points] : specialized->allocated_points) {
              if (points <= 0) {
                continue;
              }
              const auto *node_contract =
                  SkillRegistry::Get().GetNodeContract(evt.skill_id, node_id);
              if (!node_contract ||
                  node_contract->role != SpecNodeRole::Trigger) {
                continue;
              }
#if COMBAT_TELEMETRY_ENABLED
              recordTriggerAttempt(parent_depth);
#endif
              if (!SkillSystem::CanApplyScopePolicy(
                      registry, caster, evt.skill_id, evt.skill_id,
                      node_contract->scope_policy)) {
#if COMBAT_TELEMETRY_ENABLED
                recordTriggerBlocked(parent_depth);
#endif
                LogGuardBlocked(kDiagScopePolicy, evt.skill_id, node_id, caster,
                                "scope policy rejected");
                continue;
              }
              if (runtime.trigger_cooldowns.contains(node_id)) {
#if COMBAT_TELEMETRY_ENABLED
                recordTriggerBlocked(parent_depth);
#endif
                LogGuardBlocked(kDiagTriggerCooldown, evt.skill_id, node_id,
                                caster, "trigger cooldown active");
                continue;
              }
              if (parent_depth >= kMaxTriggerDepth) {
#if COMBAT_TELEMETRY_ENABLED
                recordTriggerBlocked(parent_depth);
#endif
                LogGuardBlocked(kDiagTriggerDepth, evt.skill_id, node_id, caster,
                                "max trigger depth reached");
                continue;
              }
              // Counter window should not recursively dispatch trigger chains.
              if (const auto *pf =
                      registry.try_get<PhantomFlashComponent>(caster)) {
                if (pf->counter_window > 0.0f && !pf->triggered) {
#if COMBAT_TELEMETRY_ENABLED
                  recordTriggerBlocked(parent_depth);
#endif
                  LogGuardBlocked(kDiagTriggerDepth, evt.skill_id, node_id,
                                  caster,
                                  "counter window suppresses trigger chain");
                  continue;
                }
              }

              const uint32_t trigger_skill_id =
                  node_contract->trigger.trigger_skill_id;
              if (trigger_skill_id == 0) {
                continue;
              }
              const auto *trigger_skill =
                  SkillRegistry::Get().GetSkill(trigger_skill_id);
              if (!trigger_skill) {
#if COMBAT_TELEMETRY_ENABLED
                recordTriggerBlocked(parent_depth);
#endif
                LogGuardBlocked(kDiagTriggerSkillUnavailable, evt.skill_id,
                                node_id, caster, "trigger skill not found");
                continue;
              }
              if (!ProcBudgetManager::Get().RequestProc(
                      caster, ProcBudgetType::TriggerProc, 1.0f)) {
#if COMBAT_TELEMETRY_ENABLED
                recordTriggerBlocked(parent_depth);
#endif
                LogGuardBlocked(kDiagTriggerDepth, evt.skill_id, node_id, caster,
                                "proc budget denied");
                continue;
              }
              if (node_contract->trigger.consumes_mana) {
                if (auto *stats = registry.try_get<CombatStats>(caster)) {
                  if (stats->mana < trigger_skill->mana_cost) {
#if COMBAT_TELEMETRY_ENABLED
                    recordTriggerBlocked(parent_depth);
#endif
                    LogGuardBlocked(kDiagTriggerManaBlocked, evt.skill_id,
                                    node_id, caster, "insufficient mana");
                    continue;
                  }
                  stats->mana -= trigger_skill->mana_cost;
                }
              }

              auto trigger_exec_entity = registry.create();
              registry.emplace<LocalLevelTag>(trigger_exec_entity);
              auto &trigger_exec =
                  registry.emplace<SkillExecution>(trigger_exec_entity);
              trigger_exec.skill_id = trigger_skill_id;
              trigger_exec.owner = caster;
              trigger_exec.slot_index =
                  active ? FindSkillSlotById(*active, trigger_skill_id) : -1;
              trigger_exec.target_pos = trigger_target;
              trigger_exec.cast_id = SkillSystem::NextCastId();
              trigger_exec.state = SkillState::Preparing;
              trigger_exec.timer = 0.0f;
              trigger_exec.trigger_depth = static_cast<uint8_t>(parent_depth + 1);
              trigger_exec.trigger_effectiveness =
                  (std::max)(0.0f, node_contract->trigger.effectiveness);
              SkillSystem::RememberCastDepth(trigger_exec.cast_id,
                                             trigger_exec.trigger_depth,
                                             trigger_exec.trigger_effectiveness);
#if COMBAT_TELEMETRY_ENABLED
              recordTriggerDispatched(trigger_exec.trigger_depth);
#endif

              if (active) {
                const SpecializedSkill *trigger_specialized =
                    FindSpecializedSkill(*active, trigger_skill_id, -1);
                PopulateActiveNodesFromSpecialized(trigger_specialized,
                                                   trigger_exec);
              }

              if (node_contract->trigger.internal_cooldown > 0.0f) {
                runtime.trigger_cooldowns[node_id] =
                    node_contract->trigger.internal_cooldown;
              }

              LOG_INFO(
                  "Trigger dispatched: caster={} source_skill={} node={} "
                  "trigger_skill={} depth={}",
                  static_cast<uint32_t>(caster), evt.skill_id, node_id,
                  trigger_skill_id, trigger_exec.trigger_depth);

              const SkillExecutionContext triggerContext =
                  BuildSkillVfxContextFromEvent(registry, caster, evt.skill_id,
                                                evt.castId, trigger_target,
                                                evt.tags);
              EmitSkillVfxEvent(triggerContext, SkillVfxEventType::TriggerProc,
                                1.05f, SkillVfxNodeRoleMask::Trigger);

              const SkillExecutionContext triggeredSkillContext =
                  BuildSkillVfxContextFromEvent(
                      registry, caster, trigger_skill_id, trigger_exec.cast_id,
                      trigger_target);
              EmitSkillVfxEvent(triggeredSkillContext,
                                SkillVfxEventType::CastStart, 0.9f);
            }
          }
        }

        // 触发链防护：仅主命中（trigger_depth==0 且非 SecondaryHit 标记）允许进入
        // 行为层 per-hit 副作用。两类衍生命中均被排除：
        // 1) 触发施法（TriggerCast，depth>=1，如反击/未来触发规则）派生的命中；
        // 2) 冰爆(271)/连锁(273)等直连 ResolveDamage 的二次伤害（depth 恒 0，
        //    由生产侧 additional_tags 携带 SecondaryHit 标记识别）。
        // 目的：阻断烙印/回剑意/传染/引爆等节点的触发链自我放大。
        // ProcEngine 经 OnDealDamage/OnCrit 等独立事件监听（上方仅显式转发），不受影响。
        if (evt.skill_id != 0 && evt.trigger_depth == 0 &&
            !HasTag(evt.tags, Tag::SecondaryHit)) {
          if (auto hitFunc = SkillBehaviorRegistry::GetHit(evt.skill_id)) {
            hitFunc(registry, evt.source, evt.target, evt.tags, evt.isCrit);
          }
        }
        if (evt.summon_source_skill != 0 && evt.summon_source_skill != evt.skill_id &&
            evt.trigger_depth == 0 && !HasTag(evt.tags, Tag::SecondaryHit)) {
          if (auto summonHitFunc = SkillBehaviorRegistry::GetHit(evt.summon_source_skill)) {
            summonHitFunc(registry, evt.source, evt.target, evt.tags, evt.isCrit);
          }
        }
      },
      50);

  // 3. Unified Proc Engine (Defensive / OnTakeDamage)
  s_onTakeDamageHandlerId = CombatEventDispatcher::Register(
      CombatEventType::OnTakeDamage,
      [](entt::registry &registry, const CombatEvent &evt) {
        if (!registry.valid(evt.source))
          return;
        ProcEngine::DispatchEvent(registry, evt.source, evt);
      },
      50);

  // 4. Register generic ProcEngine event listeners for other combat hooks
  constexpr CombatEventType kProcEvents[] = {
      CombatEventType::OnSkillCast,
      CombatEventType::OnDealDamage,
      CombatEventType::OnCrit,
      CombatEventType::OnKill,
      CombatEventType::OnDodge,
      CombatEventType::OnBlock,
  };
  for (const auto evType : kProcEvents) {
    const uint32_t hid = CombatEventDispatcher::Register(
        evType,
        [](entt::registry &registry, const CombatEvent &evt) {
          if (registry.valid(evt.source)) {
            ProcEngine::DispatchEvent(registry, evt.source, evt);
          }
        },
        50);
    s_procHandlerIds.emplace_back(evType, hid);
  }

  s_hooksInitialized = true;
  LOG_INFO("Skill Hooks initialized. Skills are now loaded from "
           "SkillBehaviorRegistry.");
}

void SkillSystem::ShutdownHooks() {
  if (s_onSkillHitHandlerId != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnSkillHit,
                                      s_onSkillHitHandlerId);
    s_onSkillHitHandlerId = 0;
  }
  if (s_onTakeDamageHandlerId != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnTakeDamage,
                                      s_onTakeDamageHandlerId);
    s_onTakeDamageHandlerId = 0;
  }
  for (const auto &[evType, hid] : s_procHandlerIds) {
    CombatEventDispatcher::Unregister(evType, hid);
  }
  s_procHandlerIds.clear();

  ClearHooks();
  s_hooksInitialized = false;
}

void SkillSystem::Update(entt::registry &registry,
                         systems::SpatialHashGrid &grid, float dt,
                         tf::Executor *executor) {
  UpdateCooldowns(registry, dt);
  UpdateStates(registry, dt);
  UpdateSwordIntent(registry, dt);

  // Emit Sword Step lifecycle VFX events from buff state edges.
  static thread_local std::unordered_set<entt::entity> s_prevSwordStepEntities;
  static thread_local std::unordered_set<entt::entity> s_currSwordStepEntities;
  s_currSwordStepEntities.clear();
  constexpr uint32_t kFlowingThrustSkillId = 1u;
  auto effects_view = registry.view<ActiveEffectsComponent>();
  for (auto entity : effects_view) {
    const auto &effects = effects_view.get<ActiveEffectsComponent>(entity);
    const auto *swift = effects.Get(BuffId::SwordStep);
    if (swift == nullptr || swift->remaining <= 0.0f) {
      continue;
    }
    s_currSwordStepEntities.insert(entity);
    if (s_prevSwordStepEntities.contains(entity)) {
      continue;
    }

    const SkillExecutionContext swordStepEnterContext = BuildSkillVfxContextFromEvent(
        registry, entity, kFlowingThrustSkillId, 0u,
        ResolveEntityWorldPosition(registry, entity));
    EmitSkillVfxEvent(swordStepEnterContext, SkillVfxEventType::BuffEnter, 0.85f);
  }
  for (auto entity : s_prevSwordStepEntities) {
    if (s_currSwordStepEntities.contains(entity) || !registry.valid(entity)) {
      continue;
    }
    const SkillExecutionContext swordStepExitContext = BuildSkillVfxContextFromEvent(
        registry, entity, kFlowingThrustSkillId, 0u,
        ResolveEntityWorldPosition(registry, entity));
    EmitSkillVfxEvent(swordStepExitContext, SkillVfxEventType::BuffExit, 0.8f);
  }
  s_prevSwordStepEntities.swap(s_currSwordStepEntities);

  // Keep Sword Step phase state aligned with its owning buff lifecycle.
  static thread_local std::vector<entt::entity> s_phase_to_remove;
  s_phase_to_remove.clear();
  auto phase_view = registry.view<PhaseTag>();
  for (auto entity : phase_view) {
    const auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
    const auto *swift = effects ? effects->Get(BuffId::SwordStep) : nullptr;
    if (swift == nullptr || swift->remaining <= 0.0f) {
      s_phase_to_remove.push_back(entity);
    }
  }
  for (auto entity : s_phase_to_remove) {
    registry.remove<PhaseTag>(entity);
  }

  // Update Blade Formation (ID 3)
  auto formation_view = registry.view<BladeFormationComponent, Position>();
  for (auto entity : formation_view) {
    auto &formation = formation_view.get<BladeFormationComponent>(entity);
    const auto &pos = formation_view.get<Position>(entity);

    // Update current_swords count from actual entities
    int count = 0;
    auto swordView = registry.view<SpiritSwordTag, SummonComponent>();
    for (auto swordEnt : swordView) {
      if (swordView.get<SummonComponent>(swordEnt).owner == entity) {
        count++;
      }
    }
    formation.current_swords = count;

    // 不灭剑魂 (Talent 353) 内置冷却计时
    if (formation.immortality_cooldown > 0.0f) {
      formation.immortality_cooldown -= dt;
      if (formation.immortality_cooldown <= 0.0f && formation.has_immortality) {
        formation.immortality_ready = true;
      }
    }

    // 灵力网络 (Talent 312) 每柄灵剑每秒回蓝
    // 仅由 BladeFormationComponent.mana_regen_per_sword 驱动：DoCast 已在施放时烘焙
    // （含 profile 兜底 3.0f），此处不再每帧 GetFloat 查表，避免热路径字符串堆分配
    if (formation.mana_regen_per_sword > 0.0f && formation.current_swords > 0) {
      if (auto *stats = registry.try_get<CombatStats>(entity)) {
        stats->mana = std::min(stats->max_mana, stats->mana + formation.mana_regen_per_sword * static_cast<float>(formation.current_swords) * dt);
      }
    }
  }

  // Update Sword Array (ID 6)
  auto array_view = registry.view<SwordArrayComponent, Position>();
  for (auto entity : array_view) {
    auto &array = array_view.get<SwordArrayComponent>(entity);
    skills::SwordArray::Update(registry, entity, array, dt, grid);
  }

  auto heavenly_field_view = registry.view<HeavenlySwordFieldComponent, Position>();
  for (auto entity : heavenly_field_view) {
    auto &field = heavenly_field_view.get<HeavenlySwordFieldComponent>(entity);
    skills::HeavenlySwordDescent::UpdateField(registry, entity, field, dt, grid);
  }

  auto blood_sea_view = registry.view<BloodSeaFieldComponent, Position>();
  for (auto entity : blood_sea_view) {
    auto &field = blood_sea_view.get<BloodSeaFieldComponent>(entity);
    skills::BloodSea::UpdateField(registry, entity, field, dt, grid);
  }

  // Update Flowing Thrust Ember Trails (170 劫火余烬带 / 171 业火焚途站位加成)
  skills::UpdateFlowingThrustEmbers(registry, dt);
  // Update Flowing Thrust Phantom Shield (135 虚实相生: 离开残影区触发临时护盾)
  skills::UpdateFlowingThrustPhantomShield(registry, dt);

  // Update Mind Blade (ID 7)
  auto mind_blade_view =
      registry.view<MindBladeComponent, MindBladeAI, Position>();
  static thread_local std::vector<entt::entity> s_mb_to_destroy;
  s_mb_to_destroy.clear();

  for (auto entity : mind_blade_view) {
    auto &mc = mind_blade_view.get<MindBladeComponent>(entity);
    auto &ai = mind_blade_view.get<MindBladeAI>(entity);
    if (!skills::MindBlade::Update(registry, entity, ai, mc, dt, grid)) {
      s_mb_to_destroy.push_back(entity);
    }
  }
  for (auto e : s_mb_to_destroy) {
    registry.destroy(e);
  }

  // Update Area Fields
  AreaFieldDeliverySystem::Update(registry, grid, dt);

  // Update Channeling (ID 5 & 7)
  BeamChannelDeliverySystem::Update(registry, grid, dt);

  // Update Universal Delivery Systems (Phase 2)
  MobilityDeliverySystem::Update(registry, dt);
  BoomerangDeliverySystem::Update(registry, grid, dt);
  OrbitingSentinelDeliverySystem::Update(registry, grid, dt);

  // Update Reactive Ward (Task 2.6)
  static thread_local std::vector<entt::entity> s_ward_finished;
  s_ward_finished.clear();
  auto reactive_ward_view = registry.view<ReactiveWardComponent>();
  for (auto entity : reactive_ward_view) {
    auto &rw = reactive_ward_view.get<ReactiveWardComponent>(entity);
    rw.timer += dt;
    if (rw.timer >= rw.ward_duration) {
      s_ward_finished.push_back(entity);
    }
  }
  for (auto e : s_ward_finished) {
    if (registry.valid(e)) {
      registry.remove<ReactiveWardComponent>(e);
    }
  }

  // Update Blade Ward
  auto ward_view = registry.view<BladeWardComponent>();
  for (auto entity : ward_view) {
    auto &ward = ward_view.get<BladeWardComponent>(entity);

    const auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
    const BuffEffect *wardBuff =
        effects != nullptr ? effects->Get(BuffId::BladeWard) : nullptr;

    if (wardBuff != nullptr) {
      ward.duration = std::max(0.01f, wardBuff->duration);
      ward.remaining = wardBuff->remaining;
    } else {
      // Fallback path for missing buff entry: decay locally and exit.
      ward.remaining -= dt;
    }

    if (ward.remaining <= 0.0f) {
      SkillExecutionContext wardExitContext = BuildSkillVfxContextFromEvent(
          registry, entity, 4u, 0u, ResolveEntityWorldPosition(registry, entity));
      EmitSkillVfxEvent(wardExitContext, SkillVfxEventType::BuffExit, 0.9f);
      registry.remove<BladeWardComponent>(entity);
      continue;
    }

    // Keep the buff refreshed if we want it to stay for the duration
    // Actually, the buff has its own duration in ActiveEffectsComponent.
    // We just need to sync them or let them be independent.
  }

  // Update Phantom Flash
  std::vector<entt::entity> pf_to_remove;
  auto pf_view = registry.view<PhantomFlashComponent>();
  pf_view.each([&](entt::entity entity, PhantomFlashComponent &pf) {
    if (skills::PhantomFlash::Update(registry, entity, pf, dt)) {
      pf_to_remove.push_back(entity);
    }
  });

  for (auto e : pf_to_remove) {
    SkillExecutionContext phantomExitContext = BuildSkillVfxContextFromEvent(
        registry, e, 9u, 0u, ResolveEntityWorldPosition(registry, e));
    EmitSkillVfxEvent(phantomExitContext, SkillVfxEventType::BuffExit, 1.0f);
    if (auto *mods = registry.try_get<SkillModifierComponent>(e)) {
      mods->damage_modifiers.erase(
          std::remove_if(mods->damage_modifiers.begin(),
                         mods->damage_modifiers.end(),
                         [](const DamageModifier &mod) {
                           return mod.type == ModifierType::GainExtra &&
                                  mod.source_tag == Tag::Physical &&
                                  (mod.target_tag == Tag::Cold ||
                                   mod.target_tag == Tag::Lightning);
                         }),
          mods->damage_modifiers.end());
    }
    registry.remove<PhantomFlashComponent>(e);
  }
}

void SkillSystem::AddPreCastHook(SkillHook hook) {
  s_pre_cast_hooks.push_back(hook);
}

void SkillSystem::ClearHooks() {
  s_pre_cast_hooks.clear();
}

float SkillSystem::GetTriggerEffectivenessForCast(uint64_t cast_id) {
  return SkillSystem::QueryTriggerEffectiveness(cast_id);
}

bool SkillSystem::ShadowCast(entt::registry &registry, entt::entity owner,
                             uint32_t skill_id, Vector2 position,
                             Vector2 target_pos,
                             float override_damage_scale) {
  const auto *data = SkillRegistry::Get().GetSkill(skill_id);
  if (!data)
    return false;

  entt::entity shadow = owner;

  if (!registry.any_of<ShadowComponent>(owner) &&
      !registry.any_of<ShadowLifetime>(owner)) {
    shadow = registry.create();
    registry.emplace<LocalLevelTag>(shadow);
    registry.emplace<Position>(shadow, position.x, position.y);
    registry.emplace<Velocity>(shadow, 0.0f,
                               0.0f); // Ensure it has velocity for grid
    registry.emplace<AnimationStateComponent>(shadow);
    registry.emplace<ShadowLifetime>(shadow, 1.0f);

    if (registry.any_of<SpiritSwordTag>(owner)) {
      registry.emplace<SpiritSwordTag>(shadow);
    }

    if (const auto *summon = registry.try_get<SummonComponent>(owner)) {
      SummonAttributionContext context;
      context.owner = summon->owner;
      context.summon = owner;
      context.source_skill_id = summon->skill_id;
      registry.emplace_or_replace<SummonAttributionContext>(shadow, context);
    } else if (const auto *context =
                   registry.try_get<SummonAttributionContext>(owner)) {
      registry.emplace_or_replace<SummonAttributionContext>(shadow, *context);
    }
  }

  auto exec_ent = registry.create();
  registry.emplace<LocalLevelTag>(exec_ent);
  auto &exec = registry.emplace<SkillExecution>(exec_ent);
  exec.skill_id = skill_id;
  exec.owner = shadow;
  exec.state = SkillState::Preparing;
  exec.timer = 0.05f;
  exec.target_pos = target_pos;
  exec.trigger_depth = 0;

  exec.cast_id = SkillSystem::NextCastId();
  SkillSystem::RememberCastDepth(exec.cast_id, exec.trigger_depth);
  EmitSkillVfxEvent(BuildSkillVfxContext(registry, exec),
                    SkillVfxEventType::CastStart, 0.85f);

  // Check if the caller provided a snapshot (either via ShadowComponent or
  // manual call)
  if (auto *sc = registry.try_get<ShadowComponent>(owner)) {
    exec.has_snapshot = true;
    exec.snapshot = sc->snapshot;
    exec.is_empowered = sc->snapshot.is_empowered;
    exec.active_nodes = sc->snapshot.active_nodes;

    // Inherit damage scale from the shadow that cast this (if chaining shadows)
    // Or strictly use the component's value if we want to mutate it.
    // Actually, shadows don't usually cast shadows?
    // If owner is a shadow, 'shadow' variable is 'owner'.
  } else if (auto *stats = registry.try_get<CombatStats>(owner)) {
    // Fallback: Use current owner stats
    exec.has_snapshot = true;
    exec.snapshot.stats = *stats;
    exec.snapshot.skill_id = skill_id;
    DamagePayloadContext ctx{};
    ctx.base_damage_min = stats->min_weapon_damage;
    ctx.base_damage_max = stats->max_weapon_damage;
    ctx.crit_chance = stats->crit_chance;
    ctx.crit_multiplier = stats->crit_damage;
    ctx.increased_damage = 0.0f;
    ctx.more_damage = 1.0f;
    ctx.source_skill_id = skill_id;
    exec.snapshot.payload_context = ctx;
    // No empowerment by default for non-snapshot casts unless we want it?
  }

  // Ensure shadow components are initialized
  if (!registry.all_of<ShadowComponent>(shadow)) {
    auto &sc = registry.get_or_emplace<ShadowComponent>(shadow);
    if (override_damage_scale >= 0.0f) {
      sc.damage_scale = override_damage_scale;
    } else {
      sc.damage_scale = registry.any_of<SpiritSwordTag>(owner) ? 1.0f : 0.3f;
    }

    if (!registry.all_of<ShadowVisualComponent>(shadow)) {
      auto &visual = registry.emplace<ShadowVisualComponent>(shadow);
      visual.color_tint = {40, 0, 60, 180}; // Deep ink purple
    }
  }

  registry.emplace_or_replace<CombatStats>(
      shadow, exec.snapshot.stats); // Ensure stats are on the entity

  if (auto *mods = registry.try_get<SkillModifierComponent>(owner)) {
    registry.emplace_or_replace<SkillModifierComponent>(shadow, *mods);
  } else if (shadow != owner && registry.all_of<SkillModifierComponent>(shadow)) {
    registry.remove<SkillModifierComponent>(shadow);
  }

  registry.emplace<ShadowCastTag>(exec_ent);
  LOG_INFO("Shadow casting skill: {}", data->name_key);
  return true;
}

entt::entity SkillSystem::SpawnShadowEcho(
    entt::registry &registry, entt::entity owner, uint32_t skill_id,
    const Vector2 &startPos, const Vector2 &targetPos, const CombatStats *stats,
    float damageScale, float delay, float lifetime, Color tint,
    const std::bitset<128> &activeNodes)
{
  auto shadow_ent = registry.create();
  registry.emplace<LocalLevelTag>(shadow_ent);
  registry.emplace<Position>(shadow_ent, startPos.x, startPos.y);
  registry.emplace<ShadowVisualComponent>(
      shadow_ent, ShadowVisualComponent{.color_tint = tint, .use_shader = true});
  registry.emplace<DelayedDestroyComponent>(shadow_ent,
                                            DelayedDestroyComponent{lifetime + 0.5f});

  SkillSnapshot snapshot;
  snapshot.skill_id = skill_id;
  snapshot.position = startPos;
  snapshot.target_pos = targetPos;
  snapshot.active_nodes = activeNodes;

  if (stats) {
    snapshot.stats = *stats;
    snapshot.stats.min_weapon_damage *= damageScale;
    snapshot.stats.max_weapon_damage *= damageScale;
    for (auto &val : snapshot.stats.flat_damage) {
      val *= damageScale;
    }
    DamagePayloadContext ctx{};
    ctx.base_damage_min = snapshot.stats.min_weapon_damage;
    ctx.base_damage_max = snapshot.stats.max_weapon_damage;
    ctx.crit_chance = stats->crit_chance;
    ctx.crit_multiplier = stats->crit_damage;
    ctx.increased_damage = 0.0f;
    ctx.more_damage = damageScale;
    ctx.source_skill_id = skill_id;
    snapshot.payload_context = ctx;
  }

  ShadowComponent shadow_comp;
  shadow_comp.snapshot = snapshot;
  shadow_comp.delay = delay;
  shadow_comp.lifetime = lifetime;
  shadow_comp.damage_scale = damageScale;
  registry.emplace<ShadowComponent>(shadow_ent, shadow_comp);

  auto &summon = registry.emplace<SummonComponent>(shadow_ent);
  summon.owner = owner;
  summon.skill_id = skill_id;
  summon.archetype_id = SummonArchetype::ShadowEcho;
  summon.lifetime = lifetime;
  summon.max_lifetime = lifetime;

  return shadow_ent;
}

bool SkillSystem::TriggerCast(entt::registry &registry, entt::entity caster,
                              uint32_t skill_id, entt::entity target_entity,
                              Vector2 target_pos, uint8_t depth,
                              float effectiveness) {
  if (depth > kMaxTriggerDepth) {
    return false;
  }
  if (!registry.valid(caster)) {
    return false;
  }

  const auto *data = SkillRegistry::Get().GetSkill(skill_id);
  if (!data) {
    LOG_WARN("TriggerCast FAILED: Skill ID {} not found", skill_id);
    return false;
  }

  if (target_pos.x == 0.0f && target_pos.y == 0.0f) {
    if (registry.valid(target_entity) && registry.all_of<Position>(target_entity)) {
      const auto &pos = registry.get<Position>(target_entity);
      target_pos = {pos.x, pos.y};
    } else if (registry.all_of<Position>(caster)) {
      const auto &pos = registry.get<Position>(caster);
      target_pos = {pos.x, pos.y};
    }
  }

  auto exec_ent = registry.create();
  registry.emplace<LocalLevelTag>(exec_ent);
  auto &exec = registry.emplace<SkillExecution>(exec_ent);
  exec.skill_id = skill_id;
  exec.owner = caster;
  auto *active = registry.try_get<ActiveSkillsComponent>(caster);
  exec.slot_index = active ? FindSkillSlotById(*active, skill_id) : -1;
  exec.target_pos = target_pos;
  exec.cast_id = SkillSystem::NextCastId();
  exec.state = SkillState::Preparing;
  exec.timer = 0.0f;
  exec.trigger_depth = depth;
  exec.trigger_effectiveness = std::max(0.0f, effectiveness);
  SkillSystem::RememberCastDepth(exec.cast_id, exec.trigger_depth,
                                 exec.trigger_effectiveness);

  if (active) {
    const SpecializedSkill *specialized =
        FindSpecializedSkillContext(active, skill_id);
    if (specialized) {
      for (const auto &[node_id, points] : specialized->allocated_points) {
        if (points > 0 && node_id < 128) {
          exec.active_nodes.set(node_id);
        }
      }
    }
  }

  LOG_INFO("TriggerCast dispatched: caster={} skill_id={} depth={} eff={:.2f}",
           static_cast<uint32_t>(caster), skill_id, depth, effectiveness);
  return true;
}

void SkillSystem::UpdateSwordIntent(entt::registry &registry, float dt) {
  systems::BladeResourceService::Update(registry, dt);

  auto view = registry.view<SwordIntentComponent>(entt::exclude<BladeResourceComponent>);
  for (auto entity : view) {
    auto &intent = view.get<SwordIntentComponent>(entity);

    // 1. Passive Gain - REMOVED per design change
    // Previously gained 1 stack/sec. Now only skill hits gain stacks.
    intent.passive_timer = 0.0f;

    // 2. Decay Logic
    if (intent.stacks > 0) {
      intent.time_since_last_gain += dt;

      if (intent.time_since_last_gain >= intent.grace_period) {
        // New Design: Clear ALL stacks after grace period (default 5s)
        intent.stacks = 0;
        intent.time_since_last_gain = 0.0f;
        registry.get_or_emplace<StatsDirty>(entity); // NEW: Notify stats system
        LOG_INFO("Entity {} Sword Intent cleared (Inactive for {:.1f}s)",
                 (uint32_t)entity, intent.grace_period);
      }

      // Visuals
      if (IsWindowReady() && registry.all_of<Position>(entity)) {
        const auto &pos = registry.get<Position>(entity);
        if (utils::FrameRateUtils::ShouldTrigger(
                static_cast<float>(intent.stacks * 3), dt)) {
          components::GPUParticle p;
          p.position = {pos.x + GetRandomValue(-15, 15),
                        pos.y + GetRandomValue(-30, 0)};
          p.velocity = {0, -30.0f};
          p.acceleration = {0, 0};
          p.color = ColorAlpha(WHITE, 0.4f);
          p.lifetime = 0.5f;
          p.maxLifetime = 0.5f;
          p.scale = 1.0f + (intent.stacks * 0.1f);
          p.flags = 2; // Spark
          systems::GPUParticleSystem::Get().Emit(p);
        }
      }
    } else {
      intent.time_since_last_gain = 0.0f;
    }

    // Clean up old hit tracking entries to prevent memory leak
    for (auto it = intent.hit_tracking.begin();
         it != intent.hit_tracking.end();) {
      if (it->second.last_gain_time < (float)GetTime() - 10.0f) {
        it = intent.hit_tracking.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void SkillSystem::UpdateCooldowns(entt::registry &registry, float dt) {
  ProcEngine::UpdateCooldowns(registry, dt);
  auto view = registry.view<ActiveSkillsComponent>();
  for (auto entity : view) {
    auto &active = view.get<ActiveSkillsComponent>(entity);
    if (auto *runtime = registry.try_get<SkillContractRuntimeComponent>(entity)) {
      TickTriggerCooldowns(*runtime, dt);
    }
    for (auto &slot : active.slots) {
      if (slot.id == 0)
        continue;

      const auto *data = SkillRegistry::Get().GetSkill(slot.id);
      if (!data)
        continue;

      const auto *bakedProfile = GetBakedSkillProfile(registry, entity, slot.id);
      const int maxCharges = (bakedProfile && bakedProfile->effective_charges > 0)
                                 ? bakedProfile->effective_charges
                                 : data->max_charges;

      if (slot.current_charges < maxCharges) {
        // Paused Cooldown Logic: If channeling THIS skill, do not reduce
        // cooldown. This ensures the cooldown effectively starts AFTER
        // channeling (or duration is added).
        bool isChannelingThis = false;
        if (auto *chan = registry.try_get<ChannelingComponent>(entity)) {
          if (chan->skill_id == slot.id) {
            isChannelingThis = true;
          }
        }

        if (!isChannelingThis) {
          slot.cooldown -= dt;
          if (slot.cooldown <= 0.0f) {
            slot.current_charges++;
            if (slot.current_charges < maxCharges) {
              auto *stats = registry.try_get<CombatStats>(entity);
              float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
              float cdr = StatsSystem::GetStatWithTags(
                              registry, entity, StatType::CooldownReduction,
                              data->tags, slot.id) /
                          100.0f;
              float raw_cooldown =
                  bakedProfile ? bakedProfile->effective_cooldown : data->cooldown;
              slot.cooldown =
                  (raw_cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
            } else {
              slot.cooldown = 0.0f;
            }
          }
        }
      }
    }
  }
}

void SkillSystem::UpdateStates(entt::registry &registry, float dt) {
  static thread_local std::vector<entt::entity> s_to_remove;
  s_to_remove.clear();

  auto view = registry.view<SkillExecution>();
  view.each([&](entt::entity entity, SkillExecution &exec) {
    if (!registry.valid(entity))
      return;

    exec.timer -= dt;

    if (exec.timer <= 0.0f) {
      switch (exec.state) {
      case SkillState::Preparing:
        for (auto &hook : s_pre_cast_hooks) {
          if (registry.valid(entity) &&
              registry.all_of<SkillExecution>(entity)) {
            // Re-fetch in case hook caused reallocation
            hook(registry, entity, registry.get<SkillExecution>(entity));
          }
        }
        
        {
          // Re-fetch again after all hooks
          auto& current_exec = registry.get<SkillExecution>(entity);
          current_exec.state = SkillState::Casting;
          current_exec.timer = 0.05f;

          const SkillExecutionContext vfxContext =
              BuildSkillVfxContext(registry, current_exec);
          EmitSkillVfxEvent(vfxContext, SkillVfxEventType::CastImpact,
                            current_exec.is_empowered ? 1.2f : 1.0f);
          if (current_exec.skill_id == 3 || current_exec.skill_id == 4 ||
              current_exec.skill_id == 6 || current_exec.skill_id == 9) {
            EmitSkillVfxEvent(vfxContext, SkillVfxEventType::BuffEnter, 1.0f);
          }

          LOG_INFO("UpdateStates: Executing skill ID {} for entity {}",
                   current_exec.skill_id, (uint32_t)current_exec.owner);

          if (auto castFunc = SkillBehaviorRegistry::GetCast(current_exec.skill_id)) {
            castFunc(registry, current_exec.owner, current_exec);
          } else {
            LOG_WARN("UpdateStates: No callback found for skill ID {} on entity {}",
                     current_exec.skill_id, (uint32_t)entity);
          }

          // Node 354 法术共鸣 (Spell Echo): 当你施放任意法术时，所有灵剑同步发射微型剑气 (20% 效力)
          if (current_exec.skill_id != 3 && registry.valid(current_exec.owner) &&
              !registry.any_of<ShadowComponent>(current_exec.owner)) {
            if (const auto *bf = registry.try_get<BladeFormationComponent>(current_exec.owner)) {
              if (bf->has_spell_echo) {
                auto swordView = registry.view<SpiritSwordTag, SummonComponent, Position>();
                for (auto sEnt : swordView) {
                  if (swordView.get<SummonComponent>(sEnt).owner == current_exec.owner) {
                    const auto &sPos = swordView.get<Position>(sEnt);
                    systems::SummonCombatBridge::CastSpiritSwordEcho(
                        registry, sEnt, current_exec.target_pos, {sPos.x, sPos.y});
                  }
                }
              }
            }
          }
        }
        break;

      case SkillState::Casting:
        exec.state = SkillState::Settle;
        exec.timer = 0.1f;
        break;

      case SkillState::Settle:
        if (auto *anim = registry.try_get<AnimationStateComponent>(entity)) {
          anim->state = EntityAnimState::Idle;
        }
        s_to_remove.push_back(entity);
        return;

      default:
        s_to_remove.push_back(entity);
        return;
      }
    }

    if (auto *anim = registry.try_get<AnimationStateComponent>(entity)) {
      switch (exec.state) {
      case SkillState::Preparing:
        anim->state = EntityAnimState::SkillWindup;
        break;
      case SkillState::Casting:
        anim->state = EntityAnimState::SkillCasting;
        break;
      case SkillState::Settle:
        anim->state = EntityAnimState::SkillRecovery;
        break;
      default:
        break;
      }
      anim->state_timer = exec.timer;
    }
  });

  if (!s_to_remove.empty()) {
    for (const auto ent : s_to_remove) {
      if (registry.valid(ent)) {
        if (const auto *ex = registry.try_get<SkillExecution>(ent)) {
          if (ex->owner != ent) {
            registry.destroy(ent);
            continue;
          }
        }
        registry.remove<SkillExecution>(ent);
      }
    }
  }
}

bool SkillSystem::TryCast(entt::registry &registry, entt::entity entity,
                          int slot_index, Vector2 target_pos) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active || slot_index < 0 || slot_index >= (int)active->slots.size())
    return false;

  auto &slot = active->slots[slot_index];
  if (slot.id == 0) {
    LOG_WARN("TryCast FAILED: No skill in slot {}", slot_index);
    return false;
  }

  if (registry.any_of<SkillExecution>(entity)) {
    LOG_TRACE("TryCast: Entity {} is already executing a skill",
              (uint32_t)entity);
    return false;
  }

  const auto *data = SkillRegistry::Get().GetSkill(slot.id);
  if (!data) {
    LOG_ERROR("TryCast FAILED: Skill ID {} data not found", slot.id);
    return false;
  }
  if ((slot.id == 10 || slot.id == 11 || slot.id == 12) &&
      !systems::BladeMasteryService::IsSignatureSkillUnlocked(registry, entity,
                                                              slot.id)) {
    LOG_WARN("TryCast FAILED: Signature skill {} is locked", slot.id);
    return false;
  }

  const SpecializedSkill *specialized =
      FindSpecializedSkill(*active, slot.id, slot_index);
  const auto *skill_contract = SkillRegistry::Get().GetSkillContract(slot.id);
  static thread_local std::vector<uint32_t> s_allocated_transmuters;
  static thread_local std::vector<uint32_t> s_allocated_triggers;
  if (!skill::ValidateContractCastConstraints(
          SkillRegistry::Get(), skill_contract, specialized, slot.id,
          &s_allocated_transmuters, &s_allocated_triggers)) {
    return false;
  }

  const auto *bakedProfile = GetBakedSkillProfile(registry, entity, slot.id);
  const int maxCharges = (bakedProfile && bakedProfile->effective_charges > 0)
                             ? bakedProfile->effective_charges
                             : data->max_charges;

  if (slot.current_charges <= 0) {
    LOG_TRACE("TryCast: Skill {} has no charges ({} / {})", data->name_key,
              slot.current_charges, maxCharges);
    return false;
  }

  auto *stats = registry.try_get<CombatStats>(entity);
  float rcr = stats ? StatsSystem::GetStatWithTags(
                          registry, entity, StatType::ResourceCostReduction,
                          data->tags, slot.id) /
                          100.0f
                    : 0.0f;
  float raw_mana_cost = bakedProfile ? bakedProfile->effective_mana_cost : data->mana_cost;
  float base_cost = raw_mana_cost * (1.0f - std::min(0.9f, rcr));

  const auto shadowHook = CheckPreCastShadowDuplication(registry, entity, data, base_cost, stats);

  if (stats) {
    float total_cost = base_cost + shadowHook.extra_cost;

    const bool demonBladeLifeSpend =
        total_cost > 0.0f &&
        systems::BladeResourceService::IsDemonBladeActive(registry, entity);
    if (demonBladeLifeSpend) {
      if (!systems::BladeResourceService::TrySpendLifeForDemonBladeCast(
              registry, entity, total_cost, slot.id)) {
        return false;
      }
    } else {
      if (stats->mana < total_cost)
        return false;
      stats->mana -= total_cost;
    }
  }

  if (shadowHook.duplicate) {
    ExecutePreCastShadowDuplication(registry, entity, slot.id, target_pos, stats);
  }

  if (slot.current_charges >= maxCharges) {
    float cdr = StatsSystem::GetStatWithTags(registry, entity,
                                             StatType::CooldownReduction,
                                             data->tags, slot.id) /
                100.0f;
    float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
    // Optimization: For Channeled skills with very long cooldowns (like 60s),
    // we might NOT want to start cooldown here but when channeling ends?
    // But preventing abuse is safer.
    float raw_cooldown = bakedProfile ? bakedProfile->effective_cooldown : data->cooldown;
    slot.cooldown = (raw_cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
  }
  slot.current_charges--;

  const uint64_t cast_id = SkillSystem::NextCastId();

  auto &exec = registry.emplace<SkillExecution>(entity);
  exec.skill_id = slot.id;
  exec.owner = entity;
  exec.cast_id = cast_id;
  exec.slot_index = slot_index;
  exec.state = SkillState::Preparing;
  exec.timer = 0.1f;
  exec.target_pos = target_pos;
  exec.trigger_depth = 0;
  SkillSystem::RememberCastDepth(exec.cast_id, exec.trigger_depth);

  // Populate active_nodes from Specialization
  PopulateActiveNodesFromSpecialized(specialized, exec);

  if (specialized != nullptr) {
    const auto nodeIds =
        SkillSpecModifierAdapter::CollectAllocatedNodeIds(*specialized);
    const Tag skillTags = (data != nullptr) ? data->tags : Tag::None;
    if (SkillSpecModifierAdapter::EvaluateDamageMultiplier(slot.id, skillTags,
                                                           nodeIds) >
            1.0f &&
        stats != nullptr) {
      exec.has_snapshot = true;
      exec.snapshot.stats = *stats;
      SkillSpecModifierAdapter::ApplyHeavyMomentumToDamageMultipliers(
          exec.snapshot.stats.damage_multipliers, slot.id, skillTags, nodeIds);
      DamagePayloadContext ctx{};
      ctx.base_damage_min = stats->min_weapon_damage;
      ctx.base_damage_max = stats->max_weapon_damage;
      ctx.crit_chance = stats->crit_chance;
      ctx.crit_multiplier = stats->crit_damage;
      ctx.increased_damage = 0.0f;
      ctx.more_damage = exec.snapshot.stats.damage_multipliers[0];
      ctx.effective_tags = skillTags;
      ctx.source_skill_id = slot.id;
      exec.snapshot.payload_context = ctx;
    }
  }

  if (specialized && skill_contract) {
    auto &runtime = registry.get_or_emplace<SkillContractRuntimeComponent>(entity);
    runtime.version = kSkillContractRuntimeVersion;
    if (!s_allocated_transmuters.empty()) {
      runtime.active_transmuter_node_by_skill[slot.id] =
          s_allocated_transmuters.front();
    } else {
      runtime.active_transmuter_node_by_skill.erase(slot.id);
    }
  }

  const SkillExecutionContext castStartContext = BuildSkillVfxContext(registry, exec);
  EmitSkillVfxEvent(castStartContext, SkillVfxEventType::CastStart,
                    exec.is_empowered ? 1.15f : 1.0f);

  LOG_INFO("TryCast SUCCESS: Entity {} casting skill ID {} ({})",
           (uint32_t)entity, slot.id, data->name_key);
  return true;
}

void SkillSystem::HandleSkillInput(entt::registry &registry,
                                   entt::entity entity, int slot_index,
                                   Vector2 target_pos) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active || slot_index < 0 || slot_index >= (int)active->slots.size())
    return;

  auto &slot = active->slots[slot_index];
  if (slot.id == 0)
    return;

  // 1. Maintain Channeling
  if (auto *chan = registry.try_get<ChannelingComponent>(entity)) {
    if (chan->skill_id == slot.id) {
      chan->channel_timer = 0.25f; // Keep alive
      chan->target_pos = target_pos;
      // Maybe handle ticking here if we want instant feedback?
      // No, update loop handles it.
      return;
    }
    // If channeling something else, we ignore input (or we could interrupt)
    return;
  }

  // 2. Start New Cast
  TryCast(registry, entity, slot_index, target_pos);
}

bool SkillSystem::AddTalentPoint(entt::registry &registry, entt::entity entity,
                                 uint32_t skill_id, uint32_t node_id) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return false;

  SpecializedSkill *specialized = nullptr;
  for (auto &slot : active->specialized_slots) {
    if (slot.skill_id == skill_id) {
      specialized = &slot;
      break;
    }
  }
  if (!specialized) {
    LOG_WARN(
        "Cannot add talent point: Skill {} is not specialized for entity {}",
        skill_id, (uint32_t)entity);
    return false;
  }

  const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
  if (!tree)
    return false;

  auto node_it = tree->nodes.find(node_id);
  if (node_it == tree->nodes.end())
    return false;
  const auto &node = node_it->second;

  int current_pts = specialized->allocated_points.contains(node_id)
                        ? specialized->allocated_points.at(node_id)
                        : 0;
  if (current_pts >= node.max_points) {
    LOG_WARN("Cannot add talent point: Node {} already at max ({}/{})", node_id,
             current_pts, node.max_points);
    return false;
  }

  bool has_valid_prereq = false;
  bool prereq_satisfied = node.prerequisites.empty();
  for (const auto &pre_req : node.prerequisites) {
    const uint32_t pre_id = pre_req.node_id;
    if (pre_id == 0 || !tree->nodes.contains(pre_id)) {
      continue;
    }
    has_valid_prereq = true;
    int pre_pts = specialized->allocated_points.contains(pre_id)
                      ? specialized->allocated_points.at(pre_id)
                      : 0;
    const int required_points =
        (pre_req.required_points > 0) ? pre_req.required_points : 1;
    if (pre_pts >= required_points) {
      prereq_satisfied = true;
      break;
    }
  }
  if (!prereq_satisfied && has_valid_prereq) {
    LOG_WARN("Cannot add talent point: no prerequisite met for node {}", node_id);
    return false;
  }

  const NodeContractData *node_contract =
      SkillRegistry::Get().GetNodeContract(skill_id, node_id);
  const uint8_t exclusion_group =
      node_contract ? node_contract->keystone_exclusion_group : 0u;

  static thread_local std::vector<uint32_t> s_excluded_nodes;
  s_excluded_nodes.clear();
  int refunded_points_from_exclusion = 0;
  if (exclusion_group != 0) {
    for (const auto &[other_node_id, other_points] :
         specialized->allocated_points) {
      if (other_node_id == node_id || other_points <= 0) {
        continue;
      }
      const auto *other_contract =
          SkillRegistry::Get().GetNodeContract(skill_id, other_node_id);
      if (!other_contract) {
        continue;
      }
      if (other_contract->keystone_exclusion_group != exclusion_group) {
        continue;
      }
      refunded_points_from_exclusion += other_points;
      s_excluded_nodes.push_back(other_node_id);
    }
  }

  if (active->available_talent_points + refunded_points_from_exclusion <= 0) {
    LOG_WARN("Cannot add talent point: No points available for entity {}",
             (uint32_t)entity);
    return false;
  }

  const int projected_spent = specialized->GetPointsSpent() -
                              refunded_points_from_exclusion + 1;
  if (projected_spent > specialized->GetMaxPoints()) {
    LOG_WARN("Cannot add talent point: Skill {} has reached max points ({}/{})",
             skill_id, specialized->GetPointsSpent(),
             specialized->GetMaxPoints());
    return false;
  }

  if (!s_excluded_nodes.empty()) {
    for (const uint32_t excluded_node_id : s_excluded_nodes) {
      auto it = specialized->allocated_points.find(excluded_node_id);
      if (it == specialized->allocated_points.end()) {
        continue;
      }
      active->available_talent_points += it->second;
      specialized->allocated_points.erase(it);
      if (auto *runtime =
              registry.try_get<SkillContractRuntimeComponent>(entity)) {
        runtime->trigger_cooldowns.erase(excluded_node_id);
      }
    }
    LOG_INFO(
        "Entity {} anti-meta exclusion applied for skill {} group {} replacing "
        "node {}",
        static_cast<uint32_t>(entity), skill_id,
        static_cast<uint32_t>(exclusion_group), node_id);
  }

  active->available_talent_points--;
  specialized->allocated_points[node_id] = current_pts + 1;
  registry.get_or_emplace<StatsDirty>(entity);

  LOG_INFO("Entity {} spent talent point on Skill {} -> Node {} ({}/{})",
           (uint32_t)entity, skill_id, node_id,
           specialized->allocated_points[node_id], node.max_points);

  return true;
}

bool SkillSystem::ResetTalents(entt::registry &registry, entt::entity entity,
                               uint32_t skill_id) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return false;

  SpecializedSkill *specialized = nullptr;
  for (auto &slot : active->specialized_slots) {
    if (slot.skill_id == skill_id) {
      specialized = &slot;
      break;
    }
  }
  if (!specialized)
    return false;

  int points_to_refund = 0;
  static thread_local std::vector<uint32_t> s_reset_node_ids;
  s_reset_node_ids.clear();
  for (auto [node_id, pts] : specialized->allocated_points) {
    points_to_refund += pts;
    if (pts > 0) {
      s_reset_node_ids.push_back(node_id);
    }
  }

  active->available_talent_points += points_to_refund;
  specialized->allocated_points.clear();
  if (auto *runtime = registry.try_get<SkillContractRuntimeComponent>(entity)) {
    runtime->active_transmuter_node_by_skill.erase(skill_id);
    for (const uint32_t node_id : s_reset_node_ids) {
      runtime->trigger_cooldowns.erase(node_id);
    }
  }

  registry.get_or_emplace<StatsDirty>(entity);
  LOG_INFO("Entity {} reset talents for Skill {}. Refunded {} points.",
           (uint32_t)entity, skill_id, points_to_refund);

  return true;
}

bool SkillSystem::ClearAllTalents(entt::registry &registry,
                                  entt::entity entity) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return false;

  int total_refunded = 0;
  for (auto &slot : active->specialized_slots) {
    if (slot.skill_id == INVALID_SKILL_ID)
      continue;

    for (auto [node_id, pts] : slot.allocated_points) {
      total_refunded += pts;
    }
    slot.allocated_points.clear();
  }

  active->available_talent_points += total_refunded;
  if (auto *runtime = registry.try_get<SkillContractRuntimeComponent>(entity)) {
    runtime->active_transmuter_node_by_skill.clear();
    runtime->trigger_cooldowns.clear();
  }
  registry.get_or_emplace<StatsDirty>(entity);
  LOG_INFO("Entity {} cleared all talents. Refunded {} points.",
           (uint32_t)entity, total_refunded);

  return true;
}

Tag SkillSystem::GetEffectiveSkillTags(entt::registry &registry,
                                       entt::entity entity, uint32_t skill_id) {
  // Start with base tags from skill definition
  const auto *skill = SkillRegistry::Get().GetSkill(skill_id);
  if (!skill)
    return Tag::None;

  Tag tags = skill->tags;

  // Apply talent modifications
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return tags;

  // Find the specialized slot for this skill
  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id != skill_id)
      continue;

    const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
    if (!tree)
      break;

    const auto *contract = SkillRegistry::Get().GetSkillContract(skill_id);
    const auto *runtime =
        registry.try_get<SkillContractRuntimeComponent>(entity);

    uint32_t selected_transmuter = 0;
    if (runtime) {
      const auto it = runtime->active_transmuter_node_by_skill.find(skill_id);
      if (it != runtime->active_transmuter_node_by_skill.end()) {
        selected_transmuter = it->second;
      }
    }
    if (selected_transmuter == 0 && contract) {
      for (const uint32_t preferred : contract->transmuter_node_ids) {
        if (preferred == 0) {
          continue;
        }
        auto it = spec.allocated_points.find(preferred);
        if (it != spec.allocated_points.end() && it->second > 0) {
          selected_transmuter = preferred;
          break;
        }
      }
    }
    if (selected_transmuter == 0) {
      for (const auto &[node_id, points] : spec.allocated_points) {
        if (points <= 0) {
          continue;
        }
        const auto *node_contract =
            SkillRegistry::Get().GetNodeContract(skill_id, node_id);
        if (!node_contract || node_contract->role != SpecNodeRole::Transmuter) {
          continue;
        }
        if (selected_transmuter == 0 || node_id < selected_transmuter) {
          selected_transmuter = node_id;
        }
      }
    }

    for (const auto &[node_id, points] : spec.allocated_points) {
      if (points <= 0)
        continue;

      const auto *node_contract =
          SkillRegistry::Get().GetNodeContract(skill_id, node_id);
      if (node_contract && node_contract->role == SpecNodeRole::Transmuter &&
          selected_transmuter != 0 && node_id != selected_transmuter) {
        continue;
      }
      if (node_contract && node_contract->keystone_exclusion_group != 0) {
        const uint32_t selected_keystone = ResolveActiveKeystoneByGroup(
            spec, skill_id, node_contract->keystone_exclusion_group);
        if (selected_keystone != 0 && selected_keystone != node_id) {
          continue;
        }
      }

      auto it = tree->nodes.find(node_id);
      if (it == tree->nodes.end())
        continue;

      const auto &node = it->second;

      // Add tags from this talent
      tags = tags | node.add_tags;

      // Remove tags from this talent (using bitwise AND with NOT)
      tags = tags & ~node.remove_tags;
    }
    break;
  }

  return tags;
}

uint32_t SkillSystem::GetActiveTransmuterNode(const entt::registry &registry,
                                              entt::entity entity,
                                              uint32_t skill_id) {
  const auto *runtime = registry.try_get<SkillContractRuntimeComponent>(entity);
  if (!runtime) {
    return 0;
  }
  auto it = runtime->active_transmuter_node_by_skill.find(skill_id);
  if (it == runtime->active_transmuter_node_by_skill.end()) {
    return 0;
  }
  return it->second;
}

bool SkillSystem::NodeAffectsSwordIntent(const entt::registry &,
                                         uint32_t skill_id, uint32_t node_id) {
  const auto *node_contract =
      SkillRegistry::Get().GetNodeContract(skill_id, node_id);
  return node_contract ? node_contract->affects_sword_intent : false;
}

bool SkillSystem::NodeAffectsSwordStep(const entt::registry &, uint32_t skill_id,
                                       uint32_t node_id) {
  const auto *node_contract =
      SkillRegistry::Get().GetNodeContract(skill_id, node_id);
  return node_contract ? node_contract->affects_sword_step : false;
}

bool SkillSystem::CanApplyScopePolicy(const entt::registry &registry,
                                      entt::entity entity,
                                      uint32_t context_skill_id,
                                      uint32_t source_skill_id,
                                      ScopePolicy scope) {
  switch (scope) {
  case ScopePolicy::SkillOnly:
    return context_skill_id != 0 && context_skill_id == source_skill_id;
  case ScopePolicy::GlobalAlways:
    return true;
  case ScopePolicy::GlobalWhileBuffActive:
    if (const auto *chan = registry.try_get<ChannelingComponent>(entity)) {
      if (chan->skill_id == source_skill_id) {
        return true;
      }
    }
    if (source_skill_id == 9) {
      if (const auto *pf = registry.try_get<PhantomFlashComponent>(entity)) {
        return pf->counter_window > 0.0f && !pf->triggered;
      }
    }
    return false;
  default:
    return false;
  }
}

bool SkillSystem::IsNodeExcludedByMutualKeystone(
    const entt::registry &registry, entt::entity entity, uint32_t skill_id,
    uint32_t node_id) {
  if (!registry.valid(entity)) {
    return false;
  }
  const auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active) {
    return false;
  }

  const SpecializedSkill *specialized = nullptr;
  for (const auto &slot : active->specialized_slots) {
    if (slot.skill_id == skill_id) {
      specialized = &slot;
      break;
    }
  }
  if (!specialized) {
    return false;
  }

  const auto *node_contract =
      SkillRegistry::Get().GetNodeContract(skill_id, node_id);
  if (!node_contract || node_contract->keystone_exclusion_group == 0) {
    return false;
  }

  const uint32_t selected_node =
      ResolveActiveKeystoneByGroup(*specialized, skill_id,
                                   node_contract->keystone_exclusion_group);
  return selected_node != 0 && selected_node != node_id;
}

bool SkillSystem::GainSwordIntent(entt::registry &registry, entt::entity entity,
                                  int amount, uint32_t source_skill_id) {
  if (amount <= 0) {
    return false;
  }
  if (registry.all_of<BladeResourceComponent>(entity)) {
    return systems::BladeResourceService::Gain(registry, entity, amount,
                                               source_skill_id);
  }
  auto *intent = registry.try_get<SwordIntentComponent>(entity);
  if (!intent) {
    return false;
  }
  const int before = intent->stacks;
  intent->stacks = std::min(intent->max_stacks, intent->stacks + amount);
  if (intent->stacks == before) {
    return false;
  }
  intent->time_since_last_gain = 0.0f;
  intent->decay_tick_timer = 0.0f;
  registry.get_or_emplace<StatsDirty>(entity);
  LOG_INFO("SwordIntent gain: entity={} skill={} delta={} stacks={}/{}",
           static_cast<uint32_t>(entity), source_skill_id, intent->stacks - before,
           intent->stacks, intent->max_stacks);
  return true;
}

bool SkillSystem::ConsumeSwordIntent(entt::registry &registry,
                                     entt::entity entity, int amount,
                                     uint32_t source_skill_id) {
  if (amount <= 0) {
    return false;
  }
  if (registry.all_of<BladeResourceComponent>(entity)) {
    const bool consumed = systems::BladeResourceService::Consume(
        registry, entity, amount, source_skill_id);
    if (consumed && source_skill_id != 0) {
      SkillExecutionContext consumeContext = BuildSkillVfxContextFromEvent(
          registry, entity, source_skill_id, 0u,
          ResolveEntityWorldPosition(registry, entity));
      EmitSkillVfxEvent(consumeContext, SkillVfxEventType::EmpoweredConsume,
                       1.1f);
    }
    return consumed;
  }
  auto *intent = registry.try_get<SwordIntentComponent>(entity);
  if (!intent || intent->stacks < amount) {
    return false;
  }
  intent->stacks -= amount;
  intent->time_since_last_gain = 0.0f;
  intent->decay_tick_timer = 0.0f;
  registry.get_or_emplace<StatsDirty>(entity);
  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateResourceConsumed(
                    entity, Tag::SwordSkill, static_cast<float>(amount),
                    source_skill_id));
  if (source_skill_id != 0) {
    SkillExecutionContext consumeContext = BuildSkillVfxContextFromEvent(
        registry, entity, source_skill_id, 0u,
        ResolveEntityWorldPosition(registry, entity));
    EmitSkillVfxEvent(consumeContext, SkillVfxEventType::EmpoweredConsume, 1.1f);
  }
  LOG_INFO("SwordIntent consume: entity={} skill={} delta={} stacks={}/{}",
           static_cast<uint32_t>(entity), source_skill_id, amount, intent->stacks,
           intent->max_stacks);
  return true;
}

void SkillSystem::RebakeSkillProfiles(entt::registry &registry, entt::entity entity) {
  auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return;

  for (size_t i = 0; i < SkillConstants::MAX_SKILL_SLOTS; ++i) {
    const uint32_t skill_id = active->slots[i].id;
    if (skill_id == 0 || skill_id == INVALID_SKILL_ID) {
      const uint32_t old_skill_id = active->baked_profiles[i].skill_id;
      if (old_skill_id != 0 && old_skill_id != INVALID_SKILL_ID) {
        bool still_equipped = false;
        for (size_t k = 0; k < SkillConstants::MAX_SKILL_SLOTS; ++k) {
          if (k != i && active->slots[k].id == old_skill_id) {
            still_equipped = true;
            break;
          }
        }
        if (!still_equipped) {
          SkillSpecializationBaker::SyncTriggerRules(registry, entity, old_skill_id, nullptr);
        }
      }
      if (!(active->baked_profiles[i] == BakedSkillProfile{})) {
        active->baked_profiles[i] = BakedSkillProfile{};
      }
      continue;
    }

    const auto *skillData = SkillRegistry::Get().GetSkill(skill_id);
    if (!skillData) {
      BakedSkillProfile p{};
      p.skill_id = skill_id;
      if (!(active->baked_profiles[i] == p)) {
        active->baked_profiles[i] = p;
      }
      continue;
    }

    const SpecializedSkill *specPtr = nullptr;
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == skill_id) {
        specPtr = &spec;
        break;
      }
    }

    SkillSpecializationBaker::SyncTriggerRules(registry, entity, skill_id, specPtr);

    BakedSkillProfile new_profile{};
    SkillSpecializationBaker::Bake(registry, entity, skill_id, specPtr, new_profile, nullptr);

    // 幂等判断：先比较后写入，未变化时跳过写回
    if (active->baked_profiles[i] == new_profile) {
      continue;
    }
    active->baked_profiles[i] = new_profile;
  }
}

const BakedSkillProfile *SkillSystem::GetBakedSkillProfile(const entt::registry &registry,
                                                          entt::entity entity,
                                                          uint32_t skill_id) {
  const auto *active = registry.try_get<ActiveSkillsComponent>(entity);
  if (!active)
    return nullptr;

  for (size_t i = 0; i < SkillConstants::MAX_SKILL_SLOTS; ++i) {
    if (active->baked_profiles[i].skill_id == skill_id) {
      return &active->baked_profiles[i];
    }
  }
  return nullptr;
}

} // namespace NoMoreDay
