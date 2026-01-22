# 核心系统逻辑缺陷修复 - 技术规格

## 1. 概述

本规格书对应 `2026-01-22_comprehensive_risk_audit.md` 中识别出的 **第二部分：风险详情**，涵盖战斗系统、AI 系统、物品系统及 GPU 物理引擎的四项核心逻辑隐患。

### 1.1 设计目标

| 目标 | 描述 | 优先级 |
|-----|-----|-------|
| **伤害转换逻辑 (Conversion)** | 实现递归/多阶段元素转换链，遵循"物理 → 元素 → 混沌"单向规则 | **P0 URGENT** |
| **刺客 AI 安全传送 (Teleport Raycast)** | 瞬移目标点校验障碍物，防止卡墙 | **P0 HIGH** |
| **刺客背刺方向判定 (Backstab Angle)** | 通过向量夹角判断攻击者是否在目标后方 | **P1 MEDIUM** |
| **符文之语精确类型校验 (Runeword Tags)** | 基于 `Subtype/Tags` 实现细粒度装备匹配（如仅剑类） | **P2 LOW** |
| **GPU 物理双缓冲 (Physics Double Buffer)** | 隔离读写 SSBO，消除并行更新竞态 | **P1 MEDIUM** |
| **地图边界参数化 (Boundary Uniforms)** | 消除 Shader 中硬编码的 `5000.0` 边界值 | **P2 LOW** |

---

## 2. 风险1: 战斗系统 - 属性转换逻辑缺失

### 2.1 问题陈述

**当前状态**：`DamagePipeline.cpp:174-237` 已实现 `Convert` 和 `GainExtra` 修改器的采集和初步处理，但存在以下限制：
1. **单级转换**：仅支持直接转换（A → B），无法处理链式转换（A → B → C）。
2. **More 倍率时序错误**：`More` 倍率应在所有转换完成后统一应用，当前代码在 `instances[i].amount -= actual_conv_total` 后立即进入下一元素类型处理，未等待链式完成。
3. **循环转换风险**：若数据配置错误（如 Fire → Cold → Fire），当前代码无熔断机制。

### 2.2 目标指标

| 指标 | 当前 | 目标 |
|-----|-----|-----|
| 支持转换链深度 | 1 | ≥3（物理 → 元素 → 混沌） |
| 循环检测 | 无 | 编译时/运行时熔断 |
| More 倍率应用点 | 分散 | 统一在结算前 |

### 2.3 技术方案

```cpp
// Common.hpp - 新增转换链约束常量
namespace NoMoreDay::Constants::Combat::Conversion {
    // 转换优先级顺序（单向：idx 小 → idx 大）
    // Physical(0) → Lightning(3) → Cold(2) → Fire(1) → Poison(4) → Shadow(5)
    constexpr std::array<int, 6> CONVERSION_ORDER = {0, 3, 2, 1, 4, 5};
    
    // 最大递归深度（防止配置错误导致无限循环）
    constexpr int MAX_CONVERSION_DEPTH = 8;
    
    // 合法转换方向检查 (from_idx, to_idx) -> from 必须 < to
    inline constexpr bool IsValidConversion(int from_idx, int to_idx) {
        // 在 CONVERSION_ORDER 中，from 的位置必须在 to 之前
        int from_pos = -1, to_pos = -1;
        for (int i = 0; i < 6; ++i) {
            if (CONVERSION_ORDER[i] == from_idx) from_pos = i;
            if (CONVERSION_ORDER[i] == to_idx) to_pos = i;
        }
        return from_pos < to_pos;
    }
}
```

**核心算法**：在 `DamagePipeline::Calculate` 中实现多阶段迭代处理：

```cpp
// Phase 1: 标记所有待处理的 Instance
// Phase 2: 按 CONVERSION_ORDER 迭代处理每个元素类型
// Phase 3: 对新生成的 Instance 递归调用处理（最多 MAX_CONVERSION_DEPTH 次）
// Phase 4: 统一应用 More 倍率
```

### 2.4 修改文件清单

| 文件 | 修改类型 | 内容 |
|-----|---------|-----|
| `src/game/components/Common.hpp` | ADD | `Conversion` 常量命名空间 |
| `src/game/systems/combat/DamagePipeline.cpp` | MODIFY | 重构 Instance 处理循环 |
| `tests/unit/DamagePipelineConversionTest.cpp` | ADD | 链式转换单元测试 |

---

## 3. 风险2: AI 系统 - 刺客背刺与瞬移缺陷

### 3.1 问题陈述

**位置**：`EnemyAIBehaviors.cpp:254-262`

1. **瞬移穿墙**：`ExecuteBackstab` 使用 `std::clamp` 将目标点限制在地图边界内，但未考虑墙体或障碍物。高密度战斗中刺客可能直接穿透地形。
2. **背刺判定缺失**：当前通过应用临时 Buff `assassin_backstab_boost` 实现增伤，但未验证"攻击者是否在目标后方"。任何方向的攻击都能获得背刺加成。

### 3.2 目标指标

| 指标 | 当前 | 目标 |
|-----|-----|-----|
| 瞬移安全校验 | 边界 clamp | Raycast 或回退安全点 |
| 背刺角度判定 | 无 | dot(attacker_fwd, defender_fwd) > 0.5 |
| 卡墙发生率 | 未知（高风险） | 0% |

### 3.3 技术方案

#### 3.3.1 安全瞬移 (Teleport Raycast)

```cpp
// EnemyAIBehaviors.cpp - 新增安全瞬移辅助函数
static Position FindSafeTeleportTarget(
    entt::registry& registry,
    const Position& current, 
    const Position& desired,
    float fallbackRadius = 50.0f
) {
    // 1. 调用物理系统的简单 LineOfSight 检测
    // GPUPhysicsSystem 暂无 Raycast，使用 CPU 侧 Tilemap 碰撞检测
    // 或者在 PhysicsWorld 中添加简单的 AABB 线段检测
    
    // 2. 若 desired 不可达，在 current 周围搜索最近的安全点
    // 使用螺旋搜索或预计算的安全点网格
    
    // 3. Fallback：返回 current（传送失败，留在原地）
}
```

由于当前项目 GPU 物理不支持 Raycast，采用 **简化方案**：
- 利用现有 `Tilemap` 进行障碍物检测
- 添加 `TilemapCollisionSystem::IsPositionWalkable(Position)` 接口

#### 3.3.2 背刺角度判定

```cpp
// DamagePipeline.cpp - 在 Calculate() 或 CombatSystem::ApplyDamage() 中
float CalculateBackstabMultiplier(
    entt::registry& registry,
    entt::entity attacker,
    entt::entity defender
) {
    auto* attPos = registry.try_get<Position>(attacker);
    auto* attVel = registry.try_get<Velocity>(attacker); // 用速度近似面向
    auto* defPos = registry.try_get<Position>(defender);
    auto* defVel = registry.try_get<Velocity>(defender);
    
    if (!attPos || !defPos) return 1.0f;
    
    // 计算攻击者攻击方向（attacker -> defender）
    Vector2 attackDir = Normalize({defPos->x - attPos->x, defPos->y - attPos->y});
    
    // 计算防御者面向（使用速度方向，若静止则使用上一帧面向缓存）
    Vector2 defenderFacing = GetEntityFacing(registry, defender);
    
    // dot product: > 0.7 表示从背后（同向）
    float dot = attackDir.x * defenderFacing.x + attackDir.y * defenderFacing.y;
    
    using namespace NoMoreDay::Constants::AI::Assassin;
    if (dot > BACKSTAB_DOT_THRESHOLD) { // 0.5 ~ 0.7
        return BACKSTAB_DAMAGE_MULT; // 配置值
    }
    return 1.0f;
}
```

### 3.4 修改文件清单

| 文件 | 修改类型 | 内容 |
|-----|---------|-----|
| `src/game/components/Common.hpp` | MODIFY | 添加 `Assassin::BACKSTAB_DOT_THRESHOLD` |
| `src/game/systems/ai/EnemyAIBehaviors.cpp` | MODIFY | 重构 `ExecuteBackstab`，添加安全传送 |
| `src/game/systems/combat/DamagePipeline.cpp` | MODIFY | 添加背刺方向判定 |
| `src/game/world/TilemapCollisionSystem.hpp` | ADD | `IsPositionWalkable()` 接口 |
| `tests/unit/BackstabMechanicsTest.cpp` | ADD | 方向判定单元测试 |

---

## 4. 风险3: 物品系统 - 符文之语类型校验

### 4.1 问题陈述

**位置**：`RunewordSystem.cpp:148-157`

`checkForRuneword` 仅通过 `ItemComponent.type`（大类：Weapon/Armor/Shield）校验装备类型，无法区分子类（Sword/Mace/Axe/Staff）。这导致：
- 剑类专属符文（如"悔恨"）可镶嵌在斧头上
- 法杖专属符文可用于匕首

### 4.2 技术方案

```cpp
// ItemComponent.hpp - 新增子类型枚举
enum class WeaponSubtype : uint8_t {
    None = 0,
    Sword,
    Axe,
    Mace,
    Staff,
    Dagger,
    Bow,
    Wand
};

struct ItemComponent {
    // ... existing fields
    WeaponSubtype weaponSubtype = WeaponSubtype::None; // NEW
};
```

```cpp
// RunewordDefinition - 扩展允许类型
struct RunewordDefinition {
    // ... existing
    std::vector<WeaponSubtype> allowedSubtypes; // NEW: e.g., {Sword, Dagger}
};
```

```cpp
// checkForRuneword - 增加子类型校验
if (!word.allowedSubtypes.empty()) {
    bool subtypeMatch = std::find(
        word.allowedSubtypes.begin(),
        word.allowedSubtypes.end(),
        item.weaponSubtype
    ) != word.allowedSubtypes.end();
    
    if (!subtypeMatch) continue;
}
```

### 4.3 修改文件清单

| 文件 | 修改类型 | 内容 |
|-----|---------|-----|
| `src/game/data/ItemComponent.hpp` | MODIFY | 添加 `WeaponSubtype` 枚举和字段 |
| `src/game/systems/item/RunewordSystem.hpp` | MODIFY | 扩展 `RunewordDefinition` |
| `src/game/systems/item/RunewordSystem.cpp` | MODIFY | 更新 `checkForRuneword` 校验 |
| `assets/data/runewords.json` | MODIFY | 添加 `allowed_subtypes` 字段 |
| `tests/unit/RunewordValidationTest.cpp` | ADD | 子类型校验测试 |

---

## 5. 风险4: GPU 物理 - SSBO 竞态与硬编码

### 5.1 问题陈述

**位置**：`physics.compute`

1. **竞态条件**：`entities[neighborId].position` 可能读取到当前帧正在写入的中间值。
2. **硬编码边界**：`5000.0` 直接写死在 Shader 中，与 `Constants::World::MAP_BOUNDARY` 不同步。

### 5.2 技术方案

#### 5.2.1 双缓冲 SSBO

```cpp
// GPUPhysicsSystem.hpp
class GPUPhysicsSystem {
    // 双缓冲：帧 N 读 Read、写 Write；帧 N+1 交换角色
    uint32_t m_entityBufferRead;  // Binding 1
    uint32_t m_entityBufferWrite; // Binding 7 (新增)
    
    void SwapBuffers() {
        std::swap(m_entityBufferRead, m_entityBufferWrite);
    }
};
```

```glsl
// physics.compute
layout(std430, binding = 1) buffer EntityBufferRead { Entity entitiesRead[]; };
layout(std430, binding = 7) buffer EntityBufferWrite { Entity entitiesWrite[]; };

void main() {
    // 读取上一帧稳定数据
    vec2 nPos = entitiesRead[neighborId].position;
    
    // 写入当前帧结果
    entitiesWrite[id].position = pos;
    entitiesWrite[id].velocity = vel;
}
```

#### 5.2.2 边界 Uniform 化

```glsl
// physics.compute
uniform float mapBoundary; // 替代硬编码 5000.0

// 使用
if (pos.x > mapBoundary) { pos.x = mapBoundary; vel.x *= -0.5; }
```

```cpp
// GPUPhysicsSystem.cpp
glUniform1f(glGetUniformLocation(m_physicsShader, "mapBoundary"), 
            Constants::World::MAP_BOUNDARY);
```

### 5.3 修改文件清单

| 文件 | 修改类型 | 内容 |
|-----|---------|-----|
| `assets/shaders/physics.compute` | MODIFY | 双缓冲 + Uniform 边界 |
| `src/engine/physics/GPUPhysicsSystem.hpp` | MODIFY | 双缓冲管理 |
| `src/engine/physics/GPUPhysicsSystem.cpp` | MODIFY | 缓冲交换逻辑 |
| `src/game/components/Common.hpp` | VERIFY | 确认 `MAP_BOUNDARY` 存在 |
| `tests/integration/GPUPhysicsRaceTest.cpp` | ADD | 并发写入验证测试 |

---

## 6. 验收标准

- [ ] **REQ-CONV-1**: 伤害转换支持 ≥3 级链式（物理 → 冰 → 混沌）
- [ ] **REQ-CONV-2**: 循环转换配置触发编译期/运行时警告
- [ ] **REQ-AI-1**: 刺客瞬移后 100% 位于可行走区域
- [ ] **REQ-AI-2**: 背刺仅在攻击者处于防御者背后时触发（dot > 0.5）
- [ ] **REQ-RUNE-1**: 剑类符文无法镶嵌在非剑类武器上
- [ ] **REQ-GPU-1**: 高密度（500+ 实体）物理模拟无抖动
- [ ] **REQ-GPU-2**: 地图边界变更后无需修改 Shader 代码

---

## 7. 测试矩阵

| 测试类型 | 文件 | 覆盖内容 |
|---------|-----|---------| 
| Unit | `DamagePipelineConversionTest.cpp` | 链式转换、循环检测 |
| Unit | `BackstabMechanicsTest.cpp` | 角度阈值边界 |
| Unit | `RunewordValidationTest.cpp` | 子类型匹配 |
| Integration | `GPUPhysicsRaceTest.cpp` | 双缓冲正确性 |
| Performance | N/A | 转换管线无性能回归 |
