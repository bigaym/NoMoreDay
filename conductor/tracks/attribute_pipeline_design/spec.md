# Attribute Pipeline Design & Implementation Spec (Detailed)

## 1. 核心目标 (Core Objectives)
构建一套 **高性能 (High-Performance)**、**上下文感知 (Context-Aware)** 且 **可视化 (Visual-Ready)** 的属性计算流水线。
目标是在 **1.0ms** (10k 实体) 内完成一轮全量属性重算，并直接驱动 GPU 视觉表现。

## 2. 数据结构定义 (Data Structures)

### 2.1 修饰符 (StatModifier) - 内存布局优化
为了最大化 Cache 命中率，`StatModifier` 必须紧凑且对齐。

```cpp
// 24 Bytes - fits 2 per Cache Line (64B) with room to spare
struct alignas(8) StatModifier {
    float value;                // 4B: 数值
    StatType type;              // 1B: 目标属性类型
    ModifierMode mode;          // 1B: Flat / PercentAdd / PercentMult
    uint16_t _padding;          // 2B: Padding
    Tag required_tags;          // 8B: 生效条件 (Bitmask)
    
    // Helper to check if modifier is active
    bool IsActive(Tag context_tags) const {
        return (required_tags == Tag::None) || 
               ((context_tags & required_tags) == required_tags);
    }
};
```

### 2.2 计算上下文 (CalculationContext)
用于在计算过程中传递环境信息，支持条件修饰符（如 "对流血敌人造成更多伤害"）。

```cpp
struct CalculationContext {
    Tag source_tags;    // 来源标签 (e.g. Melee, Fire, Player)
    Tag target_tags;    // 目标标签 (e.g. Boss, Burning) - 可选，用于预计算时通常为空
    int level;          // 实体等级 (用于缩放公式)
    float delta_time;   // 帧时间
};
```

### 2.3 GPU 视觉属性 (GPUVisualStats) - std430 Layout
该结构体将直接映射到 SSBO，供 Compute Shader 和 Fragment Shader 读取。
必须严格遵循 **std430** 布局规则 (vec4 alignment)。

```cpp
// Shader Binding: layout(std430, binding = 2) buffer VisualStats { ... }
struct alignas(16) GPUVisualStats {
    // Offset 0
    float attack_speed_scale;   // 动画播放速率 (Base = 1.0)
    float size_modifier;        // 模型缩放 (Base = 1.0)
    float weapon_glow_intensity;// 武器发光强度 (0.0 - 5.0)
    uint32_t weapon_glow_color; // Packed RGBA (0xAABBGGRR) - 最高伤害类型决定颜色
    
    // Offset 16
    float outline_width;        // 轮廓线宽度 (Pixel)
    uint32_t outline_color;     // Packed RGBA - 基于 Elite/Boss/Buff 状态
    float _pad0;
    float _pad1;
    
    // Total Size: 32 Bytes
};
```

## 3. 计算流水线逻辑 (Pipeline Logic)

流水线分为 **5 个阶段**，以处理属性依赖和转换。为避免循环依赖，禁止在 Phase 2 之后修改 Primary Stats。

### Phase 0: Initialize (初始化)
*   **Input**: `PrimaryStats` (Base Str/Dex/Int), `CombatStats` (Base HP/Mana).
*   **Action**: 
    1.  将 `CombatStats` 重置为基础值 (Level Scaling Base)。
    2.  准备 `ModifierList` 容器 (使用 `std::pmr::vector` 避免堆分配)。

### Phase 1: Gather & Filter (收集与过滤)
*   **Action**: 遍历所有修饰符源 (`Equipment`, `Buffs`, `Passives`, `Talents`)。
*   **Logic**:
    ```cpp
    for (auto& source : sources) {
        for (auto& mod : source.modifiers) {
            // Bitwise Filter: O(1)
            if (mod.IsActive(ctx.source_tags | ctx.target_tags)) {
                active_modifiers.push_back(mod);
            }
        }
    }
    ```

### Phase 2: Resolve Primary & Conversions (主属性与转换)
*   **Step 2.1**: 应用所有针对 `Strength`, `Dexterity`, `Intelligence`, `Vitality` 的修饰符。
*   **Step 2.2**: 执行 **Stat Conversion** (硬编码顺序以保证无环):
    *   `Armor += Strength * CONSTANTS.STR_TO_ARMOR`
    *   `Health += Vitality * CONSTANTS.VIT_TO_HEALTH`
    *   `Mana += Intelligence * CONSTANTS.INT_TO_MANA`
    *   `CritChance += Dexterity * CONSTANTS.DEX_TO_CRIT_CHANCE`
    *   *Ref: `NoMoreDay::Constants::Attribute`*

### Phase 3: Resolve Secondary Stats (次级属性)
*   **Action**: 应用剩余所有修饰符 (Attack, Defense, Utility)。
*   **Ordering**:
    1.  **Flat**: `Base + Flat1 + Flat2`
    2.  **% Inc**: `* (1.0 + Sum(Increases))`
    3.  **% More**: `* Product(Mores)`
*   **Formula**: `Final = (Base + Flat) * (1 + %Inc) * (More1 * More2)`

### Phase 4: Bake Effective Stats (烘焙有效值)
将理论值转换为实战有效值 (UI 显示用)。
*   **Armor -> DR**: 
    `EffectiveArmorDR = Armor / (Armor + 50 * Level)` (From Scaling Constants)
*   **Dodge -> Chance**:
    `EffectiveDodge = DodgeRating / (DodgeRating + 100 * Level)`
*   **Resistance**:
    `EffectiveRes = Min(RawRes, Cap::RESISTANCE)`

### Phase 5: GPU Sync (视觉同步)
*   **Logic**:
    1.  计算最高元素伤害类型 -> `weapon_glow_color`
        *   Fire -> Red, Cold -> Blue, etc.
    2.  `attack_speed_scale` = `CombatStats.attack_speed`.
    3.  将数据写入 `GPUInstanceData` 的映射缓冲区。

## 4. 关键公式 (Key Formulas)

### 4.1 伤害减免 (Damage Reduction)
```cpp
float effective_armor = stats.armor;
// 护甲减伤公式 (Last Epoch Style)
float armor_mitigation = effective_armor / (effective_armor + 50.0f * level);
stats.damage_reduction = std::min(armor_mitigation, Constants::Combat::Cap::DR);
```

### 4.2 闪避率 (Dodge Chance)
```cpp
// 递减收益公式
float dodge_chance = stats.dodge_rating / (stats.dodge_rating + 100.0f * level);
stats.effective_dodge = std::min(dodge_chance, Constants::Combat::Cap::DODGE);
```

## 5. 验收标准与测试用例 (Acceptance Criteria)

### 5.1 单元测试 (Unit Tests)
1.  **Tag Filter Test**: 验证带有 `Tag::Melee` 的修饰符仅在上下文包含 `Melee` 时生效。
2.  **Order of Operations**: 验证 Flat -> Add -> Mult 的计算顺序正确。
    *   Example: Base 10, +5 Flat, +50% Inc, x2 More -> (10+5) * 1.5 * 2 = 45.
3.  **Conversion Test**: 验证 Strength 增加时，Armor 和 PhysicalDamage 同时增加。

### 5.2 性能指标 (Performance Targets)
| Metric | Baseline (Current) | Target (New Pipeline) |
| :--- | :--- | :--- |
| **Throughput** | ~3ms / 10k entities | **< 1.0ms / 10k entities** |
| **Allocation** | `std::vector` per entity | **0 Alloc (Stack/PMR)** |
| **Cache Miss** | Random Access (Pointer chasing) | **Linear Access (Contiguous)** |

### 5.3 视觉验证 (Visual Validation)
*   **Test**: 装备一把纯火焰伤害的剑。
*   **Result**: 角色攻击动作加快 (攻速属性)，且武器挥动轨迹呈现 **红色 (0xFF0000FF)**。