# NoMoreDay 技术风险与历史遗留隐患审计报告 (2026-01-22)

## 1. 综述 (Executive Summary)
本次审计利用 `code-risk-analyzer` 对项目 `src` 及 `assets/shaders` 进行了深度扫描。重点评估了战斗系统、AI 行为、物品系统及 GPU 物理引擎的稳定性与逻辑完整性。识别出 3 个高风险逻辑断层和 2 个中风险系统隐患。

---

## 2. 风险详情与修改方向

### 2.1 战斗系统：属性转换逻辑缺失 (Damage Conversion)
- **位置**: `src/game/systems/combat/DamagePipeline.cpp:263`
- **隐患描述**: 天赋系统中的 `ModifierType::Convert` 尚未集成进伤害管道。目前的 `TODO` 表明复杂的链式属性转换（如物理 -> 冰霜 -> 混沌）无法正确生效，将直接导致后期依赖元素转换的角色构建（Build）失效。
- **潜在影响**: 角色强度计算错误，严重影响装备与天赋系统的深度。
- **修改方向**:
    1. 在 `DamagePipeline::Calculate` 的 Instance 处理阶段前，引入递归或多阶段转换函数。
    2. 严格遵循“物理 -> 元素 -> 混沌”的单向转换链，防止无限循环转换。
    3. 确保 `More` 倍率在所有转换完成后统一应用。

### 2.2 AI 系统：刺客背刺逻辑缺陷 (Assassin Backstab & Teleport)
- **位置**: `src/game/systems/ai/EnemyAIBehaviors.cpp:254, 281`
- **隐患描述**:
    1. **物理穿插**: 瞬移（Teleport）仅通过坐标钳制，未进行物理射线检测（Raycast）。
    2. **判定失效**: 背刺仅通过 Buff 实现，未检测“攻击者是否在目标后方”。
- **潜在影响**: AI 永久卡入墙体；玩家可以通过绕前攻击同样享受背刺加成，破坏机制平衡。
- **修改方向**:
    1. **瞬移安全**: 调用物理系统的 `Raycast` 函数，如果目标点在障碍物内，则自动寻找最近的安全落脚点。
    2. **方向判定**: 在 `DamagePipeline` 中计算伤害时，通过 `dot(attacker_forward, defender_forward)` 判定夹角，只有夹角在特定范围（如 > 0.7）时才应用背刺倍率。

### 2.3 物品系统：符文之语类型校验过粗 (Runeword Validation)
- **位置**: `src/game/systems/item/RunewordSystem.cpp:155`
- **隐患描述**: 目前仅支持 `Weapon`, `Armor`, `Shield` 大类校验。
- **潜在影响**: 剑类专属符文（如“悔恨”）可能被错误地镶嵌在斧头或长柄武器上。
- **修改方向**:
    1. 在 `ItemComponent` 中引入 `Subtype` 或 `Tags`。
    2. 修改 `checkForRuneword`，利用 `ItemRegistry` 获取详细标签，实现“Only Swords”或“Only Maces”的精确匹配。

### 2.4 GPU 物理：SSBO 竞态与硬编码 (Physics Compute Race & Hardcoding)
- **位置**: `assets/shaders/physics.compute`
- **隐患描述**:
    1. **竞态**: 并行读取其他实体的 `position` 时可能读取到该帧正在更新的中间值。
    2. **硬编码**: 地图边界 `5000.0` 硬编码。
- **潜在影响**: 高密度战斗下实体异常抖动；地图扩建后出现“无形墙”。
- **修改方向**:
    1. **双缓冲**: 引入 `EntityBufferRead` 和 `EntityBufferWrite`，确保读取的是上一帧的稳定数据（代价是显存翻倍）。
    2. **Uniform 化**: 将 `MAP_BOUNDARY` 通过 Uniform 传入，与 C++ 端 `Constants::World` 同步。

---

## 3. 历史遗留 TODO 统计与清理优先级

| 模块 | 优先级 | 描述 | 建议处理版本 |
| :--- | :--- | :--- | :--- |
| **DamagePipeline** | **URGENT** | 属性转换逻辑 (Conversion) | v0.4.1 (Next) |
| **AI Behaviors** | **HIGH** | 刺客瞬移射线检测 (Raycast) | v0.4.1 (Next) |
| **SaveManager** | **MEDIUM** | 游戏时长统计 (Playtime) | v0.4.2 |
| **Inventory** | **LOW** | 掉落物 Sprite 自动分配 | v0.5.0 |

---

## 4. 结论 (Conclusion)
目前项目的底层框架（MDI 渲染、SSBO 对齐）表现优秀，风险主要集中在**核心玩法逻辑的完整性**以及**跨系统（AI 与物理）的边界处理**上。建议优先解决“刺客 AI 卡墙”和“伤害转换缺失”这两个影响用户体验的核心问题。
