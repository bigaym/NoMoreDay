#include "game/systems/skill/SkillSystem.hpp"
#include "core/logging/Logger.hpp"
#include "core/utils/FrameRateUtils.hpp" // Frame-rate independent utilities
#include "engine/physics/SpatialGrid.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "game/components/AIComponent.hpp" // For EnemyTag
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp" // For Position
#include "game/components/EffectComponent.hpp"
#include "game/components/SkillVfxEvent.hpp"
#include "game/components/PlayerState.hpp" // For DashComponent
#include "game/components/Projectile.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/systems/combat/CombatSystem.hpp"
#include "game/systems/combat/CombatTelemetry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/combat/ProcBudgetManager.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/modifier/SkillSpecModifierAdapter.hpp"
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/systems/skill/behaviors/MindBlade.hpp"
#include "game/systems/skill/behaviors/PhantomFlash.hpp" // Added
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/SwordArray.hpp"
#include "raymath.h"
#include <algorithm>
#include <deque>
#include <map>
#include <unordered_map>
#include <unordered_set>


namespace NoMoreDay {

// Static scratch buffers to avoid per-frame allocations in hot paths
// (Replaced by local/thread_local buffers for safety and performance)

namespace {

constexpr uint8_t kMaxTriggerDepth = 2;
constexpr size_t kCastDepthRetention = 4096;
constexpr const char *kDiagTriggerCooldown = "SKILL_GUARD_TRIGGER_CD";
constexpr const char *kDiagTriggerDepth = "SKILL_GUARD_TRIGGER_DEPTH";
constexpr const char *kDiagTransmuterMutex = "SKILL_GUARD_TRANSMUTER_MUTEX";
constexpr const char *kDiagScopePolicy = "SKILL_GUARD_SCOPE_POLICY";
constexpr const char *kDiagTriggerSkillUnavailable =
    "SKILL_GUARD_TRIGGER_SKILL_UNAVAILABLE";
constexpr const char *kDiagTriggerManaBlocked = "SKILL_GUARD_TRIGGER_MANA";

std::unordered_map<uint64_t, uint8_t> g_cast_depth;
std::unordered_map<uint64_t, float> g_cast_trigger_effectiveness;
std::deque<uint64_t> g_cast_depth_order;

void RememberCastDepth(uint64_t cast_id, uint8_t depth,
                       float trigger_effectiveness = -1.0f) {
  if (cast_id == 0) {
    return;
  }
  g_cast_depth[cast_id] = depth;
  if (trigger_effectiveness >= 0.0f) {
    g_cast_trigger_effectiveness[cast_id] =
        (std::max)(0.0f, trigger_effectiveness);
  } else if (!g_cast_trigger_effectiveness.contains(cast_id)) {
    g_cast_trigger_effectiveness[cast_id] = 1.0f;
  }
  g_cast_depth_order.push_back(cast_id);
  while (g_cast_depth_order.size() > kCastDepthRetention) {
    const uint64_t stale = g_cast_depth_order.front();
    g_cast_depth_order.pop_front();
    if (auto it = g_cast_depth.find(stale); it != g_cast_depth.end()) {
      g_cast_depth.erase(it);
    }
    g_cast_trigger_effectiveness.erase(stale);
  }
}

uint8_t QueryCastDepth(uint64_t cast_id) {
  if (cast_id == 0) {
    return 0;
  }
  if (auto it = g_cast_depth.find(cast_id); it != g_cast_depth.end()) {
    return it->second;
  }
  return 0;
}

float QueryTriggerEffectiveness(uint64_t cast_id) {
  if (cast_id == 0) {
    return 1.0f;
  }
  if (auto it = g_cast_trigger_effectiveness.find(cast_id);
      it != g_cast_trigger_effectiveness.end()) {
    return it->second;
  }
  return 1.0f;
}

uint64_t NextCastId() {
  static uint64_t s_next_cast_id = 1;
  return s_next_cast_id++;
}

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

uint8_t EncodeElementTypeFromTags(const Tag tags) {
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
    return EncodeElementTypeFromTags(effective_tags);
  }

  const uint32_t activeTransmuter =
      SkillSystem::GetActiveTransmuterNode(registry, caster, skill_id);
  if (activeTransmuter != 0) {
    const auto *tree = SkillRegistry::Get().GetSkillTree(skill_id);
    if (tree) {
      if (auto it = tree->nodes.find(activeTransmuter); it != tree->nodes.end()) {
        const uint8_t nodeElement = EncodeElementTypeFromTags(it->second.add_tags);
        if (nodeElement != static_cast<uint8_t>(SkillVfxElementType::Physical)) {
          return nodeElement;
        }
      }
    }
  }
  return EncodeElementTypeFromTags(effective_tags);
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
  event.effectiveTags = context.effective_tags;
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

bool HasSwordStepBuff(const ActiveEffectsComponent *effects) {
  if (!effects) {
    return false;
  }
  for (const auto &effect : effects->effects) {
    if (effect.id == "flowing_thrust_swift" && effect.remaining > 0.0f) {
      return true;
    }
  }
  return false;
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

bool ValidateContractCastConstraints(
    const SkillRegistry &registry_data, const SkillContract *contract,
    const SpecializedSkill *specialized, uint32_t skill_id,
    std::vector<uint32_t> *allocated_transmuters,
    std::vector<uint32_t> *allocated_triggers) {
  if (!contract || !specialized) {
    return true;
  }

  uint32_t transmuter_count = 0;
  uint32_t trigger_count = 0;
  allocated_transmuters->clear();
  allocated_triggers->clear();

  for (const auto &[node_id, points] : specialized->allocated_points) {
    if (points <= 0) {
      continue;
    }
    const auto *node_contract = registry_data.GetNodeContract(skill_id, node_id);
    if (!node_contract) {
      continue;
    }
    if (node_contract->role == SpecNodeRole::Transmuter) {
      ++transmuter_count;
      allocated_transmuters->push_back(node_id);
    } else if (node_contract->role == SpecNodeRole::Trigger) {
      ++trigger_count;
      allocated_triggers->push_back(node_id);
    }
  }

  if (transmuter_count > contract->max_transmuters) {
    LOG_WARN("TryCast blocked: skill {} has {} transmuters > max {}",
             skill_id, transmuter_count,
             static_cast<uint32_t>(contract->max_transmuters));
    return false;
  }
  if (trigger_count > contract->max_triggers) {
    LOG_WARN("TryCast blocked: skill {} has {} triggers > max {}", skill_id,
             trigger_count, static_cast<uint32_t>(contract->max_triggers));
    return false;
  }

  if (allocated_transmuters->size() > 1) {
    // Keep one transmuter active at runtime to enforce mutual exclusion.
    uint32_t selected = 0;
    for (const uint32_t preferred : contract->transmuter_node_ids) {
      if (preferred == 0) {
        continue;
      }
      if (std::find(allocated_transmuters->begin(), allocated_transmuters->end(),
                    preferred) != allocated_transmuters->end()) {
        selected = preferred;
        break;
      }
    }
    if (selected == 0) {
      selected = allocated_transmuters->front();
    }
    allocated_transmuters->erase(
        std::remove_if(allocated_transmuters->begin(),
                       allocated_transmuters->end(),
                       [selected](uint32_t node_id) {
                         return node_id != selected;
                       }),
        allocated_transmuters->end());
    LOG_WARN("[{}] skill={} selected_transmuter={} conflicting_transmuters={}",
             kDiagTransmuterMutex, skill_id, selected, transmuter_count);
  }
  return true;
}

} // namespace

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
  s_skill_callbacks.clear();

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
          if (!node.behavior_id.empty()) {
            BehaviorInjectionRegistry::Apply(node.behavior_id, registry,
                                             exec.owner);
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

    if (SkillSystem::ConsumeSwordIntent(
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
                .color = NoMoreDay::Constants::Visuals::COLOR_BLADE_ASCENDANT});
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

        if (evt.skill_id != 0) {
          Vector2 impact = ResolveEntityWorldPosition(
              registry, evt.target, ResolveEntityWorldPosition(registry, caster));
          SkillExecutionContext hitContext = BuildSkillVfxContextFromEvent(
              registry, caster, evt.skill_id, evt.castId, impact, evt.tags);
          EmitSkillVfxEvent(hitContext, SkillVfxEventType::TriggerProc,
                            evt.isCrit ? 1.15f : 1.0f);
        }

        auto *intent = registry.try_get<SwordIntentComponent>(caster);
        if (intent && HasTag(evt.tags, Tag::Hit)) {
          bool gain_stack = false;
          const float current_time = static_cast<float>(GetTime());
          const bool is_continuous =
              HasTag(evt.tags, Tag::Channeled) || HasTag(evt.tags, Tag::Aura);

          // Use cast_id if available, otherwise fallback to skill_id.
          const uint64_t tracking_key =
              (evt.castId != 0) ? evt.castId : static_cast<uint64_t>(evt.skill_id);
          auto &tracking = intent->hit_tracking[tracking_key];

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

          if (gain_stack) {
            SkillSystem::GainSwordIntent(registry, caster, 1, evt.skill_id);
          }
        }

        // Unified Sword Step linkage: on-hit mana return and crit extension.
        if (HasTag(evt.tags, Tag::Hit) && registry.any_of<PhaseTag>(caster)) {
          auto *effects = registry.try_get<ActiveEffectsComponent>(caster);
          if (HasSwordStepBuff(effects)) {
            if (auto *stats = registry.try_get<CombatStats>(caster)) {
              stats->mana = std::min(stats->max_mana, stats->mana + 1.0f);
              registry.get_or_emplace<StatsDirty>(caster);
            }
            if (evt.isCrit && effects) {
              for (auto &effect : effects->effects) {
                if (effect.id == "flowing_thrust_swift") {
                  effect.remaining =
                      std::min(effect.duration + 0.5f, effect.remaining + 0.2f);
                  break;
                }
              }
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

            uint8_t parent_depth = QueryCastDepth(evt.castId);
            if (evt.castId != 0 && parent_depth == 0) {
              auto exec_view = registry.view<SkillExecution>();
              for (auto exec_entity : exec_view) {
                const auto &exec = exec_view.get<SkillExecution>(exec_entity);
                if (exec.cast_id == evt.castId) {
                  parent_depth = exec.trigger_depth;
                  RememberCastDepth(evt.castId, parent_depth);
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
              if (evt.skill_id == 9) {
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
              trigger_exec.cast_id = NextCastId();
              trigger_exec.state = SkillState::Preparing;
              trigger_exec.timer = 0.0f;
              trigger_exec.trigger_depth = static_cast<uint8_t>(parent_depth + 1);
              trigger_exec.trigger_effectiveness =
                  (std::max)(0.0f, node_contract->trigger.effectiveness);
              RememberCastDepth(trigger_exec.cast_id,
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

        // Dispatch to specific Skill Behavior.
        if (evt.skill_id != 0) {
          if (auto hitFunc = SkillBehaviorRegistry::GetHit(evt.skill_id)) {
            hitFunc(registry, evt.source, evt.target, evt.tags, evt.isCrit);
          }
        }
      },
      50);

  // 3. Phantom Flash Counter (Defensive)
  s_onTakeDamageHandlerId = CombatEventDispatcher::Register(
      CombatEventType::OnTakeDamage,
      [](entt::registry &registry, const CombatEvent &evt) {
        entt::entity victim =
            evt.source; // In OnTakeDamage, source is the victim (defender)
        entt::entity attacker =
            evt.target; // In OnTakeDamage, target is the attacker

        if (!registry.valid(victim) || !registry.valid(attacker))
          return;

        if (auto *pf = registry.try_get<PhantomFlashComponent>(victim)) {
          if (!pf->triggered && pf->counter_window > 0.0f) {
            pf->triggered = true;
            LOG_INFO("Phantom Flash Counter Triggered for entity {}!",
                     (uint32_t)victim);

            if (pf->flow_reset) {
              if (auto *active = registry.try_get<ActiveSkillsComponent>(victim)) {
                for (auto &slot : active->slots) {
                  if (slot.id != 8 || slot.cooldown <= 0.0f) {
                    continue;
                  }
                  slot.cooldown = std::max(0.0f, slot.cooldown - 1.5f);
                }
              }
            }

            // Logic: Deal counter damage to attacker
            if (registry.all_of<CombatStats>(attacker)) {
              // Create a "Counter Strike" execution
              auto exec_ent = registry.create();
              registry.emplace<LocalLevelTag>(exec_ent);
              auto &exec = registry.emplace<SkillExecution>(exec_ent);
              exec.skill_id = 9; // Phantom Flash
              exec.owner = victim;
              exec.state = SkillState::Preparing;
              exec.timer = 0.0f; // Instant
              exec.cast_id = NextCastId();
              exec.trigger_depth = 0;
              RememberCastDepth(exec.cast_id, exec.trigger_depth);

              if (registry.all_of<Position>(attacker)) {
                const auto &attr_pos = registry.get<Position>(attacker);
                exec.target_pos = {attr_pos.x, attr_pos.y};
              }

              // Add counter-attack tag or logic?
              if (auto *victim_stats = registry.try_get<CombatStats>(victim)) {
                if (victim_stats->damage_multipliers[0] <= 0.0f) {
                  victim_stats->damage_multipliers[0] = 1.0f;
                }
                DamagePool counterPool;
                const float baseCounterDamage =
                    (std::max)(25.0f, victim_stats->damage_multipliers[0] * 100.0f);
                const Tag counterTag =
                    (pf->enchant_tag != Tag::None) ? pf->enchant_tag : Tag::Physical;
                const float counterScale = pf->synergy_shadow_hide ? 1.2f : 1.0f;
                counterPool.Add(counterTag, baseCounterDamage * counterScale);

                DamageRequest counterRequest;
                counterRequest.attacker = victim;
                counterRequest.defender = attacker;
                counterRequest.skill_id = exec.skill_id;
                counterRequest.base_pool = counterPool;
                counterRequest.additional_tags = Tag::Hit | Tag::Melee;
                counterRequest.source_entity = exec_ent;
                const auto counterResult =
                    DamagePipeline::Execute(registry, counterRequest, victim);

                LOG_INFO(
                    "Phantom Flash Counter resolved: victim={} attacker={} "
                    "damage={}",
                    (uint32_t)victim, (uint32_t)attacker,
                    counterResult.damage.total_damage);
              } else {
                LOG_WARN("Phantom Flash Counter skipped: victim {} has no "
                         "CombatStats",
                         (uint32_t)victim);
              }
            }
            else {
              LOG_WARN("Phantom Flash Counter skipped: attacker {} has no "
                       "CombatStats",
                       (uint32_t)attacker);
            }
          }
        }
      },
      50);

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

  ClearHooks();
  s_skill_callbacks.clear();
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
    if (!HasSwordStepBuff(&effects)) {
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
    if (!HasSwordStepBuff(effects)) {
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

    // Talent: Ling Jian Hu Ti (灵剑护体) - ID 320
    if (auto *active = registry.try_get<ActiveSkillsComponent>(entity)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == 3 && spec.allocated_points.contains(320) &&
            spec.allocated_points.at(320) > 0) {
          auto &effects =
              registry.get_or_emplace<ActiveEffectsComponent>(entity);
          BuffEffect bladeDR;
          bladeDR.id = "ling_jian_hu_ti";
          bladeDR.name = "Ling Jian Hu Ti";
          bladeDR.type = BuffType::Shield;
          bladeDR.duration = 0.2f; // Short duration, refreshed every update
          bladeDR.remaining = 0.2f;

          float dr_per_sword = 2.0f * spec.allocated_points.at(320);
          float total_dr = formation.current_swords * dr_per_sword;

          bladeDR.modifiers.push_back({.value = total_dr,
                                       .type = StatType::ResistAll,
                                       .mode = ModifierMode::Flat});
          effects.AddOrRefresh(bladeDR);
          break;
        }
      }
    }
  }

  // Update Sword Array (ID 6)
  auto array_view = registry.view<SwordArrayComponent, Position>();
  for (auto entity : array_view) {
    auto &array = array_view.get<SwordArrayComponent>(entity);
    skills::SwordArray::Update(registry, entity, array, dt, grid);
  }

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

  // Update Channeling (ID 5 & 7)
  auto chan_view = registry.view<ChannelingComponent, Position>();
  for (auto entity : chan_view) {
    auto &chan = chan_view.get<ChannelingComponent>(entity);
    const auto &pos = chan_view.get<Position>(entity);

    // For skill 7, target_pos is updated in InputSystem.cpp to follow mouse
    // accurately.

    // 1. Duration Limit (5s hard cap)
    chan.total_duration += dt;
    if (chan.total_duration >= 5.0f) {
      registry.remove<ChannelingComponent>(entity);
      continue;
    }

    chan.channel_timer -= dt;
    if (chan.channel_timer <= 0.0f) {
      // Burst Finisher (Talent 513)
      if (chan.skill_id == 5 && chan.burst_finisher) {
        // Giant Sword Slash logic
        auto finisher_ent = registry.create();
        registry.emplace<LocalLevelTag>(finisher_ent);
        registry.emplace<ShadowCastTag>(finisher_ent);
        registry.emplace<Position>(finisher_ent, pos.x, pos.y);

        // Use Rending Wave (ID 2) but massively scaled
        auto &exec = registry.emplace<SkillExecution>(finisher_ent);
        exec.skill_id = 2;
        exec.owner = entity;
        exec.state = SkillState::Preparing;
        exec.timer = 0.0f;                 // Instant
        exec.target_pos = chan.target_pos; // Final aim
        exec.is_empowered = true;

        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          exec.has_snapshot = true;
          exec.snapshot.stats = *stats;
          exec.snapshot.skill_id = 5; // Attribute to Infinite Blades
          // 500% Damage
          for (auto &mult : exec.snapshot.stats.damage_multipliers) {
            mult *= 5.0f;
          }
          // Size scale handled in RendingWave logic or Projectile logic?
          // Ideally we'd set a specific flag or use a different skill ID (e.g.
          // 2 with "Giant" tag) For now, relies on base damage scaling.
        }
        LOG_INFO("Infinite Blades: Triggered Burst Finisher!");
      }

      registry.remove<ChannelingComponent>(entity);
      continue;
    }

    chan.tick_timer -= dt;

    // 2. VFX: Channeling Aura
    if (chan.skill_id == 5 || chan.skill_id == 7) {
      auto &particleSys = systems::GPUParticleSystem::Get();
      if ((float)GetRandomValue(0, 1000) < 500.0f * dt) {
        components::GPUParticle p;
        p.position = {pos.x + (float)GetRandomValue(-20, 20),
                      pos.y + (float)GetRandomValue(-10, 10)};
        p.velocity = {(float)GetRandomValue(-20, 20),
                      -50.0f - (float)GetRandomValue(0, 50)};
        p.color = chan.is_empowered ? GOLD : SKYBLUE;
        p.lifetime = 0.6f;
        p.maxLifetime = 0.6f;
        p.scale = 2.0f;
        p.flags = 2; // Spark
        particleSys.Emit(p);
      }
    }

    // Skill 7 continuous channel visuals:
    // - persistent distortion at cursor
    // - very thin caster->cursor guiding link (alpha ~0.1)
    if (chan.skill_id == 7) {
      Vector2 diff = Vector2Subtract(chan.target_pos, {pos.x, pos.y});
      float dist = Vector2Length(diff);
      float max_range = 350.0f;
      Vector2 cutPos = chan.target_pos;
      Vector2 dir = {1.0f, 0.0f};
      if (dist > 0.001f) {
        dir = Vector2Scale(diff, 1.0f / dist);
        if (dist > max_range) {
          cutPos = {pos.x + dir.x * max_range, pos.y + dir.y * max_range};
        }
      } else {
        cutPos = {pos.x + 50.0f, pos.y};
      }

      if (chan.synergy_lock) {
        float bestDistSq = 450.0f * 450.0f;
        entt::entity bestTarget = entt::null;
        grid.query({pos.x, pos.y}, 450.0f,
                   [&](entt::entity e, const Position &ep) {
                     if (!registry.any_of<EnemyTag>(e) ||
                         registry.any_of<KilledTag>(e)) {
                       return;
                     }
                     float distSq = Vector2DistanceSqr({pos.x, pos.y}, {ep.x, ep.y});
                     if (distSq < bestDistSq) {
                       bestDistSq = distSq;
                       bestTarget = e;
                     }
                   });
        if (registry.valid(bestTarget) && registry.all_of<Position>(bestTarget)) {
          const auto &tp = registry.get<Position>(bestTarget);
          cutPos = {tp.x, tp.y};
        }
      }

      Tag effectiveTags = GetEffectiveSkillTags(registry, entity, 7u);
      if (chan.conversion_tag != Tag::None) {
        effectiveTags = (effectiveTags & ~Tag::Physical) | chan.conversion_tag;
      }
      const uint8_t elementType = EncodeElementTypeFromTags(effectiveTags);
      const bool hasVoidRift = (chan.conversion_tag == Tag::Void);
      const bool isCold = HasTag(effectiveTags, Tag::Cold);
      const bool isLightning = HasTag(effectiveTags, Tag::Lightning);
      const bool isEmpowered = chan.is_empowered;

      // Persistent rift core: non-particle body for readability.
      {
        components::GPUSkillEffect riftRing = {};
        riftRing.position = cutPos;
        riftRing.velocity = Vector2Scale(dir, 40.0f);
        riftRing.coreColor =
            isEmpowered ? Vector4{0.22f, 0.30f, 0.48f, 0.98f}
                        : (hasVoidRift ? Vector4{0.08f, 0.07f, 0.14f, 0.95f}
                                       : Vector4{0.18f, 0.24f, 0.38f, 0.90f});
        riftRing.glowColor =
            isLightning ? Vector4{0.72f, 0.56f, 1.00f, 0.90f}
                        : (isCold ? Vector4{0.76f, 0.92f, 1.00f, 0.88f}
                                  : Vector4{0.46f, 0.74f, 1.00f, 0.84f});
        riftRing.radius = 24.0f;
        riftRing.sectorAngle = 360.0f;
        riftRing.type = isEmpowered   ? 7.0f
                        : hasVoidRift ? 3.0f
                        : isLightning ? 6.0f
                        : isCold      ? 5.0f
                                      : 4.0f;
        riftRing.flags =
            NoMoreDay::render::skillfx::PackSkillEffectFlags(elementType, 7u);
        systems::GPUSkillEffectSystem::Get().Submit(riftRing);
        LOG_LIMITED_INFO(
            1.0f,
            "Skill7VFX channel: riftType={:.0f} element={} empowered={} void={} cold={} lightning={}",
            riftRing.type, static_cast<uint32_t>(elementType), isEmpowered ? 1 : 0,
            hasVoidRift ? 1 : 0, isCold ? 1 : 0, isLightning ? 1 : 0);
      }
      const float distortionRadius = isEmpowered ? 34.0f : (hasVoidRift ? 32.0f : 28.0f);
      const float distortionStrength =
          isEmpowered ? 0.30f : (hasVoidRift ? 0.26f : 0.22f);
      RenderSystem::AddDistortionSource(cutPos.x, cutPos.y, distortionRadius,
                                        distortionStrength);

      auto &particleSys = systems::GPUParticleSystem::Get();
      if ((float)GetRandomValue(0, 1000) < 700.0f * dt) {
        constexpr int kSamples = 6;
        for (int i = 1; i <= kSamples; ++i) {
          const float t = static_cast<float>(i) / static_cast<float>(kSamples + 1);
          const Vector2 samplePos = Vector2Lerp({pos.x, pos.y}, cutPos, t);
          components::GPUParticle link = {};
          link.position = samplePos;
          link.velocity = {0.0f, 0.0f};
          link.acceleration = {0.0f, 0.0f};
          link.color = isEmpowered ? Color{236, 246, 255, 96}
                                   : Color{185, 225, 240, 48};
          link.scale = isEmpowered ? 3.2f : 2.6f;
          link.lifetime = 0.13f;
          link.maxLifetime = 0.13f;
          link.flags = 1;
          link.growthRate = -4.0f;
          particleSys.Emit(link);
        }
      }

      if (isLightning && (float)GetRandomValue(0, 1000) < 800.0f * dt) {
        for (int i = 0; i < 2; ++i) {
          components::GPUParticle arc = {};
          arc.position = {cutPos.x + (float)GetRandomValue(-18, 18),
                          cutPos.y + (float)GetRandomValue(-18, 18)};
          arc.velocity = {(float)GetRandomValue(-40, 40),
                          (float)GetRandomValue(-40, 40)};
          arc.acceleration = {0.0f, 0.0f};
          arc.color = Color{220, 188, 255, 215};
          arc.scale = 4.4f;
          arc.lifetime = 0.18f;
          arc.maxLifetime = 0.18f;
          arc.flags = 2;
          arc.growthRate = -9.0f;
          particleSys.Emit(arc);
        }
      }
    }

    if (chan.tick_timer <= 0.0f) {
      if (chan.skill_id == 5) {
        // Infinite Blades (Wan Jian Gui Zong)

        // 1. Determine Count (Base 2 per 0.2s - Smoother stream)
        int projectileCount = 2;
        if (chan.extra_projectiles) {
          projectileCount += 2; // 4 total
        }

        // 2. Target Logic (Full Screen Lock check)
        Vector2 targetPos = chan.target_pos;
        if (chan.full_screen_lock) {
          float bestDistSq = 900.0f * 900.0f;
          entt::entity bestTarget = entt::null;
          grid.query({targetPos.x, targetPos.y}, 900.0f,
                     [&](entt::entity e, const Position &ep) {
                       if (registry.any_of<EnemyTag>(e) &&
                           !registry.any_of<KilledTag>(e)) {
                         float distSq = Vector2DistanceSqr(
                             {targetPos.x, targetPos.y}, {ep.x, ep.y});
                         if (distSq < bestDistSq) {
                           bestDistSq = distSq;
                           bestTarget = e;
                         }
                       }
                     });
          if (registry.valid(bestTarget) &&
              registry.all_of<Position>(bestTarget)) {
            const auto &tp = registry.get<Position>(bestTarget);
            targetPos = {tp.x, tp.y};
          }
        }

        Vector2 dirToTarget =
            Vector2Normalize(Vector2Subtract(targetPos, {pos.x, pos.y}));

        // 3. Loop and Spawn
        for (int i = 0; i < projectileCount; ++i) {
          float spreadAmt = (float)GetRandomValue(-20, 20) * DEG2RAD;
          Vector2 fireDir = Vector2Rotate(dirToTarget, spreadAmt);

          auto proj_ent = registry.create();
          registry.emplace<LocalLevelTag>(proj_ent);
          registry.emplace<ShadowCastTag>(proj_ent);
          // Spawn a bit forward
          registry.emplace<Position>(proj_ent, pos.x + fireDir.x * 20.0f,
                                     pos.y + fireDir.y * 20.0f);

          float speed = 1000.0f;
          registry.emplace<Velocity>(proj_ent, fireDir.x * speed,
                                     fireDir.y * speed);

          // Visuals: Deep Sky Blue for body + White Rim (from Shader).
          // Using manual Color value for stronger presence than BLADE_CYAN.
          Color swordColor = chan.is_empowered
                                 ? GOLD
                                 : ColorAlpha(Color{0, 170, 255, 255}, 0.5f);
          if (chan.conversion_tag == Tag::Fire) {
            swordColor = ORANGE;
          } else if (chan.conversion_tag == Tag::Cold) {
            swordColor = SKYBLUE;
          } else if (chan.conversion_tag == Tag::Lightning) {
            swordColor = PURPLE;
          } else if (chan.conversion_tag == Tag::Void) {
            swordColor = Color{120, 90, 180, 255};
          }
          registry.emplace<ColorComponent>(proj_ent, swordColor);

          auto &proj = registry.emplace<Projectile>(proj_ent);
          proj.owner = entity;
          proj.cast_id = chan.cast_id;
          proj.radius = 35.0f;
          proj.speed = speed;
          proj.lifeTime = 1.2f;
          proj.visualType = 2; // SWORD

          // Stats & Damage
          if (auto *stats = registry.try_get<CombatStats>(entity)) {
            proj.snapshot = *stats;
            // Scale damage: 35% per sword
            for (auto &mult : proj.snapshot.damage_multipliers) {
              mult *= (0.35f * chan.bonus_damage_mult);
            }
            proj.snapshot.crit_chance += chan.bonus_crit_chance;
            proj.snapshot.armor_pen += chan.bonus_armor_pen;
          }

          // CRITICAL: Add SkillComponent so DamagePipeline knows this is Skill
          // 5
          auto &sc = registry.emplace<SkillComponent>(proj_ent);
          sc.skill_id = 5;
          if (chan.conversion_tag != Tag::None) {
            auto &mods = registry.emplace<SkillModifierComponent>(proj_ent);
            mods.damage_modifiers.push_back(
                DamageModifier{Tag::Physical, chan.conversion_tag, 1.0f,
                               ModifierType::Convert});
          }

          // VFX: Flash on spawn
          auto &particleSys = systems::GPUParticleSystem::Get();
          components::GPUParticle p;
          p.position = {pos.x + fireDir.x * 30.0f, pos.y + fireDir.y * 30.0f};
          p.velocity = Vector2Scale(fireDir, 200.0f);
          p.color = chan.is_empowered ? GOLD : ColorAlpha(WHITE, 0.6f);
          p.lifetime = 0.2f;
          p.maxLifetime = 0.2f;
          p.scale = 1.8f;
          p.flags = 2;
          particleSys.Emit(p);
        }

        chan.tick_timer = std::max(0.08f, chan.tick_interval);

      } else if (chan.skill_id == 7) {
        // 1. Calculate Cut Position (Clamped to Range)
        Vector2 diff = Vector2Subtract(chan.target_pos, {pos.x, pos.y});
        float dist = Vector2Length(diff);
        float max_range = 350.0f;
        Vector2 cutPos = chan.target_pos;
        Vector2 dir = {1.0f, 0.0f}; // Default if dist is 0

        if (dist > 0.001f) {
          dir = Vector2Scale(diff, 1.0f / dist); // Normalize
          if (dist > max_range) {
            cutPos = {pos.x + dir.x * max_range, pos.y + dir.y * max_range};
          }
        } else {
          cutPos = {pos.x + 50.0f, pos.y};
        }

        const Tag effectiveTags = GetEffectiveSkillTags(registry, entity, 7u);
        const uint8_t elementType = EncodeElementTypeFromTags(effectiveTags);
        const bool isCold = HasTag(effectiveTags, Tag::Cold);
        const bool isLightning = HasTag(effectiveTags, Tag::Lightning);
        const bool isEmpowered = chan.is_empowered;

        // 2. VFX: random-angle high-frequency cut lines (1-2 per tick)
        auto &particleSys = systems::GPUParticleSystem::Get();
        const int slashCount =
            (isEmpowered ? GetRandomValue(4, 8) : GetRandomValue(2, 4)) +
            ((chan.bonus_damage_mult > 1.2f) ? 1 : 0);
        for (int s = 0; s < slashCount; ++s) {
          const float slashAngle =
              static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
          const Vector2 slashDir = {cosf(slashAngle), sinf(slashAngle)};
          const float halfLen =
              isEmpowered ? static_cast<float>(GetRandomValue(20, 34))
                          : static_cast<float>(GetRandomValue(16, 26));
          constexpr int kSegments = 12;
          for (int i = 0; i < kSegments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSegments - 1);
            const float offset = (t - 0.5f) * (halfLen * 2.0f);
            const Vector2 pPos = {cutPos.x + slashDir.x * offset,
                                  cutPos.y + slashDir.y * offset};

            components::GPUParticle cut = {};
            cut.position = pPos;
            cut.velocity = Vector2Scale(slashDir, static_cast<float>(GetRandomValue(20, 60)));
            cut.acceleration = {0.0f, 0.0f};
            cut.color = isLightning ? Color{214, 188, 255, 220}
                        : (isCold ? Color{210, 245, 255, 218}
                                  : Color{200, 242, 255, 210});
            cut.scale = isEmpowered ? 5.0f : 4.0f;
            cut.lifetime = 0.20f;
            cut.maxLifetime = 0.20f;
            cut.flags = 2;
            cut.growthRate = -10.5f;
            particleSys.Emit(cut);
          }

          // Add a short non-particle slash body to avoid "only thin particles".
          components::GPUSkillEffect slashBody = {};
          slashBody.position = cutPos;
          slashBody.velocity = Vector2Scale(slashDir, 900.0f);
          slashBody.coreColor = isLightning ? Vector4{0.66f, 0.58f, 1.00f, 0.96f}
                               : (isCold ? Vector4{0.72f, 0.90f, 1.00f, 0.96f}
                                         : Vector4{0.42f, 0.82f, 1.00f, 0.95f});
          slashBody.glowColor = isLightning ? Vector4{0.90f, 0.84f, 1.00f, 0.90f}
                               : (isCold ? Vector4{0.88f, 0.96f, 1.00f, 0.90f}
                                         : Vector4{0.24f, 0.56f, 0.96f, 0.88f});
          slashBody.radius = halfLen * 1.35f;
          slashBody.sectorAngle = 0.0f;
          slashBody.type = 2.0f;
          slashBody.flags =
              NoMoreDay::render::skillfx::PackSkillEffectFlags(elementType, 7u);
          systems::GPUSkillEffectSystem::Get().Submit(slashBody);
        }

        // 3. Edge shard fragments: dark geometric debris, slow outward.
        const int shardCount = GetRandomValue(2, 4); // doubled
        LOG_LIMITED_INFO(
            1.0f,
            "Skill7VFX tick: slashCount={} segments={} shardCount={} elem={} empowered={}",
            slashCount, 12, shardCount, static_cast<uint32_t>(elementType),
            isEmpowered ? 1 : 0);
        for (int i = 0; i < shardCount; ++i) {
          const float a = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
          const float r = static_cast<float>(GetRandomValue(12, 20));
          const Vector2 spawn = {cutPos.x + cosf(a) * r, cutPos.y + sinf(a) * r};

          components::GPUParticle shard = {};
          shard.position = spawn;
          shard.velocity = {cosf(a) * static_cast<float>(GetRandomValue(8, 20)),
                            sinf(a) * static_cast<float>(GetRandomValue(8, 20))};
          shard.acceleration = {0.0f, 0.0f};
          shard.color = isCold ? Color{62, 72, 90, 190}
                      : (isLightning ? Color{74, 54, 110, 190}
                                     : Color{36, 40, 52, 180});
          shard.scale = 4.8f;
          shard.lifetime = 0.62f;
          shard.maxLifetime = 0.62f;
          shard.flags = 2;
          shard.growthRate = -1.5f;
          particleSys.Emit(shard);
        }

        // 3. Logic: Spawn "Cut" Hitbox (Stationary Projectile)
        auto exec_ent = registry.create();
        registry.emplace<LocalLevelTag>(exec_ent);
        registry.emplace<Position>(exec_ent, cutPos.x, cutPos.y);
        registry.emplace<Velocity>(exec_ent, 0.0f, 0.0f);

        auto &proj = registry.emplace<Projectile>(exec_ent);
        proj.owner = entity;
        proj.cast_id = chan.cast_id;
        proj.radius = 60.0f * std::clamp(chan.bonus_damage_mult, 1.0f, 1.35f);
        proj.speed = 0.0f;
        proj.lifeTime = 0.1f; // Instant hit (one frame)
        proj.pierce = true;
        proj.pierceCount = 999;

        // Link stats
        if (auto *stats = registry.try_get<CombatStats>(entity)) {
          proj.snapshot = *stats;
          for (auto &mult : proj.snapshot.damage_multipliers) {
            mult *= chan.bonus_damage_mult;
          }
          proj.snapshot.crit_chance += chan.bonus_crit_chance;
          proj.snapshot.armor_pen += chan.bonus_armor_pen;
        }

        auto &sc = registry.emplace<SkillComponent>(exec_ent);
        sc.skill_id = 7;
        if (chan.conversion_tag != Tag::None) {
          auto &mods = registry.emplace<SkillModifierComponent>(exec_ent);
          mods.damage_modifiers.push_back(
              DamageModifier{Tag::Physical, chan.conversion_tag, 1.0f,
                             ModifierType::Convert});
        }

        chan.tick_timer = chan.tick_interval;
      }
    }
  }

  // Update Blade Ward
  auto ward_view = registry.view<BladeWardComponent>();
  for (auto entity : ward_view) {
    auto &ward = ward_view.get<BladeWardComponent>(entity);

    const auto *effects = registry.try_get<ActiveEffectsComponent>(entity);
    const BuffEffect *wardBuff = nullptr;
    if (effects != nullptr) {
      for (const auto &effect : effects->effects) {
        if (effect.id == "blade_ward") {
          wardBuff = &effect;
          break;
        }
      }
    }

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

void SkillSystem::RegisterEffect(uint32_t skill_id, CastCallback callback) {
  s_skill_callbacks[skill_id] = callback;
}

void SkillSystem::AddPreCastHook(SkillHook hook) {
  s_pre_cast_hooks.push_back(hook);
}

void SkillSystem::AddPostCastHook(SkillHook hook) {
  s_post_cast_hooks.push_back(hook);
}

void SkillSystem::ClearHooks() {
  s_pre_cast_hooks.clear();
  s_post_cast_hooks.clear();
}

float SkillSystem::GetTriggerEffectivenessForCast(uint64_t cast_id) {
  return QueryTriggerEffectiveness(cast_id);
}

bool SkillSystem::ShadowCast(entt::registry &registry, entt::entity owner,
                             uint32_t skill_id, Vector2 position,
                             Vector2 target_pos) {
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

  exec.cast_id = NextCastId();
  RememberCastDepth(exec.cast_id, exec.trigger_depth);
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
    // No empowerment by default for non-snapshot casts unless we want it?
  }

  // Ensure shadow components are initialized
  if (!registry.all_of<ShadowComponent>(shadow)) {
    auto &sc = registry.get_or_emplace<ShadowComponent>(shadow);
    sc.damage_scale = 0.3f; // Default 30%

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

void SkillSystem::UpdateSwordIntent(entt::registry &registry, float dt) {
  auto view = registry.view<SwordIntentComponent>();
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

      if (slot.current_charges < data->max_charges) {
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
            if (slot.current_charges < data->max_charges) {
              auto *stats = registry.try_get<CombatStats>(entity);
              float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
              float cdr = StatsSystem::GetStatWithTags(
                              registry, entity, StatType::CooldownReduction,
                              data->tags, slot.id) /
                          100.0f;
              slot.cooldown =
                  (data->cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
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
          } else if (s_skill_callbacks.contains(current_exec.skill_id)) {
            s_skill_callbacks[current_exec.skill_id](registry, current_exec.owner, current_exec);
          } else {
            LOG_WARN("UpdateStates: No callback found for skill ID {} on entity {}",
                     current_exec.skill_id, (uint32_t)entity);
          }
        }
        break;

      case SkillState::Casting:
        exec.state = SkillState::Settle;
        exec.timer = 0.1f;
        for (auto &hook : s_post_cast_hooks) {
          if (registry.valid(entity) && registry.all_of<SkillExecution>(entity)) {
            hook(registry, entity, registry.get<SkillExecution>(entity));
          }
        }
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
    registry.remove<SkillExecution>(s_to_remove.begin(), s_to_remove.end());
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

  const SpecializedSkill *specialized =
      FindSpecializedSkill(*active, slot.id, slot_index);
  const auto *skill_contract = SkillRegistry::Get().GetSkillContract(slot.id);
  static thread_local std::vector<uint32_t> s_allocated_transmuters;
  static thread_local std::vector<uint32_t> s_allocated_triggers;
  if (!ValidateContractCastConstraints(SkillRegistry::Get(), skill_contract,
                                       specialized, slot.id,
                                       &s_allocated_transmuters,
                                       &s_allocated_triggers)) {
    return false;
  }

  if (slot.current_charges <= 0) {
    LOG_TRACE("TryCast: Skill {} has no charges ({} / {})", data->name_key,
              slot.current_charges, data->max_charges);
    return false;
  }

  auto *stats = registry.try_get<CombatStats>(entity);
  float rcr = stats ? StatsSystem::GetStatWithTags(
                          registry, entity, StatType::ResourceCostReduction,
                          data->tags, slot.id) /
                          100.0f
                    : 0.0f;
  float base_cost = data->mana_cost * (1.0f - std::min(0.9f, rcr));

  // --- Shadow Kill Array (ID 124) Duplication Logic ---
  bool shadow_duplicate = false;
  if (registry.any_of<ShadowKillArrayReady>(entity)) {
    bool excluded =
        HasTag(data->tags, Tag::Movement) || HasTag(data->tags, Tag::Buff) ||
        HasTag(data->tags, Tag::Aura) || HasTag(data->tags, Tag::Channeled);

    if (!excluded) {
      auto *pStats = registry.try_get<PlayerStats>(entity);
      float currentTime = (float)GetTime();
      if (pStats && (currentTime - pStats->last_shadow_trigger_time >= 3.0f)) {
        float extra_cost = base_cost * 0.5f;
        if (stats && stats->mana >= (base_cost + extra_cost)) {
          shadow_duplicate = true;
        }
      }
    }
  }

  if (stats) {
    float total_cost = base_cost;
    if (shadow_duplicate)
      total_cost += base_cost * 0.5f;

    if (stats->mana < total_cost)
      return false;
    stats->mana -= total_cost;
  }

  if (shadow_duplicate) {
    auto *pStats = registry.try_get<PlayerStats>(entity);
    if (pStats)
      pStats->last_shadow_trigger_time = (float)GetTime();
    registry.remove<ShadowKillArrayReady>(entity);

    auto *pos = registry.try_get<Position>(entity);
    Vector2 spawnPos = pos ? Vector2{pos->x, pos->y} : Vector2{0, 0};

    auto shadow_ent = registry.create();
    registry.emplace<LocalLevelTag>(shadow_ent);
    registry.emplace<Position>(shadow_ent, spawnPos.x, spawnPos.y);
    registry.emplace<AnimationStateComponent>(shadow_ent);
    registry.emplace<ColorComponent>(shadow_ent, ColorAlpha(PURPLE, 0.4f));
    registry.emplace<ShadowCloneComponent>(shadow_ent);

    auto &sc = registry.emplace<ShadowComponent>(shadow_ent);
    sc.damage_scale = 0.5f; // Explicit 50% for Shadow Kill Array
    sc.delay = 0.1f;
    sc.lifetime = 1.0f;
    sc.snapshot.skill_id = slot.id;
    sc.snapshot.position = spawnPos;
    sc.snapshot.target_pos = target_pos;
    if (stats) {
      sc.snapshot.stats = *stats;
    }

    registry.emplace<ShadowVisualComponent>(shadow_ent).color_tint = {
        60, 0, 80, 200}; // Distinct visual for clone
    LOG_INFO("Shadow Kill Array: Duplicating skill {} for entity {}", slot.id,
             (uint32_t)entity);
  }

  if (slot.current_charges == data->max_charges) {
    float cdr = StatsSystem::GetStatWithTags(registry, entity,
                                             StatType::CooldownReduction,
                                             data->tags, slot.id) /
                100.0f;
    float recovery = stats ? stats->cooldown_recovery_speed : 1.0f;
    // Optimization: For Channeled skills with very long cooldowns (like 60s),
    // we might NOT want to start cooldown here but when channeling ends?
    // But preventing abuse is safer.
    slot.cooldown = (data->cooldown / recovery) * (1.0f - std::min(0.75f, cdr));
  }
  slot.current_charges--;

  const uint64_t cast_id = NextCastId();

  auto &exec = registry.emplace<SkillExecution>(entity);
  exec.skill_id = slot.id;
  exec.owner = entity;
  exec.cast_id = cast_id;
  exec.slot_index = slot_index;
  exec.state = SkillState::Preparing;
  exec.timer = 0.1f;
  exec.target_pos = target_pos;
  exec.trigger_depth = 0;
  RememberCastDepth(exec.cast_id, exec.trigger_depth);

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
    if (slot.skill_id == 0)
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

} // namespace NoMoreDay
