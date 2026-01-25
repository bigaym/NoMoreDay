# 核心系统逻辑缺陷修复 - 技术规格

## 1. 概述

涵盖战斗系统、AI 系统、物品系统及 GPU 物理引擎的四项核心逻辑隐患。

### 1.1 设计目标

| 目标 | 描述 | 优先级 |
|-----|-----|-------|
| **伤害转换逻辑 (Conversion)** | 实现递归/多阶段元素转换链，遵循"物理 → 元素 → 混沌"单向规则 | **P0 URGENT** |
| **刺客 AI 安全传送 (Teleport Raycast)** | 瞬移目标点校验障碍物，防止卡墙 | **P0 HIGH** |
| **刺客背刺方向判定 (Backstab Angle)** | 通过向量夹角判断攻击者是否在目标后方 | **P1 MEDIUM** |
| **符文之语精确类型校验 (Runeword Tags)** | 基于 `Subtype/Tags` 实现细粒度装备匹配（如仅剑类） | **P2 LOW** |
| **GPU 物理双缓冲 (Physics Double Buffer)** | 隔离读写 SSBO，消除并行更新竞态 | **P1 MEDIUM** |

---

## 2. 风险1: 战斗系统 - 属性转换逻辑缺失

### 2.3 技术方案

```cpp
// Common.hpp - 新增转换链约束常量
namespace NoMoreDay::Constants::Combat::Conversion {
    // 转换优先级顺序（单向：idx 小 → idx 大）
    // Physical(0) → Lightning(3) → Cold(2) → Fire(1) → Poison(4) → Shadow(5)
    constexpr std::array<int, 6> CONVERSION_ORDER = {0, 3, 2, 1, 4, 5};
    // 最大递归深度
    constexpr int MAX_CONVERSION_DEPTH = 8;
}
```

---

## 3. 风险2: AI 系统 - 刺客背刺与瞬移缺陷

### 3.3 技术方案

#### 3.3.1 安全瞬移 (Teleport Raycast)
- 利用 `TilemapCollisionSystem::IsPositionWalkable(Position)` 进行障碍物检测。

#### 3.3.2 背刺角度判定
- 计算公式：`dot(attackDir, defenderFacing) > BACKSTAB_DOT_THRESHOLD` (0.5 ~ 0.7)。

---

## 5. 风险4: GPU 物理 - SSBO 竞态与硬编码

### 5.2 技术方案

#### 5.2.1 双缓冲 SSBO
```cpp
// GPUPhysicsSystem.hpp
class GPUPhysicsSystem {
    uint32_t m_entityBufferRead;  // Binding 1
    uint32_t m_entityBufferWrite; // Binding 7
    void SwapBuffers();
};
```

---

## 6. 验收标准
- [x] REQ-CONV-1: 伤害转换支持 ≥3 级链式
- [x] REQ-AI-1: 刺客瞬移后 100% 位于可行走区域
- [x] REQ-RUNE-1: 剑类符文无法镶嵌在非剑类武器上
- [x] REQ-GPU-1: 500+ 实体物理模拟无抖动