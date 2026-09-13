#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"

namespace NoMoreDay {

// Enum for Buff/Debuff types to map to icons
enum class BuffType {
    None,
    // Attribute Buffs
    AttackUp,
    DefenseUp,
    SpeedUp,
    CritRateUp,
    CritDamageUp,
    PowerBoost, // Added for consistency with my test code
    
    // Attribute Debuffs
    AttackDown,
    DefenseDown,
    SpeedDown,
    
    // Status Effects
    Stun,
    Freeze,
    Burn,
    Shock,
    Poison,
    Bleed,
    
    // Special
    SwordIntent,  // 剑意
    Shield,       // 护盾
    Invincible,   // 无敌
    Bloodlust,    // 嗜血
    BloodSea,     // 血海
    Hurt,          // 受伤

    // Control Effects
    Root,         // 缠绕
    Silence,      // 沉默

    DamageOverTime
};

inline void to_json(nlohmann::json& j, const BuffType& e) { j = static_cast<int>(e); }
inline void from_json(const nlohmann::json& j, BuffType& e) { e = static_cast<BuffType>(j.get<int>()); }

// BuffEffect 数值类别标识：战斗热路径 (伤害结算/技能命中) 需要按业务类别查找效果，
// 字符串键查找 (Get(const std::string&)) 会产生字符串构造与逐字符比较，违反
// 热路径禁止字符串比较分支的规定 (code_standard §2.1/§7.2)。
// kind 走整数比较；id 字符串仅保留给序列化与日志边界。
enum class BuffKind : uint16_t {
    None = 0,
    QiBrand, // 剑气烙印 (技能2 节点250)：目标受暴击伤害加深
    FateMark, // 天降命印 (技能5 节点512)：目标受万剑归宗伤害增加
    FreeCast, // 免蓝施放 (技能8 节点834 御剑接踵)：下次施放来源技能不消耗法力
    // 元素异常类别：技能1 破阵流按元素异常状态触发衍生效果，热路径需要
    // 按业务类别查找，不能依赖 id 字符串子串匹配。新增值一律追加在末尾，
    // 保持既有已存档 kind 数值稳定。
    Bleed,  // 流血 (AilmentType::Bleed)
    Ignite, // 点燃 (AilmentType::Ignite；legacy BuffType::Burn)
    Chill,  // 冰缓 (AilmentType::Chill；legacy BuffType::SpeedDown)
    Freeze, // 冰冻 (AilmentType::Freeze)
    Slow,   // 减速 (AilmentType::Slow；legacy BuffType::SpeedDown)
};

struct BuffEffect {
    std::string id;             // Unique ID for the buff type (e.g., 'sword_intent', 'rage')
    std::string name;           // Display name
    std::string description;    // Tooltip description
    BuffType type = BuffType::None; // For icon mapping

    // 数值类别 (见 BuffKind)：供 GetByKind 做无字符串的热路径查找，默认无类别
    BuffKind kind = BuffKind::None;
    
    float duration = 0.0f;      // Total duration in seconds (-1 for infinite)
    float remaining = 0.0f;     // Remaining time in seconds
    int stacks = 1;             // Current stack count
    int max_stacks = 1;         // Max stack count
    float tick_interval = 1e10;
    float tick_damage = 0.0f;
    float tick_timer = 0.0f;
    Tag tick_damage_tag = Tag::Poison;
    
    bool is_debuff = false;     // True if it's a debuff (Red border), false for buff (Green/Gold)

    // Managed-ailment structured identity (written by AilmentEngine).
    // AilmentType is defined in game/contracts/CombatEvents.hpp; to avoid a
    // foundation -> contracts include edge (the contracts layer already
    // depends on foundation), the enum is stored as its underlying uint8_t
    // value here and converted at the AilmentEngine boundary, where the
    // layouts are pinned with static_asserts (see AilmentEngine.cpp).
    bool managed_ailment = false; // True when this effect is created by AilmentEngine
    uint8_t ailment_type = 0;     // Underlying value of AilmentType (AilmentType::None == 0)
    float ailment_power = 0.0f;   // Stored per-tick power snapshot (mirrors tick_damage)

    std::vector<StatModifier> modifiers; // Modifiers applied by this buff
    
    // Optional: Source entity ID for attribution
    entt::entity source = entt::null;

    // 来源技能归属 (SkillOnly scope 过滤依据):
    // 0 = 无归属，减抗/增益对全体伤害生效；非 0 = 仅该技能的伤害受益。
    // 由施加方写入，DamagePipeline 在读取抗性时按当前伤害请求的 skill_id 过滤。
    int source_skill_id = 0;

    // Type E 抗性"上限"压制 (技能7 心念灭抗 775)：处于撕裂中心的敌人对应元素
    // 抗性上限被压制 3%...12% (每点 3%)。0 表示无压制；单位为绝对值小数 (如 0.12)，
    // 与 CombatStats.resistances[] 同尺度，结算时从 RESISTANCE_MAX 中扣除。
    // 由交付层写入，DamageMitigationService 结算时消费；本结构体不负责施加逻辑。
    float resist_cap_suppression = 0.0f;
    // 被压制的元素：Tag::None 表示全元素生效，否则仅对匹配的伤害元素生效
    // (如 Tag::Cold / Tag::Lightning)。以 Tag 存储避免热路径字符串比较。
    Tag resist_cap_element = Tag::None;

    // P3-2: DoT 冻结快照载体实体句柄（运行期字段，不序列化）。
    // AilmentEngine 施加异常时创建载体并挂 DamageSnapshotComponent，tick 结算
    // 时优先读取该快照，使施加后攻方增益不再影响本异常的每条 tick。
    entt::entity snapshot_source = entt::null;

    // 技能4 节点455「以攻代守」标记的运行期消费状态（运行期字段，不序列化）：
    // 一次攻击行为内首个实例置为已消费并记录攻击标识，同标识的后续实例继续
    // 生效，换攻击行为时清除。详见 DamagePipeline 消费块。
    bool offensive_guard_consumed = false;
    uint64_t offensive_guard_attack_key = 0;
};

// Custom serialization for BuffEffect to handle entity
inline void to_json(nlohmann::json& j, const BuffEffect& b) {
    j = nlohmann::json{
        {"id", b.id}, {"name", b.name}, {"description", b.description},
        {"type", b.type}, {"duration", b.duration}, {"remaining", b.remaining},
        {"stacks", b.stacks}, {"max_stacks", b.max_stacks}, {"is_debuff", b.is_debuff},
        {"modifiers", b.modifiers},
        {"managed_ailment", b.managed_ailment},
        {"ailment_type", b.ailment_type},
        {"ailment_power", b.ailment_power},
        {"source_skill_id", b.source_skill_id},
        {"kind", b.kind},
        {"resist_cap_suppression", b.resist_cap_suppression},
        {"resist_cap_element", static_cast<uint64_t>(b.resist_cap_element)}
    };
    // source entity is not serialized here as it's runtime transient usually, 
    // or requires UUID mapping which complexifies simple struct serialization.
    // For now, ignore source or set to null on load.
}

inline void from_json(const nlohmann::json& j, BuffEffect& b) {
    j.at("id").get_to(b.id);
    j.at("name").get_to(b.name);
    j.at("description").get_to(b.description);
    j.at("type").get_to(b.type);
    j.at("duration").get_to(b.duration);
    j.at("remaining").get_to(b.remaining);
    j.at("stacks").get_to(b.stacks);
    j.at("max_stacks").get_to(b.max_stacks);
    j.at("is_debuff").get_to(b.is_debuff);
    if (j.contains("modifiers")) j.at("modifiers").get_to(b.modifiers);
    // Managed-ailment fields are optional so that saves written before the
    // structured-identity change load with defaults (managed_ailment=false),
    // which routes them through the legacy "ailment:" id parse fallback.
    if (j.contains("managed_ailment")) j.at("managed_ailment").get_to(b.managed_ailment);
    if (j.contains("ailment_type")) b.ailment_type = j.at("ailment_type").get<uint8_t>();
    if (j.contains("ailment_power")) j.at("ailment_power").get_to(b.ailment_power);
    // 可选字段：旧存档无此字段时默认 source_skill_id=0（无归属，保持旧"全局生效"语义）
    if (j.contains("source_skill_id")) j.at("source_skill_id").get_to(b.source_skill_id);
    // 可选字段：旧存档无此字段时默认 kind=None（无数值类别，热路径查找不命中）
    if (j.contains("kind")) j.at("kind").get_to(b.kind);
    // 可选字段：旧存档无此字段时默认无抗性上限压制 (Type E 默认关闭；element 默认 None=全元素)
    if (j.contains("resist_cap_suppression"))
        j.at("resist_cap_suppression").get_to(b.resist_cap_suppression);
    if (j.contains("resist_cap_element"))
        b.resist_cap_element = static_cast<Tag>(
            j.at("resist_cap_element").get<uint64_t>());
    b.source = entt::null;
    // 快照载体为运行期实体句柄，读档后失效，重置为空。
    b.snapshot_source = entt::null;
}

struct ActiveEffectsComponent {
    std::vector<BuffEffect> effects;
    
    // Helper to add or refresh a buff
    void AddOrRefresh(const BuffEffect& new_effect) {
        for (auto& effect : effects) {
            if (effect.id == new_effect.id) {
                // Refresh duration
                effect.duration = new_effect.duration;
                effect.remaining = new_effect.duration;
                
                // Update metadata in case it changed (e.g. from a stronger version of the same buff)
                effect.name = new_effect.name;
                effect.description = new_effect.description;
                effect.modifiers = new_effect.modifiers;
                // 刷新时同步数值类别与 legacy 类型：旧存档 buff 读入后 kind=None，
                // 重施后必须升级为可被 GetByKind 命中的身份，否则元素状态检测漏检。
                effect.type = new_effect.type;
                effect.kind = new_effect.kind;
                // 刷新时更新来源技能归属：同 id 效果通常由同一技能重施，取最新归属
                effect.source_skill_id = new_effect.source_skill_id;
                // Type E 抗性上限压制随刷新同步，避免重施后仍沿用旧的压制值/元素
                effect.resist_cap_suppression = new_effect.resist_cap_suppression;
                effect.resist_cap_element = new_effect.resist_cap_element;
                // 刷新即视为一次新的挂起：传入的是全新标记（瞬态字段默认 false/0），
                // 消费状态必须重置，否则残留的已消费态会吞掉本次闪避的加成。
                effect.offensive_guard_consumed = new_effect.offensive_guard_consumed;
                effect.offensive_guard_attack_key = new_effect.offensive_guard_attack_key;
                
                // Handle Stacking
                if (effect.stacks < effect.max_stacks) {
                    // If the new effect has multiple stacks, add them but clamp to max
                    effect.stacks = std::min(effect.max_stacks, effect.stacks + new_effect.stacks);
                }
                return;
            }
        }
        effects.push_back(new_effect);
    }

    // 热路径就地刷新：同 id 效果已存在时仅改写首个 modifier 数值与剩余/总时长，
    // 避免每帧构造 BuffEffect（std::string/vector 堆分配）与整表覆盖。命中返回 true。
    bool UpdateModifierValue(std::string_view id, float value, float duration) {
        for (auto& effect : effects) {
            if (std::string_view(effect.id) == id) {
                if (!effect.modifiers.empty()) effect.modifiers[0].value = value;
                effect.duration = duration;
                effect.remaining = duration;
                return true;
            }
        }
        return false;
    }

    // Helper to remove a buff.
    // 以 std::string_view 为键：零临时 std::string、零堆分配，
    // 与 Get(std::string_view) 口径一致（code_standard §2.1/§7.2）。
    void Remove(std::string_view id) {
        std::erase_if(effects, [&](const auto& effect) {
            return std::string_view(effect.id) == id;
        });
    }

    // Helper to remove a buff by enum id
    void Remove(BuffId id) {
        Remove(BuffIdToString(id));
    }
    
    // Helper to get a buff.
    // 以 std::string_view 为键：比较时零堆分配、零临时 std::string，
    // 满足战斗热路径禁止字符串构造/比较开销的规定 (code_standard §2.1/§7.2)。
    // std::string 与 const char* 实参均可隐式转换为 string_view，调用方不受影响。
    BuffEffect* Get(std::string_view id) {
        for (auto& effect : effects) {
            if (std::string_view(effect.id) == id) {
                return &effect;
            }
        }
        return nullptr;
    }

    const BuffEffect* Get(std::string_view id) const {
        for (const auto& effect : effects) {
            if (std::string_view(effect.id) == id) {
                return &effect;
            }
        }
        return nullptr;
    }

    // Helper to get a buff by enum id.
    // BuffIdToString 返回 std::string_view，直接走上面的零分配重载，
    // 不再构造临时 std::string。
    BuffEffect* Get(BuffId id) {
        return Get(BuffIdToString(id));
    }

    // Const variant: read-only lookup by enum id
    const BuffEffect* Get(BuffId id) const {
        return Get(BuffIdToString(id));
    }

    // Helper: whether an effect with the given enum id exists.
    // NOTE: remaining-duration checks (if any) are the caller's responsibility.
    [[nodiscard]] bool Has(BuffId id) const {
        // BuffIdToString 返回 std::string_view，直接比较，避免临时 std::string。
        const std::string_view key = BuffIdToString(id);
        for (const auto& effect : effects) {
            if (std::string_view(effect.id) == key) {
                return true;
            }
        }
        return false;
    }

    // 数值类别查找：线性遍历逐个比较整数 kind，无字符串构造与比较，
    // 供战斗热路径 (如 DamagePipeline 结算烙印层数) 替代字符串键 Get()。
    BuffEffect* GetByKind(BuffKind kind) {
        for (auto& effect : effects) {
            if (effect.kind == kind) {
                return &effect;
            }
        }
        return nullptr;
    }

    // Const variant: read-only lookup by numeric kind
    const BuffEffect* GetByKind(BuffKind kind) const {
        for (const auto& effect : effects) {
            if (effect.kind == kind) {
                return &effect;
            }
        }
        return nullptr;
    }

    // 数值类别移除：与 GetByKind 配套的整数比较路径，避免字符串键
    void RemoveByKind(BuffKind kind) {
        std::erase_if(effects,
                      [&](const auto& effect) { return effect.kind == kind; });
    }

    // 数值类别 + 来源技能过滤移除：仅清除 kind 匹配、且来源为 source_skill_id
    // 或无归属 (source_skill_id==0) 的效果。无归属 0 视为通配——对任意施法者
    // 均可见并一并清除，与 TryCast 中 `source_skill_id == 0 ||
    // source_skill_id == slot.id` 的匹配语义保持一致，避免整类误清。
    void RemoveByKind(BuffKind kind, uint32_t source_skill_id) {
        std::erase_if(effects, [&](const auto& effect) {
            return effect.kind == kind &&
                   (effect.source_skill_id == 0 ||
                    effect.source_skill_id == static_cast<int>(source_skill_id));
        });
    }
    
    // swordStepDrainMult: 御剑步（988 御剑化影）自然衰减倍率，仅影响 SwordStep。
    void Update(float dt, float swordStepDrainMult = 1.0f) {
        std::erase_if(effects, [&](auto& effect) {
            if (effect.duration < 0) return false; // Infinite
            if (swordStepDrainMult != 1.0f &&
                effect.id == BuffIdToString(BuffId::SwordStep)) {
                effect.remaining -= dt * swordStepDrainMult;
            } else {
                effect.remaining -= dt;
            }
            return effect.remaining <= 0;
        });
    }
};

inline void to_json(nlohmann::json& j, const ActiveEffectsComponent& c) {
    j = nlohmann::json{{"effects", c.effects}};
}

inline void from_json(const nlohmann::json& j, ActiveEffectsComponent& c) {
    j.at("effects").get_to(c.effects);
}

} // namespace NoMoreDay
