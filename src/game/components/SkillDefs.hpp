#pragma once
#include "game/data/BladeMasteryData.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/TagRegistry.hpp"
#include "raylib.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

inline constexpr uint32_t INVALID_SKILL_ID = 0xFFFFFFFFu;

/**
 * @brief Global skill system constants
 */
namespace SkillConstants {
static constexpr int DEFAULT_MAX_TALENT_POINTS = 20;
static constexpr int MAX_SKILL_SLOTS = 5;

// Sword Intent defaults
static constexpr int DEFAULT_MAX_SWORD_INTENT = 10;
static constexpr float SWORD_INTENT_GRACE_PERIOD = 5.0f;
static constexpr float SWORD_INTENT_DECAY_INTERVAL = 0.5f;

// Summon defaults
static constexpr float DEFAULT_SUMMON_LIFETIME = 10.0f;
} // namespace SkillConstants

/**
 * @brief DamagePool stores flat damage values for multiple types.
 * Indexed by the bit position of the DamageType tag (0-15).
 */
struct alignas(64) DamagePool {
  std::array<float, 16> values = {0.0f};

  void Clear() { values.fill(0.0f); }

  void Add(Tag type, float amount) {
    // Find bit index
    uint64_t val = static_cast<uint64_t>(type);
    if (val == 0)
      return;

    // __builtin_ctzll finds the number of trailing zeros, which is the bit
    // index for single bit tags
    int index = std::countr_zero(val);
    if (index < 16) {
      values[index] += amount;
    }
  }

  float Get(Tag type) const {
    uint64_t val = static_cast<uint64_t>(type);
    if (val == 0)
      return 0.0f;
    int index = std::countr_zero(val);
    if (index < 16) {
      return values[index];
    }
    return 0.0f;
  }

  void Merge(const DamagePool &other) {
    for (size_t i = 0; i < 16; ++i) {
      values[i] += other.values[i];
    }
  }
};
static_assert(alignof(DamagePool) == 64, "DamagePool must be 64-byte aligned");
static_assert(sizeof(DamagePool) == 64, "DamagePool size must be 64 bytes");

enum class ModifierType : uint8_t {
  Flat,      // Added to base damage
  Increased, // Summed together (1 + inc1 + inc2 + ...)
  More,      // Multiplied independently (* more1 * more2 * ...)
  Convert,   // Convert X% of A to B
  GainExtra, // Gain X% of A as extra B
};

inline void to_json(nlohmann::json &j, const ModifierType &e) {
  j = static_cast<uint8_t>(e);
}
inline void from_json(const nlohmann::json &j, ModifierType &e) {
  e = static_cast<ModifierType>(j.get<uint8_t>());
}

/**
 * @brief DamageModifier defines how a damage value is modified.
 */
struct DamageModifier {
  Tag source_tag =
      Tag::None; // Tag this modifier applies to (e.g., Physical, Melee)
  Tag target_tag = Tag::None; // Tag the output has (used for Convert/GainExtra)
  float value = 0.0f;
  ModifierType type = ModifierType::Flat;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DamageModifier, source_tag, target_tag,
                                   value, type)

struct TalentPrerequisite {
  uint32_t node_id = 0;
  int required_points = 1;
};

inline void to_json(nlohmann::json &j, const TalentPrerequisite &p) {
  j = nlohmann::json{{"node_id", p.node_id},
                     {"required_points", p.required_points}};
}

inline void from_json(const nlohmann::json &j, TalentPrerequisite &p) {
  if (j.is_number_unsigned()) {
    p.node_id = j.get<uint32_t>();
    p.required_points = 1;
    return;
  }
  if (j.is_number_integer()) {
    const int64_t raw = j.get<int64_t>();
    if (raw >= 0) {
      p.node_id = static_cast<uint32_t>(raw);
      p.required_points = 1;
      return;
    }
  }

  if (j.is_object()) {
    if (j.contains("node_id")) {
      j.at("node_id").get_to(p.node_id);
    }
    if (j.contains("required_points")) {
      j.at("required_points").get_to(p.required_points);
    }
  }

  if (p.required_points <= 0) {
    p.required_points = 1;
  }
}

/**
 * @brief Represents a node in a skill's specialization talent tree.
 */
struct TalentNode {
  uint32_t id = 0;
  std::string name_key;
  std::string desc_key;
  std::string behavior_id; // ID for C++ logic injection (e.g., "shadow_caster")
  uint32_t icon_id = 0;    // Added for UI Polish
  int max_points = 1;

  std::vector<TalentPrerequisite> prerequisites;
  std::vector<StatModifier> stat_modifiers;
  std::vector<DamageModifier> damage_modifiers;

  // Tag modification (e.g., add "spell" tag to a melee skill)
  Tag add_tags =
      Tag::None; // Tags to add to skill when this talent is allocated
  Tag remove_tags =
      Tag::None; // Tags to remove from skill when this talent is allocated

  // UI Layout
  float x = 0.0f;
  float y = 0.0f;
};

inline void to_json(nlohmann::json &j, const TalentNode &n) {
  j = nlohmann::json{{"id", n.id},
                     {"name_key", n.name_key},
                     {"desc_key", n.desc_key},
                     {"behavior_id", n.behavior_id},
                     {"icon_id", n.icon_id},
                     {"max_points", n.max_points},
                     {"prerequisites", n.prerequisites},
                     {"stat_modifiers", n.stat_modifiers},
                     {"damage_modifiers", n.damage_modifiers},
                     {"x", n.x},
                     {"y", n.y}};
  // Serialize tag modifications as string arrays for human-readable JSON
  if (n.add_tags != Tag::None) {
    std::vector<std::string> add_tag_strs;
    for (const auto &info : kTagInfoTable) {
      if (HasTag(n.add_tags, info.tag)) {
        add_tag_strs.emplace_back(info.id);
      }
    }
    j["add_tags"] = add_tag_strs;
  }
  if (n.remove_tags != Tag::None) {
    std::vector<std::string> remove_tag_strs;
    for (const auto &info : kTagInfoTable) {
      if (HasTag(n.remove_tags, info.tag)) {
        remove_tag_strs.emplace_back(info.id);
      }
    }
    j["remove_tags"] = remove_tag_strs;
  }
}

inline void from_json(const nlohmann::json &j, TalentNode &n) {
  j.at("id").get_to(n.id);
  j.at("name_key").get_to(n.name_key);
  j.at("desc_key").get_to(n.desc_key);
  if (j.contains("behavior_id"))
    j.at("behavior_id").get_to(n.behavior_id);
  if (j.contains("icon_id"))
    j.at("icon_id").get_to(n.icon_id);
  if (j.contains("max_points"))
    j.at("max_points").get_to(n.max_points);
  if (j.contains("prerequisites") && j.at("prerequisites").is_array()) {
    n.prerequisites.clear();
    for (const auto &pre : j.at("prerequisites")) {
      if (pre.is_number_unsigned()) {
        n.prerequisites.push_back(
            TalentPrerequisite{pre.get<uint32_t>(), 1});
      } else if (pre.is_number_integer()) {
        const int64_t raw = pre.get<int64_t>();
        if (raw >= 0) {
          n.prerequisites.push_back(
              TalentPrerequisite{static_cast<uint32_t>(raw), 1});
        }
      } else if (pre.is_object() && pre.contains("node_id")) {
        TalentPrerequisite req;
        req.node_id = pre.at("node_id").get<uint32_t>();
        req.required_points = pre.value("required_points", 1);
        if (req.required_points <= 0) {
          req.required_points = 1;
        }
        n.prerequisites.push_back(req);
      }
    }
  }
  if (j.contains("stat_modifiers"))
    j.at("stat_modifiers").get_to(n.stat_modifiers);
  if (j.contains("damage_modifiers"))
    j.at("damage_modifiers").get_to(n.damage_modifiers);
  if (j.contains("x"))
    j.at("x").get_to(n.x);
  if (j.contains("y"))
    j.at("y").get_to(n.y);
  // Parse tag modifications from string arrays
  if (j.contains("add_tags")) {
    n.add_tags = ParseTagList(j.at("add_tags").get<std::vector<std::string>>());
  }
  if (j.contains("remove_tags")) {
    n.remove_tags =
        ParseTagList(j.at("remove_tags").get<std::vector<std::string>>());
  }
}

/**
 * @brief Definition of a full talent tree for a specific skill.
 */
struct SkillTreeDefinition {
  uint32_t skill_id = 0;
  BladeMasteryId mastery_id = BladeMasteryId::None;
  std::unordered_map<uint32_t, TalentNode> nodes;
};

inline void to_json(nlohmann::json &j, const SkillTreeDefinition &t) {
  j = nlohmann::json{{"skill_id", t.skill_id},
                     {"mastery_id", static_cast<uint32_t>(t.mastery_id)},
                     {"nodes", t.nodes}};
}

inline void from_json(const nlohmann::json &j, SkillTreeDefinition &t) {
  j.at("skill_id").get_to(t.skill_id);
  t.mastery_id = static_cast<BladeMasteryId>(
      j.value("mastery_id", static_cast<uint32_t>(BladeMasteryId::None)));
  if (j.contains("nodes")) {
    // Need to manually handle map if keys are strings in JSON but uint32 in C++
    // nlohmann::json handles map keys as strings.
    // But for unordered_map<uint32_t, ...> it might need custom handling or
    // explicit string conversion. Let's rely on default behavior first,
    // assuming json library handles string-to-int key conversion for maps if
    // supported, otherwise we might need to iterate. Actually, standard
    // nlohmann::json treats object keys as strings. `std::map<int, T>` works,
    // `std::unordered_map` too usually.
    j.at("nodes").get_to(t.nodes);
  }
}

/**
 * @brief Component for skills to store their specific modifiers.
 */
struct SkillModifierComponent {
  std::vector<StatModifier> stat_modifiers;
  std::vector<DamageModifier> damage_modifiers;
};

/**
 * @brief Global modifiers from gear, passives, etc.
 * 用于存储来自装备、被动天赋等的全局修饰符。
 * - modifiers: 伤害类型转换/增伤 (用于 DamagePipeline)
 * - stat_modifiers: 条件属性修饰符 (用于 StatsSystem::GetStatWithTags)
 */
struct GlobalModifierComponent {
  std::vector<DamageModifier> modifiers;
  std::vector<StatModifier>
      stat_modifiers; // NEW: Conditional stat modifiers from affixes
};

/**
 * @brief Represents a specialized skill slot with its talent allocation.
 */
struct SpecializedSkill {
  uint32_t skill_id = INVALID_SKILL_ID;
  int bonus_levels = 0; // Extra points from equipment
  std::unordered_map<uint32_t, int>
      allocated_points; // node_id -> points invested

  int GetPointsSpent() const {
    int total = 0;
    for (auto const &[id, pts] : allocated_points) {
      total += pts;
    }
    return total;
  }

  int GetMaxPoints() const {
    return SkillConstants::DEFAULT_MAX_TALENT_POINTS + bonus_levels;
  }
};

inline void to_json(nlohmann::json &j, const SpecializedSkill &s) {
  j = nlohmann::json{{"skill_id", s.skill_id},
                     {"bonus_levels", s.bonus_levels},
                     {"allocated_points", s.allocated_points}};
}

inline void from_json(const nlohmann::json &j, SpecializedSkill &s) {
  j.at("skill_id").get_to(s.skill_id);
  if (j.contains("bonus_levels"))
    j.at("bonus_levels").get_to(s.bonus_levels);
  if (j.contains("allocated_points"))
    j.at("allocated_points").get_to(s.allocated_points);
}

/**
 * @brief Represents an active skill slot.
 */
struct SkillSlot {
  uint32_t id = 0;
  float cooldown = 0.0f;
  int current_charges = 0;
};

inline void to_json(nlohmann::json &j, const SkillSlot &s) {
  j = nlohmann::json{{"id", s.id},
                     {"cooldown", s.cooldown},
                     {"current_charges", s.current_charges}};
}

inline void from_json(const nlohmann::json &j, SkillSlot &s) {
  j.at("id").get_to(s.id);
  if (j.contains("cooldown"))
    j.at("cooldown").get_to(s.cooldown);
  if (j.contains("current_charges"))
    j.at("current_charges").get_to(s.current_charges);
}

/**
 * @brief Attached to entities (players) that can use active skills.
 */
struct ActiveSkillsComponent {
  std::array<SkillSlot, SkillConstants::MAX_SKILL_SLOTS>
      slots; // Q, W, E, R, RMB

  // Skill Specialization System (Hotkey: S)
  std::array<SpecializedSkill, SkillConstants::MAX_SKILL_SLOTS>
      specialized_slots;
  int available_talent_points = 0;
};

inline void to_json(nlohmann::json &j, const ActiveSkillsComponent &c) {
  j = nlohmann::json{{"slots", c.slots},
                     {"specialized_slots", c.specialized_slots},
                     {"available_talent_points", c.available_talent_points}};
}

inline void from_json(const nlohmann::json &j, ActiveSkillsComponent &c) {
  j.at("slots").get_to(c.slots);
  if (j.contains("specialized_slots"))
    j.at("specialized_slots").get_to(c.specialized_slots);
  if (j.contains("available_talent_points"))
    j.at("available_talent_points").get_to(c.available_talent_points);
}

struct SkillContractRuntimeComponent {
  uint32_t version = kSkillContractRuntimeVersion;
  std::unordered_map<uint32_t, uint32_t> active_transmuter_node_by_skill;
  std::unordered_map<uint32_t, float> trigger_cooldowns;
};

struct SkillComponent {
  uint32_t skill_id = 0;
  entt::entity owner = entt::null;
  std::bitset<128>
      active_nodes; // Tracks which talent nodes are active for this instance
};

// ---星盘相关组件---

/**
 * @brief Marker for skills cast by shadow/afterimage instead of player.
 */
struct ShadowCastTag {};

/**
 * @brief Marker for the Shadow Kill Array (ID 124) clone.
 */
struct ShadowCloneComponent {};

/**
 * @brief Marker indicating the next valid skill should be duplicated by Shadow
 * Kill Array.
 */
struct ShadowKillArrayReady {};

/**
 * @brief Snapshot of skill data for delayed or repeated execution.
 */
struct SkillSnapshot {
  uint32_t skill_id = 0;
  Vector2 position = {0, 0};
  Vector2 target_pos = {0, 0};
  CombatStats stats; // Snapshot of owner's stats at time of creation
  bool is_empowered = false;
  uint64_t cast_id = 0;
  std::bitset<128> active_nodes;
};

/**
 * @brief Component for the shadow entity itself.
 */
struct ShadowComponent {
  SkillSnapshot snapshot;
  float delay = 0.0f;     // Time before skill is triggered
  float lifetime = 1.0f;  // Total time before entity is destroyed
  bool triggered = false; // Whether the skill effect has been fired
  float damage_scale =
      0.3f; // Damage multiplier for skills cast by this shadow (Default 30%)
};

/**
 * @brief Component to mark an entity for specific "Ink/Shadow" visual
 * rendering.
 */
struct ShadowVisualComponent {
  Color color_tint = {50, 0, 50, 150}; // Dark purple/black tint
  bool use_shader = false;             // Whether to use the ink shader
};

struct ShadowLifetime {
  float remaining = 1.0f;
};

/**
 * @brief Component for projectiles/entities that seek targets.
 */
struct SeekerComponent {
  entt::entity target = entt::null;
  float turn_rate = 5.0f; // Radians per second
  float range = 1000.0f;  // Maximum seeking range
  bool stop_on_arrival = false;
  float arrival_threshold = 10.0f;
};

/**
 * @brief Simple animation state for entities (players/NPCs).
 */
enum class EntityAnimState : uint8_t {
  Idle,
  Move,
  SkillWindup,
  SkillCasting,
  SkillRecovery,
  Hurt,
  Dead
};

struct AnimationStateComponent {
  EntityAnimState state = EntityAnimState::Idle;
  float state_timer = 0.0f;
};

struct BladeResourceHitTracking {
  float last_gain_time = -999.0f;
  int stacks_gained = 0;
};

struct BladeMasteryComponent {
  ProfessionID profession = static_cast<ProfessionID>(0);
  BladeMasteryId selected = BladeMasteryId::None;
  bool debug_unlock_active = false;
  BladeAttunement heavenly_attunement = BladeAttunement::None;
  bool blood_oath_active = false;
};

inline void to_json(nlohmann::json &j, const BladeMasteryComponent &c) {
  j = nlohmann::json{{"profession", static_cast<uint32_t>(c.profession)},
                     {"selected", static_cast<uint32_t>(c.selected)},
                     {"debug_unlock_active", c.debug_unlock_active},
                     {"heavenly_attunement",
                      static_cast<uint32_t>(c.heavenly_attunement)},
                     {"blood_oath_active", c.blood_oath_active}};
}

inline void from_json(const nlohmann::json &j, BladeMasteryComponent &c) {
  c.profession = static_cast<ProfessionID>(j.value("profession", 0u));
  c.selected =
      static_cast<BladeMasteryId>(j.value("selected", static_cast<uint32_t>(BladeMasteryId::None)));
  c.debug_unlock_active = j.value("debug_unlock_active", false);
  c.heavenly_attunement = static_cast<BladeAttunement>(
      j.value("heavenly_attunement",
              static_cast<uint32_t>(BladeAttunement::None)));
  c.blood_oath_active = j.value("blood_oath_active", false);
}

struct BladeResourceComponent {
  BladeResourceKind kind = BladeResourceKind::None;
  int current = 0;
  int max = SkillConstants::DEFAULT_MAX_SWORD_INTENT;
  float time_since_last_gain = 0.0f;
  float last_crit_bonus_time = -999.0f;
  float crit_bonus_feedback_timer = 0.0f;
  float restart_window_timer = 0.0f;
  bool restart_window_ready = false;
  float grace_period = SkillConstants::SWORD_INTENT_GRACE_PERIOD;
  float decay_tick_timer = 0.0f;
  float decay_interval = SkillConstants::SWORD_INTENT_DECAY_INTERVAL;
  std::unordered_map<uint64_t, BladeResourceHitTracking> hit_tracking;
};

inline void to_json(nlohmann::json &j, const BladeResourceComponent &c) {
  j = nlohmann::json{{"kind", static_cast<uint32_t>(c.kind)},
                     {"current", c.current},
                     {"max", c.max},
                     {"time_since_last_gain", c.time_since_last_gain},
                     {"last_crit_bonus_time", c.last_crit_bonus_time},
                     {"crit_bonus_feedback_timer", c.crit_bonus_feedback_timer},
                     {"restart_window_timer", c.restart_window_timer},
                     {"restart_window_ready", c.restart_window_ready},
                     {"grace_period", c.grace_period},
                     {"decay_tick_timer", c.decay_tick_timer},
                     {"decay_interval", c.decay_interval}};
}

inline void from_json(const nlohmann::json &j, BladeResourceComponent &c) {
  c.kind = static_cast<BladeResourceKind>(
      j.value("kind", static_cast<uint32_t>(BladeResourceKind::None)));
  c.current = j.value("current", 0);
  c.max = j.value("max", SkillConstants::DEFAULT_MAX_SWORD_INTENT);
  c.time_since_last_gain = j.value("time_since_last_gain", 0.0f);
  c.last_crit_bonus_time = j.value("last_crit_bonus_time", -999.0f);
  c.crit_bonus_feedback_timer = j.value("crit_bonus_feedback_timer", 0.0f);
  c.restart_window_timer = j.value("restart_window_timer", 0.0f);
  c.restart_window_ready = j.value("restart_window_ready", false);
  c.grace_period =
      j.value("grace_period", SkillConstants::SWORD_INTENT_GRACE_PERIOD);
  c.decay_tick_timer = j.value("decay_tick_timer", 0.0f);
  c.decay_interval =
      j.value("decay_interval", SkillConstants::SWORD_INTENT_DECAY_INTERVAL);
}

struct BladeSignatureSkillComponent {
  uint32_t skill_id = INVALID_SKILL_ID;
  bool unlocked = false;
};

inline void to_json(nlohmann::json &j, const BladeSignatureSkillComponent &c) {
  j = nlohmann::json{{"skill_id", c.skill_id}, {"unlocked", c.unlocked}};
}

inline void from_json(const nlohmann::json &j, BladeSignatureSkillComponent &c) {
  c.skill_id = j.value("skill_id", INVALID_SKILL_ID);
  c.unlocked = j.value("unlocked", false);
}

/**
 * @brief Blade Ascendant specific resource.
 */
struct SwordIntentComponent {
  int stacks = 0;
  int max_stacks = SkillConstants::DEFAULT_MAX_SWORD_INTENT;
  float time_since_last_gain = 0.0f; // Track time since last stack gain
  float grace_period =
      SkillConstants::SWORD_INTENT_GRACE_PERIOD; // How long before decay starts
  float decay_tick_timer = 0.0f; // Timer for individual decay ticks
  float decay_interval =
      SkillConstants::SWORD_INTENT_DECAY_INTERVAL; // How fast it decays (1
                                                   // stack per 0.5s)

  // NEW: Passive gain & Hit tracking
  float passive_timer = 0.0f;
  float gain_rate = 1.0f; // Stacks per second

  // Hit tracking for skills
  // Map cast_id -> Tracking Data
  // We use cast_id instead of skill_id to differentiate multiple casts of the
  // same skill (e.g. quick spam) For channeled skills, the cast_id remains the
  // same during the channel.
  std::unordered_map<uint64_t, BladeResourceHitTracking> hit_tracking;
};

/**
 * @brief Logic state for Blade Ward (ID 4)
 */
struct BladeWardComponent {
  float duration = 10.0f;
  float remaining = 10.0f;
  int sword_count = 3;
  float interception_chance = 0.15f;
  bool is_solidified = false;   // "Solidified" talent: swords are not consumed
  bool trigger_counter = false; // Talent 470
  bool counter_spin = false;    // Talent 473
  bool has_blink_counter = false; // Talent 451
  bool has_agile_counter = false; // Talent 452
  bool has_rainbow_qi = false;    // Talent 471
};

struct PhantomFlashComponent {
  float counter_window = 0.5f;
  float knockback_bonus = 0.0f;
  bool triggered = false;
  bool flow_reset = false;          // Talent 951
  bool synergy_shadow_hide = false; // Talent 930
  int intent_overflow = 0;          // Talent 952 points
  Tag enchant_tag = Tag::None;      // Selected transmuter element
};

struct MindBladeAI {
  entt::entity target = entt::null;
  float retarget_timer = 0.0f;
  float attack_timer = 0.0f;
  float base_interval = 0.3f;
  float range = 400.0f;
};

struct MindBladeComponent {
  entt::entity owner = entt::null;
  float intelligence_scaling = 1.0f;
  int stack_count = 0;
};

// --- SUMMON SYSTEM COMPONENTS ---

namespace SummonArchetype {
inline constexpr uint32_t SpiritSword =
    entt::hashed_string{"summon_spirit_sword"}.value();
inline constexpr uint32_t ShadowEcho =
    entt::hashed_string{"summon_shadow_echo"}.value();
inline constexpr uint32_t Unknown =
    entt::hashed_string{"summon_unknown"}.value();
} // namespace SummonArchetype

enum class SummonInheritMode : uint8_t { Snapshot, Dynamic, Mixed };
enum class SummonRole : uint8_t { Melee, Ranged, Support, Orbit };
enum class SummonCommandMode : uint8_t {
  Passive,
  Defend,
  Assist,
  Aggressive
};

enum class SpiritSwordMode : uint8_t {
  Guardian, // Attack nearest (Default)
  Elite     // Priority on high rarity
};

struct SummonComponent {
  entt::entity owner = entt::null;
  uint32_t skill_id = 0;
  uint32_t archetype_id = SummonArchetype::Unknown;
  float lifetime = 10.0f;
  float max_lifetime = 10.0f;
  uint32_t icon_id = 0;
};

struct SummonCombatProfile {
  float damage_scale = 1.0f;
  SummonInheritMode inherit_mode = SummonInheritMode::Dynamic;
  float proc_budget_per_second = 3.0f;
  float proc_budget_cap = 6.0f;
  float melee_orbit_hit_radius = 30.0f;
  float melee_orbit_base_damage = 25.0f;
};

struct SummonAIProfile {
  SummonRole role = SummonRole::Orbit;
  SummonCommandMode command_mode = SummonCommandMode::Assist;
  float retarget_interval = 0.2f;
  float leash_radius = 300.0f;
};

struct SummonRuntimeState {
  entt::entity current_target = entt::null;
  float attack_cd = 0.0f;
  float retarget_timer = 0.0f;
  float proc_budget = 0.0f;
  CombatStats snapshot_stats = {};
  bool has_snapshot = false;
};

struct SummonAttributionContext {
  entt::entity owner = entt::null;
  entt::entity summon = entt::null;
  uint32_t source_skill_id = 0;
};

struct SpiritSwordTag {};

struct SpiritSwordAI {
  entt::entity target = entt::null;
  float attack_timer = 0.0f;
  float attack_interval = 1.0f;
  Vector2 orbit_offset = {0, 0};
  float orbit_angle = 0.0f;

  // State Machine
  enum class State : uint8_t {
    Idle,      // Orbiting
    Chasing,   // Flying to target (Sword Rain)
    Attacking, // Striking (Heavy Sword)
    Returning, // Returning to orbit
    MeleeOrbit // Orbiting and dealing contact damage (Talent 352)
  } state = State::Idle;

  float state_timer = 0.0f;
  Vector2 start_pos = {0, 0}; // For return lerp
};

struct BladeFormationComponent {
  int max_swords = 1;
  int current_swords = 0;
  float damage_penalty = 1.0f; // Talent 311: 无尽剑匣
  float attack_interval = 1.0f;
  float attack_timer = 0.0f;
  float search_radius = 200.0f;
  bool is_empowered = false;

  SpiritSwordMode mode = SpiritSwordMode::Guardian;

  // Talent Flags
  bool has_giant_sword = false;   // Talent 330
  bool mana_on_hit = false;       // Talent 351
  bool immortality_ready = false; // Talent 353
  bool melee_orbit = false;       // Talent 352 (Melee Orbit)
};

struct SwordArrayComponent {
  float duration = 5.0f;
  float radius = 150.0f;
  float damage_interval = 0.5f;
  float damage_timer = 0.0f;
  entt::entity owner = entt::null;
  bool is_empowered = false;
  uint64_t cast_id = 0;

  Color core_color = {150, 50, 255, 255};
  Color glow_color = {200, 100, 255, 255};

  // Talent Flags (contract-aligned key nodes)
  bool has_slow = false;             // Talent 630
  bool has_armor_shred = false;      // Talent 631 / 671
  bool has_execute = false;          // Talent 633
  bool gain_intent_on_tick = false;  // Talent 652
  float execute_health_threshold_ratio = 0.15f;
  float execute_damage_max_health_ratio = 0.10f;
};

struct HeavenlySwordFieldComponent {
  entt::entity owner = entt::null;
  float duration = 5.0f;
  float radius = 140.0f;
  float damage_interval = 0.5f;
  float damage_timer = 0.0f;
  float linked_cut_cooldown = 0.0f;
  float cycle_refund_timer = 1.0f;
  uint64_t cast_id = 0;
  int spent_tiers = 0;
  int cycle_refunds_granted = 0;
  int linked_hit_count = 0;
  int echo_strikes_triggered = 0;
  float impact_damage_mult = 1.0f;
  float field_damage_mult = 1.0f;
  float resist_reduction = 6.0f;
  float extra_resist_reduction = 0.0f;
  float linked_cut_effectiveness = 0.25f;
  BladeAttunement attunement = BladeAttunement::None;
  bool has_trigger_echo = false;
  bool has_cycle = false;
  bool has_domain_lock = false;
  bool has_array_synchrony = false;
  bool has_polarization = false;
  bool lightning_tribunal = false;
  bool frozen_dominion = false;
  bool solar_incineration = false;
};

struct BloodSeaFieldComponent {
  entt::entity owner = entt::null;
  float duration = 5.0f;
  float radius = 120.0f;
  float damage_interval = 0.25f;
  float damage_timer = 0.0f;
  float linked_pulse_cooldown = 0.0f;
  uint64_t cast_id = 0;
  int consumed_bloodthirst = 0;
  int linked_hit_count = 0;
  int pulses_triggered = 0;
  float bonus_damage_mult = 1.0f;
  float leech_ratio = 0.12f;
  float move_follow_speed = 10.0f;
  float resist_shred = 0.0f;
  bool has_trigger_burst = false;
  bool has_linked_synergy = false;
  bool has_recovery_keystone = false;
  bool has_void_keystone = false;
  bool torrent_form = false;
  bool ring_form = false;
};

struct ChannelingComponent {
  uint32_t skill_id;
  float channel_timer = 0.0f;
  float tick_interval = 0.2f;
  float tick_timer = 0.0f;
  Vector2 target_pos;
  bool is_empowered = false;
  float total_duration = 0.0f;
  uint64_t cast_id = 0;
  bool extra_projectiles = false; // Talent 551
  bool consume_intent =
      false; // If true, will try to consume intent for effects
  bool burst_finisher = false;   // Talent 513: Trigger finisher on channel end
  bool full_screen_lock = false; // Talent 530: Target all enemies
  Tag conversion_tag = Tag::None; // Talent transmuter conversion
  float bonus_damage_mult = 1.0f;
  float bonus_crit_chance = 0.0f;
  float bonus_armor_pen = 0.0f;
  bool synergy_lock = false; // Skill 7 node 730
};

} // namespace NoMoreDay
