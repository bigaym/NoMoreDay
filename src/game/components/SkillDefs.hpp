#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <entt/entt.hpp>
#include "game/data/TagRegistry.hpp"
#include "raylib.h"
#include "game/components/Stats.hpp"

namespace NoMoreDay {

/**
 * @brief DamagePool stores flat damage values for multiple types.
 * Indexed by the bit position of the DamageType tag (0-15).
 */
struct DamagePool {
    std::array<float, 16> values = {0.0f};

    void Clear() {
        values.fill(0.0f);
    }

    void Add(Tag type, float amount) {
        // Find bit index
        uint64_t val = static_cast<uint64_t>(type);
        if (val == 0) return;
        
        // __builtin_ctzll finds the number of trailing zeros, which is the bit index for single bit tags
        int index = std::countr_zero(val);
        if (index < 16) {
            values[index] += amount;
        }
    }

    float Get(Tag type) const {
        uint64_t val = static_cast<uint64_t>(type);
        if (val == 0) return 0.0f;
        int index = std::countr_zero(val);
        if (index < 16) {
            return values[index];
        }
        return 0.0f;
    }

    void Merge(const DamagePool& other) {
        for (size_t i = 0; i < 16; ++i) {
            values[i] += other.values[i];
        }
    }
};

enum class ModifierType : uint8_t {
    Flat,           // Added to base damage
    Increased,      // Summed together (1 + inc1 + inc2 + ...)
    More,           // Multiplied independently (* more1 * more2 * ...)
    Convert,        // Convert X% of A to B
    GainExtra,      // Gain X% of A as extra B
};

inline void to_json(nlohmann::json& j, const ModifierType& e) { j = static_cast<uint8_t>(e); }
inline void from_json(const nlohmann::json& j, ModifierType& e) { e = static_cast<ModifierType>(j.get<uint8_t>()); }

/**
 * @brief DamageModifier defines how a damage value is modified.
 */
struct DamageModifier {
    Tag source_tag = Tag::None; // Tag this modifier applies to (e.g., Physical, Melee)
    Tag target_tag = Tag::None; // Tag the output has (used for Convert/GainExtra)
    float value = 0.0f;
    ModifierType type = ModifierType::Flat;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DamageModifier, source_tag, target_tag, value, type)

/**
 * @brief Represents a node in a skill's specialization talent tree.
 */
struct TalentNode {
    uint32_t id = 0;
    std::string name_key;
    std::string desc_key;
    uint32_t icon_id = 0; // Added for UI Polish
    int max_points = 1;
    
    std::vector<uint32_t> prerequisites;
    std::vector<StatModifier> stat_modifiers;
    std::vector<DamageModifier> damage_modifiers;
    
    // Tag modification (e.g., add "spell" tag to a melee skill)
    Tag add_tags = Tag::None;     // Tags to add to skill when this talent is allocated
    Tag remove_tags = Tag::None;  // Tags to remove from skill when this talent is allocated
    
    // UI Layout
    float x = 0.0f;
    float y = 0.0f;
};

inline void to_json(nlohmann::json& j, const TalentNode& n) {
    j = nlohmann::json{
        {"id", n.id}, {"name_key", n.name_key}, {"desc_key", n.desc_key},
        {"icon_id", n.icon_id},
        {"max_points", n.max_points}, {"prerequisites", n.prerequisites},
        {"stat_modifiers", n.stat_modifiers}, {"damage_modifiers", n.damage_modifiers},
        {"x", n.x}, {"y", n.y}
    };
    // Serialize tag modifications as string arrays for human-readable JSON
    if (n.add_tags != Tag::None) {
        std::vector<std::string> add_tag_strs;
        for (const auto& info : kTagInfoTable) {
            if (HasTag(n.add_tags, info.tag)) {
                add_tag_strs.emplace_back(info.id);
            }
        }
        j["add_tags"] = add_tag_strs;
    }
    if (n.remove_tags != Tag::None) {
        std::vector<std::string> remove_tag_strs;
        for (const auto& info : kTagInfoTable) {
            if (HasTag(n.remove_tags, info.tag)) {
                remove_tag_strs.emplace_back(info.id);
            }
        }
        j["remove_tags"] = remove_tag_strs;
    }
}

inline void from_json(const nlohmann::json& j, TalentNode& n) {
    j.at("id").get_to(n.id);
    j.at("name_key").get_to(n.name_key);
    j.at("desc_key").get_to(n.desc_key);
    if (j.contains("icon_id")) j.at("icon_id").get_to(n.icon_id);
    if (j.contains("max_points")) j.at("max_points").get_to(n.max_points);
    if (j.contains("prerequisites")) j.at("prerequisites").get_to(n.prerequisites);
    if (j.contains("stat_modifiers")) j.at("stat_modifiers").get_to(n.stat_modifiers);
    if (j.contains("damage_modifiers")) j.at("damage_modifiers").get_to(n.damage_modifiers);
    if (j.contains("x")) j.at("x").get_to(n.x);
    if (j.contains("y")) j.at("y").get_to(n.y);
    // Parse tag modifications from string arrays
    if (j.contains("add_tags")) {
        n.add_tags = ParseTagList(j.at("add_tags").get<std::vector<std::string>>());
    }
    if (j.contains("remove_tags")) {
        n.remove_tags = ParseTagList(j.at("remove_tags").get<std::vector<std::string>>());
    }
}

/**
 * @brief Definition of a full talent tree for a specific skill.
 */
struct SkillTreeDefinition {
    uint32_t skill_id = 0;
    std::unordered_map<uint32_t, TalentNode> nodes;
};

inline void to_json(nlohmann::json& j, const SkillTreeDefinition& t) {
    j = nlohmann::json{{"skill_id", t.skill_id}, {"nodes", t.nodes}};
}

inline void from_json(const nlohmann::json& j, SkillTreeDefinition& t) {
    j.at("skill_id").get_to(t.skill_id);
    if (j.contains("nodes")) {
        // Need to manually handle map if keys are strings in JSON but uint32 in C++
        // nlohmann::json handles map keys as strings.
        // But for unordered_map<uint32_t, ...> it might need custom handling or explicit string conversion.
        // Let's rely on default behavior first, assuming json library handles string-to-int key conversion for maps if supported,
        // otherwise we might need to iterate.
        // Actually, standard nlohmann::json treats object keys as strings. `std::map<int, T>` works, `std::unordered_map` too usually.
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
    std::vector<StatModifier> stat_modifiers;  // NEW: Conditional stat modifiers from affixes
};

/**
 * @brief Represents a specialized skill slot with its talent allocation.
 */
struct SpecializedSkill {
    uint32_t skill_id = 0;
    int bonus_levels = 0; // Extra points from equipment
    std::unordered_map<uint32_t, int> allocated_points; // node_id -> points invested

    int GetPointsSpent() const {
        int total = 0;
        for (auto const& [id, pts] : allocated_points) {
            total += pts;
        }
        return total;
    }

    int GetMaxPoints() const {
        return 20 + bonus_levels;
    }
};

inline void to_json(nlohmann::json& j, const SpecializedSkill& s) {
    j = nlohmann::json{
        {"skill_id", s.skill_id}, 
        {"bonus_levels", s.bonus_levels},
        {"allocated_points", s.allocated_points}
    };
}

inline void from_json(const nlohmann::json& j, SpecializedSkill& s) {
    j.at("skill_id").get_to(s.skill_id);
    if (j.contains("bonus_levels")) j.at("bonus_levels").get_to(s.bonus_levels);
    if (j.contains("allocated_points")) j.at("allocated_points").get_to(s.allocated_points);
}

/**
 * @brief Represents an active skill slot.
 */
struct SkillSlot {
    uint32_t id = 0;
    float cooldown = 0.0f;
    int current_charges = 0;
};

inline void to_json(nlohmann::json& j, const SkillSlot& s) {
    j = nlohmann::json{{"id", s.id}, {"cooldown", s.cooldown}, {"current_charges", s.current_charges}};
}

inline void from_json(const nlohmann::json& j, SkillSlot& s) {
    j.at("id").get_to(s.id);
    if (j.contains("cooldown")) j.at("cooldown").get_to(s.cooldown);
    if (j.contains("current_charges")) j.at("current_charges").get_to(s.current_charges);
}

/**
 * @brief Attached to entities (players) that can use active skills.
 */
struct ActiveSkillsComponent {
    std::array<SkillSlot, 5> slots; // Q, W, E, R, RMB
    
    // Skill Specialization System (Hotkey: S)
    std::array<SpecializedSkill, 5> specialized_slots;
    int available_talent_points = 0;
};

inline void to_json(nlohmann::json& j, const ActiveSkillsComponent& c) {
    j = nlohmann::json{
        {"slots", c.slots},
        {"specialized_slots", c.specialized_slots},
        {"available_talent_points", c.available_talent_points}
    };
}

inline void from_json(const nlohmann::json& j, ActiveSkillsComponent& c) {
    j.at("slots").get_to(c.slots);
    if (j.contains("specialized_slots")) j.at("specialized_slots").get_to(c.specialized_slots);
    if (j.contains("available_talent_points")) j.at("available_talent_points").get_to(c.available_talent_points);
}

struct SkillComponent {
    uint32_t skill_id;
    entt::entity owner;
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
 * @brief Marker indicating the next valid skill should be duplicated by Shadow Kill Array.
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
};

/**
 * @brief Component for the shadow entity itself.
 */
struct ShadowComponent {
    SkillSnapshot snapshot;
    float delay = 0.0f;       // Time before skill is triggered
    float lifetime = 1.0f;    // Total time before entity is destroyed
    bool triggered = false;   // Whether the skill effect has been fired
};

struct ShadowLifetime {
    float remaining = 1.0f;
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

/**
 * @brief Blade Ascendant specific resource.
 */
struct SwordIntentComponent {
    int stacks = 0;
    int max_stacks = 10;
    float time_since_last_gain = 0.0f; // Track time since last stack gain
    float grace_period = 5.0f;         // How long before decay starts
    float decay_tick_timer = 0.0f;     // Timer for individual decay ticks
    float decay_interval = 0.5f;       // How fast it decays (1 stack per 0.5s)
};

/**
 * @brief Logic state for Blade Ward (ID 4)
 */
struct BladeWardComponent {
    float remaining = 10.0f;
    int sword_count = 3;
    float interception_chance = 0.15f; 
    bool is_solidified = false;        // "Solidified" talent: swords are not consumed
};

struct PhantomFlashComponent {
    float counter_window = 0.5f;
    float knockback_bonus = 0.0f;
    bool triggered = false;
};

// --- SUMMON SYSTEM COMPONENTS ---

enum class SpiritSwordMode : uint8_t {
    Guardian,  // Attack nearest (Default)
    Elite      // Priority on high rarity
};

struct SummonComponent {
    entt::entity owner = entt::null;
    uint32_t skill_id = 0;
    float lifetime = 10.0f;
    float max_lifetime = 10.0f;
    int icon_id = 0;
    std::string name;
};

struct SpiritSwordTag {};

struct SpiritSwordAI {
    entt::entity target = entt::null;
    float attack_timer = 0.0f;
    float attack_interval = 1.0f;
    Vector2 orbit_offset = {0, 0};
    float orbit_angle = 0.0f;
};

struct BladeFormationComponent {
    int max_swords = 1;
    int current_swords = 0;
    float attack_interval = 1.0f;
    float attack_timer = 0.0f;
    float search_radius = 200.0f;
    bool is_empowered = false;

    SpiritSwordMode mode = SpiritSwordMode::Guardian;

    // Talent Flags
    bool has_giant_sword = false;     // Talent 310
    bool shockwave_on_crit = false;  // Talent 311
    bool mana_on_hit = false;        // Talent 321
    bool immortality_ready = false;  // Talent 322
};

struct SwordArrayComponent {
    float duration = 5.0f;
    float radius = 150.0f;
    float damage_interval = 0.5f;
    float damage_timer = 0.0f;
    entt::entity owner = entt::null;
    bool is_empowered = false;

    // Talent Flags
    bool has_slow = false;         // Talent 610
    bool has_armor_shred = false;  // Talent 611
    bool has_execute = false;      // Talent 612
};

struct ChannelingComponent {
    uint32_t skill_id;
    float channel_timer = 0.0f;
    float tick_interval = 0.2f;
    float tick_timer = 0.0f;
    Vector2 target_pos;
    bool is_empowered = false;
    float total_duration = 0.0f;
    bool extra_projectiles = false;    // Talent 520
    bool consume_intent = false;       // If true, will try to consume intent for effects
};

} // namespace NoMoreDay
