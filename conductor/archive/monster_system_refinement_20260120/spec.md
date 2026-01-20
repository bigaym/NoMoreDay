# Monster System Refinement Spec

**Track ID:** `monster_system_refinement_20260120`
**Status:** DRAFTing
**Related Docs:** `设计文档/怪物词缀设计.md`, `设计文档/怪物和AI设计.md`

## 1. 现状回顾 (Audit Summary)
目前的怪物系统大框架已经建立，但在代码健壮性、设计一致性和演进深度上存在以下问题：
1. **字符串硬编码**: `NemesisGenerator` 使用 `if-else` 匹配词缀字符串，效率低且维护困难。
2. **缺乏动态缩放**: 词缀数值（冷却、半径）全是 `static constexpr`，无法随宿敌级别（Evolution Tier）增强。
3. **设计鸿沟**: `Homing` (追踪) 和 `Burst Counter` (爆发反制) 尚未实现。
4. **AI 冲突**: 宿敌的“全图猎杀”特性与 `AISystem` 的 `Dormancy` (休眠) 逻辑存在冲突。

## 2. 核心调整目标 (Core Objectives)

### 2.1 数据驱动解耦
*   在 `MonsterAffixRegistry` 中引入 `Name <-> Type` 映射。
*   将硬编码在 `MonsterAffixSystem.hpp` 中的战斗常量转移至 `MonsterAffixRegistry.hpp` 中。

### 2.2 词缀能力扩展
*   **Homing (追踪)**: 当投射物由拥有 `Accurate` 词缀的怪发射时，自动获得追踪能力。
*   **PhaseShield (无敌相)**: 宿敌受到高额爆发伤害或生命降至阈值时，触发 2s 强制无敌。

### 2.3 宿敌演进增强
*   词缀数值计算公式：$Value_{runtime} = Value_{base} \times (Scale_{tier})^{Tier-1}$。
*   宿敌将忽略 `Dormancy` 逻辑，即使在屏幕外也会持续移动并追踪玩家。

## 3. 技术契约 (Technical Contract)

### 3.1 MonsterAffixRegistry 增强
```cpp
struct MonsterAffixDef {
    // ... 现有字段 ...
    float baseCooldown = 0.0f;
    float baseRadius = 0.0f;
    float scalingFactor = 1.05f; // 每级增强 5%
};
```

### 3.2 AISystem 逻辑修正
*   在 `AISystem::update` 中，检查实体是否拥有 `NemesisTag`。
*   若有，跳过 `DormancyThreshold` 判定。

### 3.3 宿敌生成器优化
*   移除 `if-else` 字符串比对。
*   使用 `MonsterAffixRegistry::GetTypeFromName` 动态转换。

## 4. 风险评估
*   **性能**: 降低 `Dormancy` 覆盖面可能对低端 CPU 造成压力，宿敌数量必须严格限制为 1 个。
*   **平衡**: 动态缩放如果不加 Limit，后期可能出现“无限筑墙”或“无极速”等极端情况。
