/**
 * @file BladeWard.cpp
 * @brief 剑气护体 (ID 4) - 防御技能行为实现
 */

#include "SkillBehaviorBase.hpp"
#include "SkillBehaviorRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "game/components/Buff.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay::skills {

struct BladeWard : SkillBehaviorBase<BladeWard> {
  static constexpr uint32_t kSkillId = 4;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec) {
    auto &active_effects =
        registry.get_or_emplace<ActiveEffectsComponent>(owner);

    float phys_dr = 10.0f;

    float elemental_res = 0.0f;
    float block_inc = 0.0f;

    // Check for specialized skill points (talents)
    if (exec.active_nodes.test(
            400 % 100)) { // Talent 400: Increased Physical Damage Reduction
      // Assuming 400 is a binary node for now, or we'd need to query levels
      // from somewhere else
      phys_dr += 5.0f; // Example value, adjust if talent has levels
    }
    if (exec.active_nodes.test(
            401 % 100)) {    // Talent 401: Increased Elemental Resistance
      elemental_res += 3.0f; // Example value
    }
    if (exec.active_nodes.test(410 %
                               100)) { // Talent 410: Increased Block Chance
      block_inc += 5.0f;               // Example value
    }

    BuffEffect ward_buff;
    ward_buff.id = "blade_ward";
    ward_buff.name = "Blade Ward";
    ward_buff.type = BuffType::Shield;
    ward_buff.duration = 10.0f;
    ward_buff.remaining = 10.0f;

    ward_buff.modifiers.push_back({.value = phys_dr,
                                   .type = StatType::ResistPhysical,
                                   .mode = ModifierMode::Flat});

    // Get the ActiveSkillsComponent to check specialized slots
    auto active = registry.try_get<ActiveSkillsComponent>(owner);
    if (active) {
      // 210: Elemental Resistance
      if (active->specialized_slots[0].allocated_points.contains(210)) {
        float elemental_res = 15.0f;
        ward_buff.modifiers.push_back({.value = elemental_res,
                                       .type = StatType::ResistAll,
                                       .mode = ModifierMode::Flat});
      }

      // 220: Block Chance
      if (active->specialized_slots[0].allocated_points.contains(220)) {
        float block_inc = 10.0f;
        ward_buff.modifiers.push_back({.value = block_inc,
                                       .type = StatType::BlockChance,
                                       .mode = ModifierMode::Flat});
      }
    }

    registry.get_or_emplace<ActiveEffectsComponent>(owner).AddOrRefresh(
        ward_buff);

    // BladeWardComponent Logic
    auto &ward = registry.get_or_emplace<BladeWardComponent>(owner);
    ward.sword_count = 3;
    ward.interception_chance = 0.3f;
    ward.is_solidified = false;

    // Talent 401/470: Interception/Counter
    // Using 401 (Interception Chance Increase) as verified in skills.json
    if (exec.active_nodes.test(
            420 % 100)) { // Talent 420: Increased Interception Chance
      ward.interception_chance += 0.25f; // +25% chance
    }

    // Talent 470: Counter Shot
    if (exec.active_nodes.test(470 % 100)) {
      ward.trigger_counter = true;
    }

    // Talent 473: Blade Storm (Counter Spin)
    if (exec.active_nodes.test(473 % 100)) {
      ward.counter_spin = true;
    }

    // Talent 4xx: Solidified (Hypothetical ID 412 "Immovable")
    if (exec.active_nodes.test(412 %
                               100)) { // Talent 412: Solidified (Immovable)
      ward.is_solidified = true;
      LOG_INFO("Blade Ward (412): Solidified (Immovable)");
    }

    // VFX
    auto *pos = registry.try_get<Position>(owner);
    if (pos) {
      auto &particleSys = systems::GPUParticleSystem::Get();
      for (int s = 0; s < 3; ++s) {
        for (int i = 0; i < 20; ++i) {
          float t = (float)i / 20.0f;
          float angle = t * 4.0f * PI + (s * 2.0f * PI / 3);
          float height = t * 60.0f;
          float radius = 40.0f * (1.0f - t * 0.5f);

          Vector2 pPos = {pos->x + cosf(angle) * radius,
                          pos->y + sinf(angle) * radius - height + 30.0f};

          components::GPUParticle p;
          p.position = pPos;
          p.velocity = {0, -20.0f};
          p.acceleration = {0, 0};
          p.color = ColorAlpha(SKYBLUE, 0.5f);
          p.lifetime = 1.0f;
          p.maxLifetime = 1.0f;
          p.scale = 1.5f;
          p.flags = 13;
          particleSys.Emit(p);
        }
      }
    }

    if (exec.is_empowered) {
      ward.sword_count += 3;
      ward.interception_chance *= 2.0f;
      LOG_INFO("Empowered Blade Ward: +3 swords and 2x interception chance!");
    }

    registry.get_or_emplace<StatsDirty>(owner);
    LOG_INFO("Blade Ward activated for entity {}", (uint32_t)owner);
  }
};

REGISTER_SKILL_BEHAVIOR(BladeWard)

void RegisterBladeWard() {}

} // namespace NoMoreDay::skills
