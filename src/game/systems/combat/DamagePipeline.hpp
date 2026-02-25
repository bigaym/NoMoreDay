#pragma once
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include <entt/entt.hpp>
#include <taskflow/taskflow.hpp>
#include <vector>

namespace NoMoreDay {

struct DamageResult {
  float total_damage = 0.0f;
  bool is_crit = false;
  DamagePool final_pool; // Damage broken down by type
};

struct DamageRequest {
  entt::entity attacker = entt::null;
  entt::entity defender = entt::null;
  uint32_t skill_id = 0;
  DamagePool base_pool;
  Tag additional_tags = Tag::None;
  entt::entity source_entity = entt::null;
  bool is_simulation = false;
  bool skip_mitigation = false;
  bool thorns_like_damage = false;
};

class DamagePipeline {
public:
  static DamageResult Calculate(entt::registry &registry,
                                const DamageRequest &request);

  /**
   * @brief Executes the 5-step damage calculation.
   *
   * @param attacker The entity performing the attack
   * @param defender The entity receiving the damage
   * @param skill_id The ID of the skill being used (to look up base tags/mods)
   * @param base_pool The initial flat damage from the skill/weapon.
   * @param additional_tags Extra tags from the specific hit (e.g., Critical,
   * Hit)
   * @param source_entity The entity representing the skill execution (e.g.,
   * Projectile, Shadow)
   */
  static DamageResult Calculate(entt::registry &registry, entt::entity attacker,
                                entt::entity defender, uint32_t skill_id,
                                const DamagePool &base_pool,
                                Tag additional_tags = Tag::None,
                                entt::entity source_entity = entt::null,
                                bool is_simulation = false);

  /**
   * @brief Optimized batch calculation for many targets.
   */
  static void CalculateBatch(entt::registry &registry, entt::entity attacker,
                             const std::vector<entt::entity> &defenders,
                             uint32_t skill_id, const DamagePool &base_pool,
                             Tag additional_tags,
                             entt::entity source_entity = entt::null,
                             tf::Executor *executor = nullptr);

private:
  struct alignas(32) AttackerSnapshot {
    std::array<float, 6> base_damage = {0.0f}; // After Inc/More/Conversion
    float crit_chance = 0.0f;
    float crit_damage = 1.5f;
    float armor_pen = 0.0f;
    Tag hit_tags = Tag::None;
    float _padding[5] = {0.0f}; // Pad to 64 bytes (24+4+4+4+8 + 20 = 64)
  };
  static_assert(alignof(AttackerSnapshot) == 32,
                "AttackerSnapshot must be 32-byte aligned for SIMD");

  static AttackerSnapshot
  CreateSnapshot(entt::registry &registry, entt::entity attacker,
                 uint32_t skill_id, const DamagePool &base_pool, Tag hit_tags,
                 entt::entity source_entity);

  static DamagePool ApplyConversion(const DamagePool &pool,
                                    const std::vector<DamageModifier> &mods);

  // Step 3 & 4: Apply Inc and More
  static DamagePool ApplyMultipliers(const DamagePool &pool,
                                     const std::vector<DamageModifier> &mods,
                                     Tag hit_tags);

  // Step 5: Final settlement (Crit & Defense)
  static DamageResult Settle(const DamagePool &pool,
                             const CombatStats &attacker_stats,
                             const CombatStats &defender_stats, Tag hit_tags);
};

} // namespace NoMoreDay
