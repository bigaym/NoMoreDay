/**
 * @file BladeFormation.cpp
 * @brief 灵剑决 (ID 3) - 召唤灵剑技能行为实现
 *
 * 召唤围绕玩家盘旋的灵剑自动攻击敌人。
 *
 * 天赋分支:
 * - 300 剑池: 增加灵剑数量
 * - 301 疾风意: 提高攻击频率
 * - 310 索敌范围: 扩大搜索半径
 * - 311 无尽剑匣: 灵剑数量翻倍, 单发伤害降低
 * - 330 巨剑降临: 合并为巨剑
 * - 351 气劲回流: 命中回蓝
 * - 353 不灭剑魂: 免死一次
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/foundation/SharedContext.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/vfx/HoloBladeComponent.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSystem.hpp"

using namespace entt::literals;

#include "core/logging/Logger.hpp"
#include <algorithm>

namespace NoMoreDay::skills {

namespace BladeFormationNodes {
// 基础分支 / Base
constexpr uint32_t SwordPool = 300;      // 剑池 / Sword Pool
constexpr uint32_t SwiftIntent = 301;    // 疾风意 / Swift Intent

// 索敌分支 / Tracking branch
constexpr uint32_t SearchRange = 310;    // 索敌范围 / Search Range
constexpr uint32_t InfiniteSheath = 311; // 无尽剑匣 / Infinite Sheath
constexpr uint32_t SpiritInfusion = 312; // 灵力灌注 / Spirit Infusion
constexpr uint32_t Godspeed = 313;       // 神速 / Godspeed

// 巨剑分支 / Giant sword branch
constexpr uint32_t GiantSword = 330;     // 巨剑降临 / Giant Sword
constexpr uint32_t WeakpointLock = 331;  // 弱点锁定 / Weakpoint Lock
constexpr uint32_t DeadlyEdge = 332;     // 致命锋芒 / Deadly Edge
constexpr uint32_t SwordPressure = 333;  // 剑压 / Sword Pressure

// 防御分支 / Defense branch
constexpr uint32_t SpiritGuard = 350;    // 灵剑护体 / Spirit Guard
constexpr uint32_t QiReflow = 351;       // 气劲回流 / Qi Reflow
constexpr uint32_t ShadowTrack = 352;    // 剑影随行 / Shadow Track
constexpr uint32_t Immortality = 353;    // 不灭剑魂 / Immortality

// 元素分支 / Element branch
constexpr uint32_t ElementEnchant = 370;  // 元素附魔 / Element Enchant
constexpr uint32_t SpiritCharge = 371;    // 灵剑充能 / Spirit Charge
constexpr uint32_t SpellResonance = 372;  // 法术共鸣 / Spell Resonance
constexpr uint32_t AllBladesReturn = 373; // 万剑归宗 / All Blades Return
} // namespace BladeFormationNodes

namespace {

ElementalConversion ResolveBladeFormationHeavenlyAttunementConversion(const Tag tag) {
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

} // namespace

struct BladeFormation : SkillBehaviorBase<BladeFormation> {
  static constexpr uint32_t kSkillId = 3;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    auto &formation = registry.get_or_emplace<BladeFormationComponent>(owner);

    int extraSwords = 0;
    float freqInc = 0.0f;
    float searchInc = 0.0f;
    int spiritChargePts = 0;
    ElementalConversion swordElementConv;
    const Tag heavenlyAttunementTag =
        systems::BladeResourceService::GetHeavenlyAttunementElementTag(registry, owner);

    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kSkillId) {
          if (spec.allocated_points.contains(BladeFormationNodes::SwordPool))
            extraSwords = spec.allocated_points.at(BladeFormationNodes::SwordPool);
          if (spec.allocated_points.contains(BladeFormationNodes::SwiftIntent))
            freqInc = spec.allocated_points.at(BladeFormationNodes::SwiftIntent) * 0.1f;
          if (spec.allocated_points.contains(BladeFormationNodes::SearchRange))
            searchInc = spec.allocated_points.at(BladeFormationNodes::SearchRange) * 0.2f;
          if (spec.allocated_points.contains(BladeFormationNodes::ElementEnchant) &&
              spec.allocated_points.at(BladeFormationNodes::ElementEnchant) > 0) {
            swordElementConv = ResolveElementalConversion(
                BladeFormationNodes::ElementEnchant,
                spec.allocated_points.at(BladeFormationNodes::ElementEnchant));
          }
          if (spec.allocated_points.contains(BladeFormationNodes::SpiritCharge)) {
            spiritChargePts =
                std::max(0, spec.allocated_points.at(BladeFormationNodes::SpiritCharge));
          }
          break;
        }
      }
    }

    // Reset transient flags for each cast.
    formation.has_giant_sword = false;
    formation.mana_on_hit = false;
    formation.melee_orbit = false;
    formation.immortality_ready = false;
    formation.damage_penalty = 1.0f;

    // Talent Flags using active_nodes
    if (exec.active_nodes.test(BladeFormationNodes::GiantSword % 100))
      formation.has_giant_sword = true; // Giant Sword
    if (exec.active_nodes.test(BladeFormationNodes::QiReflow % 100))
      formation.mana_on_hit = true;
    if (exec.active_nodes.test(BladeFormationNodes::ShadowTrack % 100))
      formation.melee_orbit =
          true; // Talent 352: Melee Orbit (Sword Shadow Tracking)

    // Immortality (353)
    if (exec.active_nodes.test(BladeFormationNodes::Immortality % 100)) {
      formation.immortality_ready = true;
      LOG_INFO("Blade Formation (353): Immortality Shield Ready");
    }
    if (!exec.active_nodes.test(BladeFormationNodes::ElementEnchant % 100)) {
      swordElementConv = {};
    }
    if (!swordElementConv.IsActive() && heavenlyAttunementTag != Tag::None) {
      swordElementConv =
          ResolveBladeFormationHeavenlyAttunementConversion(heavenlyAttunementTag);
    }

    formation.max_swords = 1 + extraSwords;
    if (exec.active_nodes.test(BladeFormationNodes::InfiniteSheath % 100)) {
      formation.max_swords *= 2;
      formation.damage_penalty = 0.6f; // 无尽剑匣: 单发伤害 -40%
      LOG_INFO("Blade Formation (311): Infinite Sheath activated");
    }
    if (formation.has_giant_sword) {
      formation.max_swords = 1;      // Limit to 1
      formation.is_empowered = true; // Visual indicator for Giant Sword
      LOG_INFO("Blade Formation (330): Giant Sword Activated");
    }

    formation.attack_interval = 1.0f / (1.0f + freqInc);
    if (formation.has_giant_sword)
      formation.attack_interval *= 2.0f; // Slower attack
    if (spiritChargePts > 0 && swordElementConv.IsActive()) {
      const float haste = 1.0f - 0.08f * static_cast<float>(spiritChargePts);
      formation.attack_interval *= (haste < 0.55f ? 0.55f : haste);
      formation.damage_penalty *= 1.0f + 0.05f * static_cast<float>(spiritChargePts);
    }

    formation.search_radius = 200.0f * (1.0f + searchInc);
    formation.is_empowered =
        exec.is_empowered; // Keep original empowered state if not giant sword

    // --- MANAGE SPIRIT SWORDS ---
    auto applySwordEnchant = [&](entt::entity sword) {
      Color swordColor = swordElementConv.IsActive()
                             ? swordElementConv.projectile_color
                             : (formation.is_empowered ? GOLD
                                                       : Color{150, 220, 255, 220});
      swordColor.a = formation.is_empowered ? 255 : 220;

      if (auto *holo = registry.try_get<components::HoloBlade>(sword)) {
        holo->holoColor = swordColor;
      }
      registry.emplace_or_replace<ColorComponent>(sword, swordColor);

       if (swordElementConv.IsActive()) {
         SkillModifierComponent mods;
         mods.damage_modifiers.push_back(
             DamageModifier{Tag::Physical, swordElementConv.target_element,
                            heavenlyAttunementTag != Tag::None ? 0.5f : 1.0f,
                            ModifierType::Convert});
         registry.emplace_or_replace<SkillModifierComponent>(
             sword, std::move(mods));
       } else {
        registry.remove<SkillModifierComponent>(sword);
      }
    };

    const auto configureSummonProfiles = [&](entt::entity sword,
                                             bool refreshSnapshot) {
      auto &summon = registry.get<SummonComponent>(sword);
      summon.owner = owner;
      summon.skill_id = kSkillId;
      summon.archetype_id = SummonArchetype::SpiritSword;
      summon.icon_id = entt::hashed_string{"vfx_spirit_sword"};

      auto &combat = registry.get_or_emplace<SummonCombatProfile>(sword);
      combat.damage_scale =
          (formation.has_giant_sword ? 1.5f : 0.5f) * formation.damage_penalty;
      combat.inherit_mode = SummonInheritMode::Mixed;
      combat.proc_budget_per_second = formation.melee_orbit ? 7.5f : 3.0f;
      combat.proc_budget_cap = formation.melee_orbit ? 10.0f : 6.0f;
      combat.melee_orbit_hit_radius = 30.0f;
      combat.melee_orbit_base_damage = formation.has_giant_sword ? 35.0f : 25.0f;

      auto &aiProfile = registry.get_or_emplace<SummonAIProfile>(sword);
      aiProfile.role =
          formation.melee_orbit ? SummonRole::Melee : SummonRole::Orbit;
      aiProfile.command_mode = formation.mode == SpiritSwordMode::Elite
                                   ? SummonCommandMode::Aggressive
                                   : SummonCommandMode::Assist;
      aiProfile.retarget_interval = 0.2f;
      aiProfile.leash_radius = formation.search_radius;

      auto &runtime = registry.get_or_emplace<SummonRuntimeState>(sword);
      if (refreshSnapshot || !runtime.has_snapshot) {
        if (const auto *ownerStats = registry.try_get<CombatStats>(owner)) {
          runtime.snapshot_stats = *ownerStats;
          runtime.has_snapshot = true;
        }
      }
      runtime.proc_budget = combat.proc_budget_cap;

      auto &attribution =
          registry.get_or_emplace<SummonAttributionContext>(sword);
      attribution.owner = owner;
      attribution.summon = sword;
      attribution.source_skill_id = kSkillId;
    };

    std::vector<entt::entity> existing_swords;
    auto view = registry.view<SpiritSwordTag, SummonComponent>();
    for (auto entity : view) {
      if (view.get<SummonComponent>(entity).owner == owner) {
        existing_swords.push_back(entity);
      }
    }

    // Refresh existing
    for (auto entity : existing_swords) {
      auto &s = registry.get<SummonComponent>(entity);
      s.lifetime = s.max_lifetime;
      configureSummonProfiles(entity, true);
      auto &ai = registry.get_or_emplace<SpiritSwordAI>(entity);
      ai.attack_interval = formation.attack_interval;
      applySwordEnchant(entity);
    }

    int current_count = (int)existing_swords.size();

    if (current_count > formation.max_swords) {
      for (int i = formation.max_swords; i < current_count; ++i) {
        registry.destroy(existing_swords[i]);
      }
      LOG_INFO("Blade Formation: Removed {} excess swords.",
               current_count - formation.max_swords);
    } else if (current_count < formation.max_swords) {
      auto *pos = registry.try_get<Position>(owner);
      uint32_t skillIcon = entt::hashed_string{"ui_skill_wanjianjue"};

      int needed = formation.max_swords - current_count;
      LOG_INFO("Blade Formation: Spawning {} new swords (Total max: {})",
               needed, formation.max_swords);

      for (int i = 0; i < needed; ++i) {
        auto sword = registry.create();
        registry.emplace<LocalLevelTag>(sword);
        registry.emplace<Position>(sword, pos ? *pos : Position{0, 0});
        registry.emplace<Velocity>(sword, 0.0f, 0.0f);

        auto &summon = registry.emplace<SummonComponent>(sword);
        summon.owner = owner;
        summon.skill_id = kSkillId;
        summon.lifetime = 10.0f;
        summon.max_lifetime = 10.0f;
        summon.icon_id = skillIcon;
        summon.archetype_id = SummonArchetype::SpiritSword;

        registry.emplace<SpiritSwordTag>(sword);

        auto &ai = registry.emplace<SpiritSwordAI>(sword);
        ai.attack_interval = formation.attack_interval;

        // Visuals
        auto &holo = registry.emplace<components::HoloBlade>(sword);
        holo.holoColor =
            formation.is_empowered ? GOLD : Color{150, 220, 255, 220};
        holo.scale = formation.has_giant_sword ? 0.5f : 0.18f;

        // Add Sprite for Holo Shader
        Texture2D swordTex = NoMoreDay::AssetLoadingSystem::GetTexture(
            entt::hashed_string{"vfx_spirit_sword"});
        registry.emplace<SpriteComponent>(sword, swordTex, 0.5f);

        // Use sword icon for the summon status
        summon.icon_id = entt::hashed_string{"vfx_spirit_sword"};
        configureSummonProfiles(sword, true);

        int total_index = current_count + i;
        ai.attack_timer =
            (float)total_index * (ai.attack_interval / formation.max_swords);
        ai.orbit_angle = (float)total_index / formation.max_swords * 2.0f * PI;

        applySwordEnchant(sword);
        CombatEventDispatcher::Dispatch(
            registry, CombatEventFactory::CreateOnSummon(owner, sword, kSkillId));
      }
      LOG_INFO("Blade Formation: Spawned {} new swords.", needed);
    } else {
      LOG_INFO("Blade Formation: Refreshed {} swords.", current_count);
    }

    formation.current_swords = formation.max_swords; // Sync component state
  }

  static void DoHit(entt::registry &registry, entt::entity attacker,
                    entt::entity target, Tag hit_tags, bool is_crit) {
    (void)target;
    (void)hit_tags;
    (void)is_crit;
    auto *formation = registry.try_get<BladeFormationComponent>(attacker);
    if (!formation)
      return;

    // Node 351: Mana on Hit (Qi Reflow)
    if (formation->mana_on_hit) {
      if (auto *stats = registry.try_get<CombatStats>(attacker)) {
        stats->mana = std::min(stats->max_mana, stats->mana + 2.0f);
        LOG_DEBUG("Blade Formation (351): Restored 2 mana");
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(BladeFormation)

void RegisterBladeFormation() {}

} // namespace NoMoreDay::skills
