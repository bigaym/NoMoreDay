/**
 * @file RendingWave.cpp
 * @brief 瑁傜┖鏂?(ID 2) - 鎶曞皠鐗╂妧鑳借涓哄疄鐜?
 *
 * 鍙戝皠鎵囧舰鍓戞皵娉紝鍙┛閫忔晫浜恒€?
 *
 * 澶╄祴鍒嗘敮:
 * - 210 澶氶噸鍓戞皵: 棰濆鎶曞皠鐗?
 * - 230 鍥炴棆鍔? 鍥炴棆闀栨晥鏋?
 * - 250 鐏靛姏杞寲: 鐗╃悊杞櫄绌?
 * - 252 鍓戞剰鐖嗗彂: 婊″眰鐖嗗彂
 * - 253 鍓戞剰姹插彇: 鍛戒腑鍥炲墤鎰?
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUData.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/vfx/VFXSequenceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/Projectile.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "raymath.h"
#include <string>


namespace NoMoreDay::skills {
namespace {

ElementalConversion ResolveRendingWaveHeavenlyAttunementConversion(const Tag tag) {
  switch (tag) {
  case Tag::Fire:
    return ElementalConversion{Tag::Physical, Tag::Fire, {255, 80, 20, 255},
                               {255, 160, 60, 180}};
  case Tag::Cold:
    return ElementalConversion{Tag::Physical, Tag::Cold, {100, 200, 255, 255},
                               {150, 220, 255, 180}};
  case Tag::Lightning:
    return ElementalConversion{Tag::Physical, Tag::Lightning,
                               {200, 180, 255, 255}, {230, 200, 255, 180}};
  default:
    return {};
  }
}

bool TryPlayRendingWaveSequence(entt::registry &registry, entt::entity owner,
                                const std::string &sequenceName,
                                Vector2 targetPos) {
  if (!registry.valid(owner) || sequenceName.empty()) {
    return false;
  }

  auto &manager = vfx::VFXSequenceManager::Get();
  if (manager.GetSequence(sequenceName) == nullptr) {
    return false;
  }

  manager.Play(registry, owner, sequenceName, entt::null, false, targetPos.x,
               targetPos.y, true);
  return true;
}

void RefundFlowingThrustCooldown(entt::registry &registry, entt::entity owner,
                                 uint32_t skillId, float amount) {
  if (amount <= 0.0f) {
    return;
  }
  auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return;
  }
  for (auto &slot : active->slots) {
    if (slot.id != skillId || slot.cooldown <= 0.0f) {
      continue;
    }
    slot.cooldown = std::max(0.0f, slot.cooldown - amount);
  }
}

void ResetSkillCooldown(entt::registry &registry, entt::entity owner,
                        uint32_t skillId) {
  auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return;
  }
  for (auto &slot : active->slots) {
    if (slot.id == skillId) {
      slot.cooldown = 0.0f;
    }
  }
}

} // namespace

namespace RendingWaveNodes {
// 鍩虹鍒嗘敮 / Base
constexpr uint32_t SwordQi = 200;     // 鍓戞皵绾垫í / Sword Qi
constexpr uint32_t Focus = 201;       // 鍑濈 / Focus

// 鎶曞皠鍒嗘敮 / Projectile branch
constexpr uint32_t MultiWave = 210;   // 澶氶噸鍓戞皵 / Multi Wave
constexpr uint32_t SplitBlade = 211;  // 纰庤涔嬪垉 / Split Blade
constexpr uint32_t ChainReact = 212;  // 杩為攣鍙嶅簲 / Chain Reaction
constexpr uint32_t BladeBurst = 213;  // 涓囧墤褰掑畻 / Blade Burst

// 鍥炴棆鍒嗘敮 / Boomerang branch
constexpr uint32_t Boomerang = 230;   // 鍥炴棆鍔?/ Boomerang
constexpr uint32_t GravityTrap = 231; // 寮曞姏闄烽槺 / Gravity Trap
constexpr uint32_t OverlapHit = 232;  // 閲嶅彔鎵撳嚮 / Overlap Hit
constexpr uint32_t TimeLock = 233;    // 鏃剁┖閿佸畾 / Time Lock

// 鍓戞剰鍒嗘敮 / Intent branch
constexpr uint32_t VoidConvert = 250; // 鐏靛姏杞寲 / Void Convert
constexpr uint32_t VoidErode = 251;   // 铏氱┖渚佃殌 / Void Erode
constexpr uint32_t IntentBurst = 252; // 鍓戞剰鐖嗗彂 / Intent Burst
constexpr uint32_t IntentGain = 253;  // 鍓戞剰姹插彇 / Intent Gain

// 鍏冪礌鍒嗘敮 / Element branch
constexpr uint32_t ElementForm = 270;     // 鍏冪礌褰㈡€?/ Element Form
constexpr uint32_t AilmentSpread = 271;   // 寮傚父鎵╂暎 / Ailment Spread
constexpr uint32_t SpiritArmorPen = 272;  // 鐏垫牴鐮寸敳 / Spirit Armor Pen
} // namespace RendingWaveNodes

struct RendingWave : SkillBehaviorBase<RendingWave> {
  static constexpr uint32_t kSkillId = 2;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    auto *pos = registry.try_get<Position>(owner);
    auto *stats = registry.try_get<CombatStats>(owner);
    if (!pos || !stats)
      return;

    const auto sevenStarLink = seven_star_shared::ConsumeLinkBuffs(
        registry, owner, kSkillId, false, exec.cast_id);

    const auto *skillData = SkillRegistry::Get().GetSkill(exec.skill_id);
    Tag skillTags = skillData ? skillData->tags : Tag::None;
    float baseSpeed = skillData ? skillData->GetParam("speed", 300.0f) : 300.0f;
    float baseRadius = skillData ? skillData->GetParam("radius", 35.0f) : 35.0f;
    float baseLifetime =
        skillData ? skillData->GetParam("lifetime", 1.2f) : 1.2f;
    int currentSwordFlow = 0;
    if (const auto *resource = registry.try_get<BladeResourceComponent>(owner);
        resource != nullptr && resource->kind == BladeResourceKind::SwordFlow) {
      currentSwordFlow = std::clamp(resource->current, 0, 10);
      const float flowStacks = static_cast<float>(currentSwordFlow);
      baseSpeed *= (1.0f + flowStacks * 0.04f);
      baseRadius *= (1.0f + flowStacks * 0.03f);
    }

    // Apply Stats (Area of Effect, Projectile Speed)
    // GetStatWithTags returns values like 100.0f for 1.0 (100%)
    float areaStat = StatsSystem::GetStatWithTags(
        registry, owner, StatType::AreaScale, skillTags, exec.skill_id);
    float speedStat = StatsSystem::GetStatWithTags(
        registry, owner, StatType::ProjectileSpeed, skillTags, exec.skill_id);

    float areaScale = (areaStat > 0.1f) ? areaStat / 100.0f : 1.0f;
    float speedScale = (speedStat > 0.1f) ? speedStat / 100.0f : 1.0f;

    // Clamp scales to sane values to prevent "all screen" or "frozen"
    // projectiles
    areaScale = std::clamp(areaScale, 0.1f, 5.0f);
    speedScale = std::clamp(speedScale, 0.1f, 10.0f);

    baseRadius *= areaScale;
    baseSpeed *= speedScale;
    if (baseSpeed < 1.0f)
      baseSpeed = 1.0f; // Prevent division by zero

    // Spirit Sword adjustment
    if (registry.any_of<SpiritSwordTag>(owner)) {
      baseRadius *= 0.5f;
      baseLifetime *= 0.75f;
      LOG_INFO(
          "Spirit Sword Rending Wave: Radius halved, Lifetime reduced to 75%.");
    }

    Vector2 baseDir =
        Vector2Normalize(Vector2Subtract(exec.target_pos, {pos->x, pos->y}));
    TryPlayRendingWaveSequence(registry, owner, "LightningStrike",
                               exec.target_pos);

    // --- TALENT BRANCH LOGIC ---
    int extraWaves = 0;
    float damagePenalty = 1.0f;
    bool boomerang = false;
    bool isVoid = false;
    ElementalConversion elementalConv;
    const Tag heavenlyAttunementTag =
        systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);
    bool splitOnDeath = false;
    bool explodeOnHit = false;
    bool hoverAtApex = false;
    float talentWidthBonus = 0.0f;
    if (currentSwordFlow > 0) {
      talentWidthBonus += static_cast<float>(currentSwordFlow) * 1.5f;
      if (currentSwordFlow >= 5) {
        extraWaves += 1;
      }
      if (currentSwordFlow >= 8) {
        extraWaves += 1;
      }
      if (currentSwordFlow >= 10) {
        extraWaves += 1;
      }
    }

    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          // Talent: Jian Qi Zong Heng (鍓戞皵绾垫í) - ID 200
          if (spec.allocated_points.contains(RendingWaveNodes::SwordQi)) {
            int pts = spec.allocated_points.at(RendingWaveNodes::SwordQi);
            baseRadius *= (1.0f + pts * 0.1f);
            talentWidthBonus = pts * 10.0f;
          }

          // Talent: Duo Zhong Jian Qi (澶氶噸鍓戞皵) - ID 210
          if (spec.allocated_points.contains(RendingWaveNodes::MultiWave)) {
            int pts = spec.allocated_points.at(RendingWaveNodes::MultiWave);
            extraWaves = pts;
            static constexpr float penalties[] = {1.0f, 0.75f, 0.8f, 0.85f};
            damagePenalty = penalties[std::min(pts, 3)];
          }

          // Talent: Sui Lie Zhi Ren (纰庤涔嬪垉) - ID 211
          if (spec.allocated_points.contains(RendingWaveNodes::SplitBlade)) {
            splitOnDeath = true;
          }

          // Talent: Wan Jian Gui Zong (涓囧墤褰掑畻) - ID 213
          if (spec.allocated_points.contains(RendingWaveNodes::BladeBurst)) {
            explodeOnHit = true;
          }

          // Talent: Hui Xuan Jin (鍥炴棆鍔? - ID 230
          if (spec.allocated_points.contains(RendingWaveNodes::Boomerang)) {
            boomerang = true;
          }

          // Talent: Shi Kong Suo Ding (鏃剁┖閿佸畾) - ID 233
          if (spec.allocated_points.contains(RendingWaveNodes::TimeLock)) {
            hoverAtApex = true;
          }

          // Talent: Ling Li Zhuan Hua (鐏靛姏杞寲) - ID 250
          if (spec.allocated_points.contains(RendingWaveNodes::VoidConvert) &&
              spec.allocated_points.at(RendingWaveNodes::VoidConvert) > 0) {
            isVoid = true;
          }

          // Talent: Element Form (鍏冪礌褰㈡€? - ID 270
          if (spec.allocated_points.contains(RendingWaveNodes::ElementForm) &&
              spec.allocated_points.at(RendingWaveNodes::ElementForm) > 0) {
            elementalConv = ResolveElementalConversion(
                RendingWaveNodes::ElementForm,
                spec.allocated_points.at(RendingWaveNodes::ElementForm));
          }

          // Talent: Jian Yi Bao Fa (鍓戞剰鐖嗗彂) - ID 252
          if (spec.allocated_points.contains(RendingWaveNodes::IntentBurst) &&
              spec.allocated_points.at(RendingWaveNodes::IntentBurst) > 0) {
            int spendAmount = SkillConstants::DEFAULT_MAX_SWORD_INTENT;
            if (const auto *resource =
                    registry.try_get<BladeResourceComponent>(owner);
                resource != nullptr &&
                resource->kind == BladeResourceKind::SwordFlow) {
              spendAmount = resource->current;
            }
            if (spendAmount > 0 &&
                SkillSystem::ConsumeSwordIntent(registry, owner, spendAmount,
                                                kSkillId)) {
              exec.is_empowered = true;
              RefundFlowingThrustCooldown(registry, owner, 1, 1.5f);
              if (spendAmount >= SkillConstants::DEFAULT_MAX_SWORD_INTENT) {
                ResetSkillCooldown(registry, owner, 1);
              }
              LOG_INFO("Sword Intent Burst (252) triggered for Rending Wave!");
            }
          }
          break;
        }
      }
    }

    if (isVoid && elementalConv.IsActive()) {
      LOG_WARN("Rending Wave: VoidConvert (250) and ElementForm (270) are both "
               "active, ElementForm will be ignored.");
      elementalConv = {};
    }

    // Visual fallback for proxy casters (e.g. spirit swords): when local talent
    // points do not provide conversion info, infer color from inherited
    // Physical->X Convert modifiers on the owner.
    bool visualIsVoid = isVoid;
    ElementalConversion visualElementalConv = elementalConv;
    if (!visualIsVoid && !visualElementalConv.IsActive() &&
        heavenlyAttunementTag != Tag::None) {
      visualElementalConv =
          ResolveRendingWaveHeavenlyAttunementConversion(heavenlyAttunementTag);
    }
    if (!visualIsVoid && !visualElementalConv.IsActive()) {
      if (const auto *ownerMods =
              registry.try_get<SkillModifierComponent>(owner)) {
        for (const auto &mod : ownerMods->damage_modifiers) {
          if (mod.type != ModifierType::Convert ||
              mod.source_tag != Tag::Physical || mod.value <= 0.0f) {
            continue;
          }

          if (mod.target_tag == Tag::Void) {
            visualIsVoid = true;
            break;
          }
          if (mod.target_tag == Tag::Fire) {
            visualElementalConv = ResolveElementalConversion(
                RendingWaveNodes::ElementForm, 1);
            break;
          }
          if (mod.target_tag == Tag::Cold) {
            visualElementalConv = ResolveElementalConversion(
                RendingWaveNodes::ElementForm, 2);
            break;
          }
          if (mod.target_tag == Tag::Lightning) {
            visualElementalConv = ResolveElementalConversion(
                RendingWaveNodes::ElementForm, 3);
            break;
          }
        }
      }
    }

    int totalCount = (int)StatsSystem::GetStatWithTags(
        registry, owner, StatType::ProjectileCount, skillTags, exec.skill_id);
    if (totalCount < 1)
      totalCount = 1;
    totalCount += extraWaves;

    if (exec.is_empowered) {
      totalCount *= 2;
      LOG_INFO("Empowered Rending Wave: Double projectiles!");
      if (!TryPlayRendingWaveSequence(registry, owner, "FireExplosion",
                                      exec.target_pos)) {
        RenderSystem::AddScreenShake(0.2f);
      }
    }

    float spread = 0.4f + (totalCount * 0.05f);
    float startAngle = (totalCount > 1) ? -spread / 2.0f : 0.0f;
    float angleStep = totalCount > 1 ? spread / (totalCount - 1) : 0.0f;

    // Fix UAF: Copy component data to local variables before creating new
    // entities
    Position ownerPos = *pos;
    CombatStats ownerStats = *stats;
    for (auto &mult : ownerStats.damage_multipliers) {
      mult *= sevenStarLink.damage_multiplier;
    }

    for (int i = 0; i < totalCount; ++i) {
      float angle = startAngle + i * angleStep;
      Vector2 dir = Vector2Rotate(baseDir, angle);

      // --- VISUAL EFFECTS ---
      auto &particleSys = systems::GPUParticleSystem::Get();
      Color coreColor =
          exec.is_empowered
              ? systems::InkEffectHelper::COLOR_GOLD_CORE
              : (visualIsVoid ? PURPLE
                        : (visualElementalConv.IsActive()
                               ? visualElementalConv.projectile_color
                               : systems::InkEffectHelper::COLOR_SWORD_QI));
      Color glowColor =
          exec.is_empowered
              ? systems::InkEffectHelper::COLOR_GOLD_GLOW
              : (visualIsVoid ? PURPLE
                        : (visualElementalConv.IsActive()
                               ? visualElementalConv.glow_color
                               : systems::InkEffectHelper::COLOR_FROST_LIGHT));
      coreColor.a = std::min<unsigned char>(coreColor.a, 170);
      glowColor.a = std::min<unsigned char>(glowColor.a, 85);

      // Optimization: Use thread-local buffer to avoid heap allocation per
      // projectile
      static thread_local std::vector<components::GPUParticle> s_trailBuffer;
      s_trailBuffer.clear();
      s_trailBuffer.reserve(32);

      systems::InkEffectHelper::AppendProjectileTrail(
          s_trailBuffer, {ownerPos.x, ownerPos.y}, dir, coreColor, glowColor,
          8.0f, 2);
      particleSys.EmitBatch(s_trailBuffer);

      auto proj_ent = registry.create();
      registry.emplace<LocalLevelTag>(proj_ent);
      float forwardOffset = baseRadius * 0.6f;
      Vector2 spawnPos = {ownerPos.x + dir.x * forwardOffset,
                          ownerPos.y + dir.y * forwardOffset};

      registry.emplace<Position>(proj_ent, spawnPos.x, spawnPos.y);
      registry.emplace<Velocity>(proj_ent, dir.x * baseSpeed,
                                 dir.y * baseSpeed);
      registry.emplace<ColorComponent>(
          proj_ent, visualIsVoid ? PURPLE
                           : (visualElementalConv.IsActive()
                                  ? visualElementalConv.projectile_color
                                  : (exec.is_empowered
                                         ? GOLD
                                         : NoMoreDay::components::Colors::BLADE_CYAN)));

      auto &proj = registry.emplace<Projectile>(proj_ent);
      proj.owner = owner;
      proj.cast_id = exec.cast_id;
      proj.speed = baseSpeed;
      proj.lifeTime = (boomerang && !hoverAtApex)
                          ? (400.0f / baseSpeed) * 2.0f + 0.5f
                          : baseLifetime;

      if (hoverAtApex) {
        proj.lifeTime = 400.0f / baseSpeed; // Expire at apex to trigger Hover
      }

      proj.radius = exec.is_empowered ? baseRadius * 1.7f : baseRadius;
      proj.arcWidth = 60.0f + talentWidthBonus;
      if (exec.is_empowered)
        proj.arcWidth *= 1.3f;

      proj.visualType = 3; // Crescent Wave (Moon)

      proj.pierce = true;
      proj.pierceCount = 99;
      proj.snapshot = ownerStats;

      // Apply Behavior Flags
      if (splitOnDeath) {
        proj.on_death = Projectile::OnDeathBehavior::Split;
        proj.split_count = 3;
        proj.split_damage_mult = 0.5f;
        proj.split_speed_mult = 0.8f;
        proj.split_radius_mult = 0.6f;
      }
      if (explodeOnHit) {
        proj.on_death = Projectile::OnDeathBehavior::Explode;
        proj.pierce = false;
        proj.pierceCount = 0;
      }
      if (hoverAtApex) {
        proj.on_death = Projectile::OnDeathBehavior::Hover;
        // Hover overrides others usually
      }

      // Apply Penalties and Empowerment
      for (auto &mult : proj.snapshot.damage_multipliers) {
        mult *= damagePenalty;
        if (exec.is_empowered)
          mult *= 2.0f;
      }

      if (exec.is_empowered) {
        proj.snapshot.crit_chance += 100.0f;
        proj.snapshot.crit_damage += 1.0f;
      }

      registry.emplace<CombatStats>(proj_ent, proj.snapshot);
      registry.emplace<SkillComponent>(proj_ent, exec.skill_id, owner);

      SkillModifierComponent *projMods = nullptr;
      auto ensureProjMods = [&]() -> SkillModifierComponent & {
        if (!projMods) {
          projMods = &registry.emplace<SkillModifierComponent>(proj_ent);
        }
        return *projMods;
      };

      if (auto *ownerMods = registry.try_get<SkillModifierComponent>(owner)) {
        auto &mods = ensureProjMods();
        mods.damage_modifiers.insert(mods.damage_modifiers.end(),
                                     ownerMods->damage_modifiers.begin(),
                                     ownerMods->damage_modifiers.end());
      }

      if (isVoid) {
        auto &mods = ensureProjMods();
        mods.damage_modifiers.push_back(
            DamageModifier{Tag::Physical, Tag::Void, 1.0f,
                           ModifierType::Convert});
      } else if (elementalConv.IsActive()) {
        auto &mods = ensureProjMods();
        mods.damage_modifiers.push_back(
            DamageModifier{Tag::Physical, elementalConv.target_element, 1.0f,
                           ModifierType::Convert});
      } else if (heavenlyAttunementTag != Tag::None) {
        auto &mods = ensureProjMods();
        mods.damage_modifiers.push_back(
            DamageModifier{Tag::Physical, heavenlyAttunementTag, 0.5f,
                           ModifierType::Convert});
      }

      if (boomerang && !hoverAtApex) {
        auto &bc = registry.emplace<BoomerangComponent>(proj_ent);
        bc.owner = owner;
        bc.returnTimer = 400.0f / baseSpeed;
        bc.phase = BoomerangComponent::Outward;
        bc.returnSpeed = proj.speed * 1.2f;
      }
    }

    LOG_INFO("Rending Wave fired {} projectiles from entity {}", totalCount,
             (uint32_t)owner);
  }

  static void DoHit(entt::registry &registry, entt::entity attacker,
                    entt::entity target, Tag hit_tags, bool is_crit) {
    if (auto *active = registry.try_get<ActiveSkillsComponent>(attacker)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          // Talent: Jian Yi Ji Qu (鍓戞剰姹插彇) - ID 253
          if (spec.allocated_points.contains(RendingWaveNodes::IntentGain)) {
            int pts = spec.allocated_points.at(RendingWaveNodes::IntentGain);
            if (pts > 0 && GetRandomValue(0, 100) < 10 * pts) {
              SkillSystem::GainSwordIntent(registry, attacker, 1, kSkillId);
              LOG_DEBUG("Rending Wave (253): Gained Intent via Hit");
            }
          }
          break;
        }
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(RendingWave)

void RegisterRendingWave() {}

} // namespace NoMoreDay::skills
