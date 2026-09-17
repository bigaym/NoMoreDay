#pragma once
#include "game/foundation/data/BladeMasteryData.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillContract.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "raylib.h"
#include <array>
#include <bitset>
#include <cstdint>
#include <entt/entt.hpp>
#include <optional>
#include <string>
#include <string_view>
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
  if (j.is_string()) {
    const std::string s = j.get<std::string>();
    if (s == "Flat") e = ModifierType::Flat;
    else if (s == "Increased") e = ModifierType::Increased;
    else if (s == "More") e = ModifierType::More;
    else if (s == "Convert") e = ModifierType::Convert;
    else if (s == "GainExtra") e = ModifierType::GainExtra;
    else e = ModifierType::Flat;
  } else if (j.is_number()) {
    e = static_cast<ModifierType>(j.get<uint8_t>());
  } else {
    e = ModifierType::Flat;
  }
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

  bool operator==(const DamageModifier &) const = default;
};

inline void to_json(nlohmann::json &j, const DamageModifier &m) {
  j = nlohmann::json{
    {"source_tag", static_cast<uint64_t>(m.source_tag)},
    {"target_tag", static_cast<uint64_t>(m.target_tag)},
    {"value", m.value},
    {"type", static_cast<uint8_t>(m.type)}
  };
}

inline void from_json(const nlohmann::json &j, DamageModifier &m) {
  if (j.contains("source_tag")) {
    if (j.at("source_tag").is_string()) {
      auto t = TagFromString(j.at("source_tag").get<std::string>());
      m.source_tag = t.value_or(Tag::None);
    } else if (j.at("source_tag").is_number()) {
      m.source_tag = static_cast<Tag>(j.at("source_tag").get<uint64_t>());
    }
  }
  if (j.contains("target_tag")) {
    if (j.at("target_tag").is_string()) {
      auto t = TagFromString(j.at("target_tag").get<std::string>());
      m.target_tag = t.value_or(Tag::None);
    } else if (j.at("target_tag").is_number()) {
      m.target_tag = static_cast<Tag>(j.at("target_tag").get<uint64_t>());
    }
  }
  if (j.contains("value")) {
    m.value = j.at("value").get<float>();
  }
  if (j.contains("type")) {
    from_json(j.at("type"), m.type);
  }
}

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
 * @brief Category of a talent tooltip quantitative line.
 *
 * Drives display priority at render time without parsing display text
 * (display labels are data). Serialized as the optional "display_category"
 * key; old data without the key falls back to Default.
 */
enum class DisplayLineCategory : uint8_t {
  Label,
  Damage,
  Duration,
  Frequency,
  Speed,
  Range,
  Cost,
  Cooldown,
  Default
};

inline constexpr std::array<std::string_view,
                            static_cast<std::size_t>(
                                DisplayLineCategory::Default) +
                                1>
    kDisplayLineCategoryNames = {
        "label",     // Label
        "damage",    // Damage
        "duration",  // Duration
        "frequency", // Frequency
        "speed",     // Speed
        "range",     // Range
        "cost",      // Cost
        "cooldown",  // Cooldown
        "default",   // Default
};

inline void to_json(nlohmann::json &j, const DisplayLineCategory &c) {
  const auto idx = static_cast<std::size_t>(c);
  j = idx < kDisplayLineCategoryNames.size()
          ? nlohmann::json(std::string(kDisplayLineCategoryNames[idx]))
          : nlohmann::json("default");
}

inline void from_json(const nlohmann::json &j, DisplayLineCategory &c) {
  if (j.is_string()) {
    const std::string raw = j.get<std::string>();
    for (std::size_t i = 0; i < kDisplayLineCategoryNames.size(); ++i) {
      if (kDisplayLineCategoryNames[i] == raw) {
        c = static_cast<DisplayLineCategory>(i);
        return;
      }
    }
    c = DisplayLineCategory::Default;
    return;
  }
  if (j.is_number_integer()) {
    c = static_cast<DisplayLineCategory>(j.get<uint8_t>());
    return;
  }
  c = DisplayLineCategory::Default;
}

/**
 * @brief Metadata for a single quantitative line in a talent tooltip.
 */
struct TalentDisplayLine {
  std::string label;
  float base_value = 0.0f;
  float per_point = 0.0f;
  bool is_percent = false;
  std::string suffix;
  DisplayLineCategory displayCategory = DisplayLineCategory::Default;
};

inline void to_json(nlohmann::json &j, const TalentDisplayLine &l) {
  j = nlohmann::json{{"label", l.label},
                     {"base_value", l.base_value},
                     {"per_point", l.per_point},
                     {"is_percent", l.is_percent},
                     {"suffix", l.suffix},
                     {"display_category", l.displayCategory}};
}

inline void from_json(const nlohmann::json &j, TalentDisplayLine &l) {
  j.at("label").get_to(l.label);
  l.base_value = j.value("base_value", 0.0f);
  l.per_point = j.value("per_point", 0.0f);
  l.is_percent = j.value("is_percent", false);
  if (j.contains("suffix")) {
    j.at("suffix").get_to(l.suffix);
  }
  if (j.contains("display_category")) {
    j.at("display_category").get_to(l.displayCategory);
  }
}

/**
 * @brief Behavior injection identifiers for C++ logic hooks.
 *
 * TalentNode::behavior_id remains a std::string for JSON (de)serialization
 * compatibility; convert at the application boundary via
 * SkillBehaviorIdFromString/SkillBehaviorIdToString.
 */
enum class SkillBehaviorId : uint8_t {
  None = 0, // empty string == no behavior
  ShadowCaster,
  Count,
};

inline constexpr std::array<std::string_view,
                            static_cast<std::size_t>(SkillBehaviorId::Count)>
    kSkillBehaviorIdNames = {
        "",              // None
        "shadow_caster", // ShadowCaster
};

[[nodiscard]] constexpr std::string_view SkillBehaviorIdToString(
    SkillBehaviorId id) noexcept {
  const auto idx = static_cast<std::size_t>(id);
  return idx < kSkillBehaviorIdNames.size() ? kSkillBehaviorIdNames[idx]
                                            : std::string_view{};
}

[[nodiscard]] constexpr std::optional<SkillBehaviorId>
SkillBehaviorIdFromString(std::string_view name) noexcept {
  for (std::size_t i = 0; i < kSkillBehaviorIdNames.size(); ++i) {
    if (kSkillBehaviorIdNames[i] == name) {
      return static_cast<SkillBehaviorId>(i);
    }
  }
  return std::nullopt;
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
  std::vector<TalentDisplayLine> display_lines;

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
                     {"display_lines", n.display_lines},
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
  if (j.contains("display_lines"))
    j.at("display_lines").get_to(n.display_lines);
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
  if (j.contains("allocated_points")) {
    j.at("allocated_points").get_to(s.allocated_points);
    // 规范不变量：0/负点数条目等价于未分配，加载期剔除。这保证
    // skills::ReadPoints(spec,node) > 0 与实际存在性判定等价，避免旧存档中
    // 的 0 值条目被错误回退判定为节点点亮（见 SkillPointAccess.hpp）。
    for (auto it = s.allocated_points.begin(); it != s.allocated_points.end();) {
      if (it->second <= 0) {
        it = s.allocated_points.erase(it);
      } else {
        ++it;
      }
    }
  }
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

// 基础载荷定义 (纯 POD / Standard Layout)
enum class PayloadType : uint8_t {
  Damage = 0,
  Ailment,
  Buff,
  Impulse
};

struct PayloadDefinition {
  PayloadType type = PayloadType::Damage;
  uint32_t ailment_id = 0;
  float value_mult = 1.0f;
  Tag damage_tags = Tag::None;
  float duration = 3.0f; // 参数化异常/持续时长

  bool operator==(const PayloadDefinition &) const = default;
};
static_assert(std::is_standard_layout_v<PayloadDefinition>);

// 技能 9 绝影绝剑专精参数: 唯一写入方为 SkillSpecializationBaker case9,
// 唯一读取方为 PhantomTrance 行为, 避免数值双源。
struct PhantomTranceParams {
  float duration_sec = 3.0f;               // 975 延命后的形态时长
  float move_speed_pct = 20.0f;            // 基础 20 + 902 身轻如燕追加
  float dodge_pct = 0.0f;                  // 902 形态内闪避
  float recovery_pct = 0.0f;               // 974 空明心境: 形态内冷却缩减
  float burst_damage_mult = 1.0f;          // 976 气旋爆发: 结束爆发伤害/半径倍率
  bool cheat_death_hold = false;           // 977 向死而生: 免死不提前结束
  float rebirth_lost_pct = 0.0f;           // 978 浴血重生: 免死触发时按损失生命回复
  float rebirth_flat_pct = 0.0f;           // 978 浴血重生: 未触发时按上限回复
  float ward_pct = 0.0f;                   // 979 绝影护甲: 施放时获得上限比例护盾
  bool void_body = false;                  // 980 虚灵之躯: 潜行/穿行/不可选中
  float void_body_hp_cost_pct = 0.15f;     // 980 施放时消耗当前生命比例
  float weaken_on_pass_pct = 0.0f;         // 913 灵流穿透: 穿行敌人获虚弱比例
  float void_gift_mana_per_sec = 0.0f;     // 914 虚境馈赠: 形态内每秒回蓝
  float void_gift_dr_pct = 0.0f;           // 914 虚境馈赠: 形态内全局减伤
  bool death_seal = false;                 // 981 逆脉: 锁血禁疗增伤
  float death_seal_hp_cap_pct = 0.33f;     // 981 锁血上限比例
  float death_seal_damage_more_pct = 33.0f;// 981 形态内全局增伤
  float last_stand_crit_pct = 0.0f;        // 982 孤注一掷: 每缺失 1% 生命的暴伤加成
  int death_spiral_count = 0;              // 983 死亡螺旋: 每轮飞剑数
  float death_spiral_damage_pct = 0.0f;    // 983 每柄伤害=临时损失上限*该值
  float bloodthirst_pct = 0.0f;            // 984 嗜血本能: 结束时按期间伤害回复
  float atk_cast_speed_pct = 0.0f;         // 934 剑随心动: 攻速/施法速度
  bool blink = false;                      // 985 破空一闪: 瞬移至光标再入形态
  bool echo_synergy = false;               // 993 影剑回响: 回旋命中追加影子回响
  int intent_per_sec = 0;                  // 987 意随神行: 每秒剑意
  float sword_step_dodge_pct = 0.0f;       // 988 御剑化影: 御剑步时形态闪避
  float sword_step_drain_mult = 1.0f;      // 988 御剑步连击点流失倍率
  float time_reversal_sec = 0.0f;          // 954 时光逆流: 结束返还其他技能冷却
  float focus_mana_reduce_pct = 0.0f;      // 955 全神贯注: 形态内其他技能法耗降低
  Tag transmuter_tag = Tag::None;          // 989 天山雪隐(Cold) / 972 疾空惊雷(Lightning)
  float frost_amp_pct = 0.0f;              // 990 凛冬附魔: 对冰冻/冰缓目标增伤
  float overload_speed_pct = 0.0f;         // 973 过载护盾: 雷盾期间移速/攻速
  float enchant_pen_per_intent_pct = 0.0f; // 991 意念穿透: 每层剑意元素穿透
  float enchant_pen_cap_pct = 40.0f;       // 991 穿透上限
  bool enchant_refresh_on_kill = false;    // 992 灵气反哺: 对应异常击杀刷新附魔

  bool operator==(const PhantomTranceParams &) const = default;
};
static_assert(std::is_standard_layout_v<PhantomTranceParams>);

// 纯 POD 交付参数结构
struct BakedDeliveryParams {
  uint32_t feature_flags = 0;      // 位掩码: 如 IsBoomerang, HasApexVortex, HasSplit

  // 通用参数包 (无需堆分配的定长紧凑结构)
  // 单一事实源说明: 数量与尺寸的唯一事实源为 BakedSkillProfile 顶层既有字段 projectile_count 与 area_radius
  float speed = 300.0f;
  float range = 200.0f;
  float duration = 1.0f;
  uint8_t sub_count = 0;           // 分裂数 / 连跳数
  float sub_interval = 0.0f;       // 脉冲间隔 / 弹幕发射间隔

  // 扩展显式语义字段 (避免重载 range/speed)
  float pull_radius = 0.0f;        // 顶点牵引/引力陷阱半径 (如 232/830)
  float armor_pen = 0.0f;          // 护甲/元素穿透 (如 571)
  float bonus_crit = 0.0f;         // 额外暴击率加成 (如 552)
  float bonus_crit_damage = 0.0f;  // 额外暴击伤害加成 (如 332)

  // 技能 8 御剑·回旋专项参数: 唯一写入方为 SkillSpecializationBaker case8,
  // 唯一读取方为 BladeBoomerang 行为与 BoomerangDeliverySystem, 避免参数双源。
  float return_damage_mult = 1.0f;     // 803 回力感应: 折返阶段伤害倍率
  float bleed_chance = 0.0f;           // 811 放血: 命中施加 1 层流血的概率 [0,1]
  float crit_mult_vs_bleeding = 0.0f;  // 812 撕裂伤口: 对流血目标额外暴击伤害
  float side_angle_mult = 1.0f;        // 831 无尽刃舞: 侧刃扇形角缩放 (可降至 0.3)
  float catch_mana = 0.0f;             // 832 接剑: 接刃回蓝
  float combo_attack_speed = 0.0f;     // 833 连环劲: 接刃后下次施放攻速加成比例
  float step_extend_sec = 0.0f;        // 834 御剑接踵: 御剑步延长秒数
  float pull_radius_mult = 1.0f;       // 851 重力网: 牵引半径与刀刃命中盒缩放
  float intent_gain_chance = 0.0f;     // 852 意随剑舞: 折返命中获得剑意的概率 [0,1]
  float intent_scaling = 0.0f;         // 853 心剑合一: 每层剑意的速度/命中盒加成比例
  float giant_armor_scale = 0.0f;      // 854 巨阙: 总护甲转化基础物理伤害比例 [0,1]
  float heal_bleed_pct = 0.0f;         // 815 风眼: 该次飞行流血总伤转化治疗比例 [0,1]
  float path_width_mult = 1.0f;        // 874 元素尾迹: 元素路径宽度倍率
  float path_duration_mult = 1.0f;     // 874 元素尾迹: 元素路径持续时间倍率
  float path_pen = 0.0f;               // 875 灵根破壁: 本技能元素穿透 [0,1]
  float path_amp = 0.0f;               // 871 燎原之势: 燃烧路径受击增伤 [0,1]
  float arc_freq_mult = 1.0f;          // 873 高压电弧: 电弧触发频率倍率
  float element_shield_pct = 0.0f;     // 876 元素护体: 元素路径内绝对减伤 [0,1]

  // 技能 9 绝影绝剑专项参数（见 PhantomTranceParams）
  PhantomTranceParams trance;

  bool operator==(const BakedDeliveryParams &) const = default;
};
static_assert(std::is_standard_layout_v<BakedDeliveryParams>);
static_assert(std::is_trivially_destructible_v<BakedDeliveryParams>);

// 纯 POD 实体烘焙属性表
struct BakedSkillProfile {
  uint32_t skill_id = 0;

  // 显式烘焙成功标记：false 表示该档案不可消费（含 skillData 缺失时写入的哨兵档案）。
  // "未烘焙"判定必须以此字段为准，不得以 area_radius == 1.0f 推断——1.0f 是结构体
  // 默认值，合法烘焙结果同样可能等于该值。
  bool is_baked = false;

  int effective_level = 1;
  float effective_cooldown = 0.0f;
  float effective_mana_cost = 0.0f;
  Tag effective_tags = Tag::None;
  int projectile_count = 1;
  float area_radius = 1.0f;
  float proc_coefficient = 1.0f;
  float more_damage_mult = 1.0f;
  int effective_charges = 0;

  // 御风而行 (114): 仅当玩家处于"御剑步"状态时,近战交付获得 8%...24% 额外暴击率
  // 运行时由交付构造处检查剑步状态后才注入,不入 delivery.bonus_crit (避免无条件生效)
  float riding_wind_bonus_crit = 0.0f;

  // 烘焙后的交付参数
  BakedDeliveryParams delivery{};

  // 定长 POD 载荷数组 (Zero Heap Allocation)
  static constexpr uint8_t kMaxInjectedPayloads = 4;
  std::array<PayloadDefinition, kMaxInjectedPayloads> injected_payloads{};
  uint8_t injected_count = 0;

  bool operator==(const BakedSkillProfile &) const = default;
};
static_assert(std::is_standard_layout_v<BakedSkillProfile>);
static_assert(std::is_trivially_destructible_v<BakedSkillProfile>);

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

  // 纯 POD 烘焙属性表缓存 (每个槽位对应一个)
  std::array<BakedSkillProfile, SkillConstants::MAX_SKILL_SLOTS> baked_profiles{};
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

// 64 字节缓存行对齐的轻量伤害上下文 (Zero Heap Allocation, Standard Layout)
struct alignas(64) DamagePayloadContext {
  float base_damage_min = 0.0f;
  float base_damage_max = 0.0f;
  float crit_chance = 0.0f;      // 归一化暴击率 [0.0, 1.0] (例如 0.05f 为 5%，1.0f 为 100%)
  float crit_multiplier = 1.5f;
  float increased_damage = 0.0f; // 增伤小数加成 (例如 0.5f 为 +50% 增伤；基准无加成为 0.0f)
  float more_damage = 1.0f;      // More 乘算倍率 (默认 1.0f)
  Tag effective_tags = Tag::None;
  uint32_t source_skill_id = 0;
  uint8_t trigger_depth = 0;

  bool operator==(const DamagePayloadContext &) const = default;
};
static_assert(std::is_standard_layout_v<DamagePayloadContext>);

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
  float interception_chance = 0.10f; // 基础偏转 10% (对齐职业设计草案 §3.4)
  bool is_solidified = false;        // Talent 412 不动如山: 偏转时不扣减灵剑数量
  bool trigger_counter = false;      // Talent 470 剑气反震: 偏转/格挡/受击发射反击剑气
  uint8_t counter_sword_count = 5;   // Talent 470 剑气反震: 反击剑气道数 (DoCast 由 Profile 缓存)
  float counter_damage_more = 0.0f;  // Talent 471 以眼还眼: 反击伤害 More 加成 (+20%..80%)
  bool is_lightning_ward = false;    // Talent 472 雷霆法环: 转闪电
  bool is_cold_ward = false;         // Talent 474 霜铠: 转冰霜
  bool counter_spin = false;         // Talent 473 雷贯长虹: 电击与旋转视觉
  float dodge_speed_points = 0.0f;   // Talent 451 借力打力: 闪避后移速/攻速加成点数
  bool dodge_power_boost = false;    // Talent 455 以攻代守: 闪避后 More+20% 并回剑意
  float block_intent_chance = 0.0f;  // Talent 435 剑意格御: 格挡回剑意几率 (15%..45%)
  float block_ward_amount = 0.0f;    // Talent 432 剑盾屏障: 格挡获取护盾 (10..30 Ward)
  // --- 专精节点运行时数值：DoCast 时按机制表烘焙，供 Update/命中结算消费 ---
  float counter_chance_bonus = 0.0f;     // 403 剑压外放: 反击触发几率加成
  float counter_range_bonus = 0.0f;      // 403 剑压外放: 反击剑气射程加成
  float armor_dr_bonus = 0.0f;           // 410 厚积薄发: 护甲减伤效果提升 (百分比点数)
  float armor_dr_per_1000 = 0.0f;        // 410 厚积薄发: 每千护甲额外减伤 (百分比点数)
  float block_effectiveness = 0.0f;      // 431 势不可挡: 格挡效果提升 (百分比点数)
  float last_stand_threshold = 0.0f;     // 413 破釜沉舟: 触发生命阈值 (最大生命占比)
  float last_stand_base_dr = 0.0f;       // 413 破釜沉舟: 低血时基础减伤 (百分比点数)
  float last_stand_armor_mult = 1.0f;    // 413 破釜沉舟: 低血时护甲倍率
  float missing_hp_regen_pct = 0.0f;     // 415 坚韧回生: 每秒按已损生命回复比例
  bool has_blood_barrier = false;        // 433 鲜血壁垒: 以生命换护甲状态
  float perfect_parry_interval = 0.0f;   // 434 无瑕之御: 完全招架充能间隔
  float perfect_parry_timer = 0.0f;      // 434 无瑕之御: 充能计时
  bool perfect_parry_ready = false;      // 434 无瑕之御: 完全招架就绪
  float block_intent_crit = 0.0f;        // 435 剑意格御: 获得剑意时暴击加成 (百分比点数)
  float aftermath_heal_pct = 0.0f;       // 453 流风余韵: 触发后按已损生命回复比例
  float sword_step_dodge_rating = 0.0f;  // 454 御剑闪步: 触发后闪避等级
  float static_interval = 0.0f;          // 472 雷霆法环: 脉冲间隔
  float static_radius = 0.0f;            // 472 雷霆法环: 脉冲半径
  float static_timer = 0.0f;             // 472 雷霆法环: 脉冲计时
  float thunder_frequency_bonus = 0.0f;  // 473 雷贯长虹: 脉冲频率提升
  float shock_slow = 0.0f;               // 473 雷贯长虹: 感电减速比例
  float shock_damage_bonus = 0.0f;       // 473 雷贯长虹: 感电易伤 (百分比点数)
  float frost_radius = 0.0f;             // 474 霜铠: 冰霜风暴半径
  float frost_knockback = 0.0f;          // 474 霜铠: 击退力度
  float permafrost_radius_bonus = 0.0f;  // 475 永冻领域: 半径提升比例
  float exposure_pct = 0.0f;             // 476 元素曝光: 目标元素易伤 (百分比点数)
  float exposure_duration = 0.0f;        // 476 元素曝光: 持续时间
};

// 技能 9 绝影绝剑运行时形态状态 (由 PhantomTrance 行为维护)
struct PhantomTranceComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  PhantomTranceParams params{};
  float duration = 3.0f;
  float remaining = 3.0f;
  float elapsed = 0.0f;
  bool lethal_triggered = false;      // 免死已消耗
  float damage_dealt_accum = 0.0f;    // 984 期间造成伤害统计
  float intent_tick = 0.0f;           // 987 剑意累计
  float mana_tick = 0.0f;             // 914 回蓝累计
  float spiral_tick = 0.0f;           // 983 死亡螺旋节拍
  float pulse_tick = 0.0f;            // 989/972 元素脉冲节拍
  float weaken_tick = 0.0f;           // 913 穿行检测节拍
  float last_stand_buff_value = 0.0f; // 982 最近一次写入的暴伤值
  float enchant_remaining = 0.0f;     // 附魔窗口剩余
  Tag enchant_tag = Tag::None;        // 附魔元素
  bool ending = false;                // 正在执行结束结算
};

// 逆脉禁疗/锁血窗口判定 (治疗点与上限同步统一走此判定)
inline bool IsDeathSealActive(const PhantomTranceComponent &pt) {
  return pt.params.death_seal && pt.remaining > 0.0f && !pt.ending;
}

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
  float bonus_crit = 0.0f;
  float bonus_crit_damage = 0.0f;
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
  bool has_giant_sword = false;   // Talent 330: 巨剑降临
  bool mana_on_hit = false;       // Legacy
  bool immortality_ready = false; // Talent 353: 不灭剑魂就绪标记
  bool has_immortality = false;   // Talent 353: 是否拥有不灭剑魂专精
  float immortality_cooldown = 0.0f; // Talent 353: 90s 冷却计时
  bool melee_orbit = false;       // Talent 351: 剑影环身 (Melee Orbit)
  float ward_dr_per_sword = 0.0f; // Talent 350: 灵剑护体全局减伤/柄
  float block_chance_per_sword = 0.0f; // Talent 352: 反击剑网格挡率/柄
  entt::entity last_skill_hit_target = entt::null; // Talent 314: 集中号令
  bool has_concentrate = false;      // Talent 314: 是否启用集中号令索敌
  float mana_regen_per_sword = 0.0f; // Talent 312: 灵力网络回蓝/柄/秒
  float sword_step_haste = 0.0f;     // Talent 315: 御剑共振攻击频率提升
  float sword_step_intent_chance = 0.0f; // Talent 315: 御剑共振剑意获取几率
  bool has_godspeed = false;      // Talent 313: 神速
  bool has_spell_echo = false;    // Talent 354: 法术共鸣
  int charge_attack_counter = 0;  // Talent 375: 灵剑充能

  // Additional node parameters
  int pts303 = 0;                 // Talent 303: 五行归元 (转换效率 +10%..40%)
  int pts333 = 0;                 // Talent 333: 剑压 (物伤易伤 +5%..15%)
  int pts334 = 0;                 // Talent 334: 碎岩 (击晕 33%..100% & 破甲)
  bool has_array_resonance = false; // Talent 355: 剑阵共鸣 (剑阵内攻速 +50%)
  bool has_fire = false;          // Talent 370: 地火明夷 (火转质 & 必点燃)
  int pts371 = 0;                 // Talent 371: 灼魂剑舞 (点燃持续+持续伤)
  bool has_lightning = false;     // Talent 372: 紫电紫雷 (雷转质 & 感电)
  int pts373 = 0;                 // Talent 373: 雷弧连锁 (感电闪电弧跳跃 1..3)
  int pts374 = 0;                 // Talent 374: 灵剑蚀甲 (降抗 2..8, max 8)
  int pts375 = 0;                 // Talent 375: 灵剑充能 (每 4/3/2 次攻击双倍引爆)

  // DoHit 热路径预烘焙缓存：DoCast 一次性读取 mechanics 写入，DoHit 零 GetFloat、零 std::string 构造
  float taken_phys_pct = 0.0f;    // Talent 333: 剑压 物伤易伤比例 (5%..15%)
  float crush_stun_chance = 0.0f; // Talent 334: 碎岩 击晕几率 (33.33%..100%)
  float crush_stun_duration = 0.0f; // Talent 334: 碎岩 击晕时长
  float ignite_base_duration = 0.0f; // Talent 370: 地火明夷 点燃基础时长
  float ignite_duration_mult = 1.0f; // Talent 371: 灼魂剑舞 点燃时长倍率
  float ignite_dot_per_sword = 0.0f; // Talent 371: 灼魂剑舞 每柄剑持续伤害加成
  float ignite_base_magnitude = 0.0f; // Talent 370: 点燃基础强度
  float chain_damage_pct = 0.0f;  // Talent 373: 雷弧连锁 每跳伤害比例 (15%..45%)
  float chain_radius = 200.0f;    // Talent 373: 雷弧连锁 跳跃半径
  float shred_per_stack = 0.0f;   // Talent 374: 灵剑蚀甲 每层降抗
  float shred_max_stacks = 0.0f;  // Talent 374: 灵剑蚀甲 最大层数
  float shred_duration = 0.0f;    // Talent 374: 灵剑蚀甲 持续时间
  float burst_mult = 0.0f;        // Talent 375: 灵剑充能 爆发倍率
  float final_damage_scale = 1.0f; // 灵剑单发攻击等效伤害缩放 (373/375 伤害基数用)

  // 372 DoCast→DoHit 交接：瞬移落点与静电场参数在命中时消费（未命中不放场）
  bool static_field_pending = false;       // 372: 本次施法待命中释放静电场
  Vector2 static_field_center{0.0f, 0.0f}; // 372: 瞬移落点（静电场落点）
  float static_field_radius = 60.0f;       // 372: 静电场半径（DoCast 缓存）
  float static_field_duration = 2.0f;      // 372: 静电场持续（DoCast 缓存）
};

struct SwordArrayComponent {
  float duration = 5.0f;
  float total_duration = 5.0f;
  float radius = 150.0f;
  float damage_interval = 0.5f;
  float damage_timer = 0.0f;
  float buff_timer = 0.0f;
  entt::entity owner = entt::null;
  bool is_empowered = false;
  uint64_t cast_id = 0;

  Color core_color = {150, 50, 255, 255};
  Color glow_color = {200, 100, 255, 255};

  // Talent Flags (contract-aligned key nodes)
  bool has_slow = false;             // Talent 630
  float slow_magnitude = 0.30f;      // Talent 630 (10%..40%)
  float slow_duration = 2.0f;        // Talent 630
  bool has_armor_shred = false;      // Talent 631
  float armor_shred_chance = 0.50f;  // Talent 631 (50%..200%)
  float shred_duration = 4.0f;       // Talent 631
  float shred_armor_per_stack = 10.0f;// Talent 631
  int shred_max_stacks = 10;         // Talent 631
  bool has_execute = false;          // Talent 633
  float execute_health_threshold_ratio = 0.12f; // Talent 633 (12% per design)
  float boss_more_damage = 1.0f;     // Talent 633 (+20% for Boss)
  float damage_more_mult = 1.0f;     // 从烘焙 profile 继承的总增伤乘数，供 613/614 等硬编码效果力消费，与 650 主伤害路径一致
  bool gain_intent_on_tick = false;  // Talent 652
  float intent_gen_chance = 0.333f;  // Talent 652 (33%..100%)

  // Branch A
  int max_arrays = 1;                // Base: 1, 610: +1, 611: +1
  float resonance_more_mult = 1.0f;  // Talent 612
  bool has_chain_connection = false; // Talent 613 (千丝万缕)
  bool has_dash_detonation = false;  // Talent 614 (流云穿阵)
  float sword_step_frequency_bonus = 0.0f; // Talent 615

  // Branch B
  float weaken_less_damage = 0.0f;   // Talent 632 (6%..18% Less)
  bool has_cage = false;             // Talent 634 (剑阵牢笼)

  // Branch C
  float core_buff_more_damage = 0.0f;// Talent 650 (15%..60% More)
  float mana_regen_per_sec = 0.0f;   // Talent 651 (2..8/s)
  bool is_mobile_aura = false;       // Talent 653 (随身剑垒)
  float cdr_buff = 0.0f;             // Talent 654 (10%..30%)
  float ward_int_mult_per_sec = 0.0f;// Talent 655 (50%..150% Int)

  // Branch D
  bool is_fire_field = false;        // Talent 670
  int burn_stacks = 2;               // Talent 670
  float burn_duration = 3.0f;        // Talent 670
  float base_ignite_magnitude = 15.0f; // Talent 670
  float ignite_more_damage = 0.0f;   // Talent 671
  bool is_lightning_pool = false;    // Talent 672
  int lightning_targets = 2;         // Talent 672 (2-3), Talent 673 (+1..3)
  bool has_chain_lightning = false;  // Talent 673
  float chain_lightning_damage_pct = 0.50f; // Talent 673
  float corrosion_stacks_per_sec = 0.0f; // Talent 674
  float corrosion_resist_per_stack = 2.0f; // Talent 674
  int corrosion_max_stacks = 10;     // Talent 674
  float corrosion_linger_duration = 3.0f; // Talent 674
  bool allow_relocate = false;       // Talent 675
  Tag effective_tag = Tag::Physical;
};

// 标记目标被技能处决斩杀 (例如 633 绝命法场处决)
struct ExecutedTag {};

// 剑阵拥有者增益节拍状态 (避免多阵重叠导致每秒增益重复结算)
struct SwordArrayOwnerBuffState {
  float tick_cooldown = 0.0f;
};

/**
 * @brief 通用地表领域组件 (AreaFieldComponent - 纯 POD / Standard Layout)
 */
struct AreaFieldComponent {
  entt::entity owner = entt::null;     // 归属施法者 (实体消亡与属性伤害结算必须)
  uint64_t cast_id = 0;                // 战斗归因 ID
  uint32_t source_skill_id = 0;        // 技能标识 (兼容外部系统查询)
  float remaining_duration = 0.0f;
  float pulse_interval = 0.25f;
  float timer = 0.0f;
  float radius = 48.0f;
  uint8_t shape_type = 0;              // 0: 圆形, 1: 环形, 2: 旋转切割线

  // 定长 POD 载荷数组 (Zero heap allocation, Standard Layout)
  static constexpr uint8_t kMaxFieldPayloads = 4;
  std::array<PayloadDefinition, kMaxFieldPayloads> payloads{};
  uint8_t payload_count = 0;

  bool operator==(const AreaFieldComponent &) const = default;
};
static_assert(std::is_standard_layout_v<AreaFieldComponent>);
static_assert(std::is_trivially_destructible_v<AreaFieldComponent>);

// 持久场公共头：持久场交付组件共享的归属、归因与脉冲节拍元数据。
// POD / standard-layout / trivially destructible，仅作成员嵌套组合（禁止继承）。
struct PersistentFieldHeader {
  entt::entity owner = entt::null;   // 归属施法者
  float duration = 0.0f;             // 剩余时长（倒计时语义）
  float radius = 0.0f;               // 作用半径
  float tick_interval = 0.25f;       // 脉冲间隔（原 damage_interval）
  float tick_timer = 0.0f;           // 脉冲计时器（原 damage_timer）
  int linked_hit_count = 0;          // 联动命中观测计数，供测试/调试断言，无生产逻辑消费者
  bool has_linked_synergy = false;   // 通用「联动能力」标记
};
static_assert(std::is_standard_layout_v<PersistentFieldHeader>);
static_assert(std::is_trivially_destructible_v<PersistentFieldHeader>);

// 持久场标记：交付系统据此识别并跳过自管理脉冲的持久场，无需具体类型特判。
struct PersistentFieldTag {};

} // namespace NoMoreDay
