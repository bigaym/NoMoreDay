/**
 * @file BladeFormation.cpp
 * @brief 灵剑决 (ID 3) - 召唤灵剑技能行为实现
 *
 * 召唤围绕玩家盘旋的灵剑自动攻击敌人。
 *
 * 天赋分支:
 * - 300 多重灵剑: 增加灵剑数量
 * - 301 疾风剑意: 提高攻击频率
 * - 302 索敌范围: 扩大搜索半径
 * - 310 归一: 合并为巨剑
 * - 311 天崩地裂: 暴击震波
 * - 321 气劲回流: 命中回蓝
 * - 322 不灭剑魂: 免死一次
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSystem.hpp"

#include "engine/render/GPUParticleSystem.hpp"
#include "game/systems/combat/CombatSystem.hpp"

#include "core/logging/Logger.hpp"

namespace NoMoreDay::skills {

struct BladeFormation : SkillBehaviorBase<BladeFormation> {
  static constexpr uint32_t kSkillId = 3;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    auto &formation = registry.get_or_emplace<BladeFormationComponent>(owner);

    int extraSwords = 0;
    float freqInc = 0.0f;
    float searchInc = 0.0f;

    if (auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
      for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == 3) {
          if (spec.allocated_points.contains(300))
            extraSwords = spec.allocated_points.at(300);
          if (spec.allocated_points.contains(301))
            freqInc = spec.allocated_points.at(301) * 0.1f;
          // Tag 310 in JSON is actually "Range" (索敌范围), unlike "Giant
          // Sword" (330)
          if (spec.allocated_points.contains(310))
            searchInc = spec.allocated_points.at(310) * 0.2f;
          break;
        }
      }
    }

    // Talent Flags using active_nodes
    if (exec.active_nodes.test(330 % 100))
      formation.has_giant_sword = true; // Giant Sword
    if (exec.active_nodes.test(311 % 100))
      formation.shockwave_on_crit = true;
    if (exec.active_nodes.test(321 % 100))
      formation.mana_on_hit = true;

    // Immortality (353)
    if (exec.active_nodes.test(353 % 100)) {
      formation.immortality_ready = true;
      LOG_INFO("Blade Formation (353): Immortality Shield Ready");
    }

    formation.max_swords = 1 + extraSwords;
    if (formation.has_giant_sword) {
      formation.max_swords = 1;      // Limit to 1
      formation.is_empowered = true; // Visual indicator for Giant Sword
      LOG_INFO("Blade Formation (330): Giant Sword Activated");
    }

    formation.attack_interval = 1.0f / (1.0f + freqInc);
    if (formation.has_giant_sword)
      formation.attack_interval *= 2.0f; // Slower attack

    formation.search_radius = 200.0f * (1.0f + searchInc);
    formation.is_empowered =
        exec.is_empowered; // Keep original empowered state if not giant sword

    // --- MANAGE SPIRIT SWORDS ---
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
      uint32_t skillIcon = 3687043718;

      int needed = formation.max_swords - current_count;
      for (int i = 0; i < needed; ++i) {
        auto sword = registry.create();
        registry.emplace<LocalLevelTag>(sword);
        registry.emplace<Position>(sword, pos ? *pos : Position{0, 0});
        registry.emplace<Velocity>(sword, 0.0f, 0.0f);

        auto &summon = registry.emplace<SummonComponent>(sword);
        summon.owner = owner;
        summon.skill_id = 3;
        summon.lifetime = 10.0f;
        summon.max_lifetime = 10.0f;
        summon.icon_id = skillIcon;
        summon.name = "灵剑";

        registry.emplace<SpiritSwordTag>(sword);

        auto &ai = registry.emplace<SpiritSwordAI>(sword);
        ai.attack_interval = formation.attack_interval;

        int total_index = current_count + i;
        ai.attack_timer =
            (float)total_index * (ai.attack_interval / formation.max_swords);
        ai.orbit_angle = (float)total_index / formation.max_swords * 2.0f * PI;
      }
      LOG_INFO("Blade Formation: Spawned {} new swords.", needed);
    } else {
      LOG_INFO("Blade Formation: Refreshed {} swords.", current_count);
    }

    formation.current_swords = formation.max_swords; // Sync component state
  }

  static void DoHit(entt::registry &registry, entt::entity attacker,
                    entt::entity target, Tag hit_tags, bool is_crit) {
    auto *formation = registry.try_get<BladeFormationComponent>(attacker);
    if (!formation)
      return;

    // Node 311: Shockwave
    if (is_crit && formation->shockwave_on_crit) {
      if (registry.all_of<Position>(target)) {
        const auto &tPos = registry.get<Position>(target);
        auto &particleSys = systems::GPUParticleSystem::Get();
        auto splash = systems::InkEffectHelper::CreateInkSplash(
            {tPos.x, tPos.y}, 12, 10.0f, 150.0f);
        for (auto &p : splash) {
          p.color = systems::InkEffectHelper::COLOR_GOLD_CORE;
          particleSys.Emit(p);
        }
        // Apply direct damage
        CombatSystem::ApplyDamage(registry, target, 15.0f, attacker, false,
                                  true);
        LOG_INFO("Blade Formation (311): Shockwave triggered");
      }
    }

    // Node 321: Mana on Hit
    if (formation->mana_on_hit) {
      if (auto *stats = registry.try_get<CombatStats>(attacker)) {
        stats->mana = std::min(stats->max_mana, stats->mana + 2.0f);
        LOG_DEBUG("Blade Formation (321): Restored 2 mana");
      }
    }
  }
};

REGISTER_SKILL_BEHAVIOR(BladeFormation)

void RegisterBladeFormation() {}

} // namespace NoMoreDay::skills
