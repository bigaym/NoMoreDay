# 核心系统逻辑缺陷修复 - 实现计划

## Track 状态

| 属性 | 值 |
|-----|---|
| **Track ID** | `core_risk_remediation_20260122` |
| **创建时间** | 2026-01-22 |
| **优先级** | **URGENT** |
| **预估工期** | 4-5 天 |
| **触发依据** | `2026-01-22_comprehensive_risk_audit.md` 第二部分 |
| **状态** | 🚧 IN PROGRESS |

---

## 依赖关系图

```
Phase 1 (Infrastructure) ─┬─> Phase 2 (The Conversion) ─┐
                          │                              │
                          └─> Phase 3 (The Assassin) ────┼─> Phase 5 (Testing)
                                                         │
Phase 4 (The GPU Barrier) ───────────────────────────────┘
                                                         
Phase 6 (The Rune Tags) ──> [独立，可并行]
```

---

## 任务清单

### Phase 1: 基础设施准备 (Infrastructure) - "The Constants"

> **目标**: 建立所有修复所需的常量、枚举和工具函数

| ID | 任务 | 状态 | 预估 | 依赖 |
|----|-----|------|-----|------|
| 1.1 | 在 `Common.hpp` 添加 `Constants::Combat::Conversion` 命名空间 | ⬜ TODO | 0.5h | - |
| 1.2 | 添加 `Conversion::CONVERSION_ORDER` 和 `IsValidConversion()` | ⬜ TODO | 0.5h | 1.1 |
| 1.3 | 添加 `Constants::AI::Assassin::BACKSTAB_DOT_THRESHOLD` | ⬜ TODO | 0.25h | - |
| 1.4 | 确认 `Constants::World::MAP_BOUNDARY` 存在并使用正确值 | ⬜ TODO | 0.25h | - |
| 1.5 | 添加 `WeaponSubtype` 枚举到 `ItemComponent.hpp` | ⬜ TODO | 0.5h | - |

**Phase 1 验证**: 编译通过，无新 Warning

---

### Phase 2: 伤害转换重构 (Damage Conversion) - "The Chainbreaker"

> **目标**: 实现多阶段链式伤害转换，遵循单向转换规则
> **风险等级**: 🔴 HIGH (核心战斗逻辑)

| ID | 任务 | 状态 | 预估 | 依赖 |
|----|-----|------|-----|------|
| 2.1 | 重构 `DamagePipeline::Calculate` 的 Instance 处理循环 | ⬜ TODO | 2h | 1.1, 1.2 |
| 2.2 | 实现迭代式 Conversion 处理（替代当前的单遍历） | ⬜ TODO | 2h | 2.1 |
| 2.3 | 添加 `IsValidConversion()` 校验，违规时输出 Warning | ⬜ TODO | 0.5h | 2.2 |
| 2.4 | 统一 `More` 倍率应用点至结算阶段末尾 | ⬜ TODO | 1h | 2.2 |
| 2.5 | 添加 `MAX_CONVERSION_DEPTH` 熔断机制 | ⬜ TODO | 0.5h | 2.2 |
| 2.6 | 编写 `DamagePipelineConversionTest.cpp` | ⬜ TODO | 2h | 2.1-2.5 |

**Phase 2 验证目标**:
- [ ] 测试用例: 物理 → 冰 → 混沌 转换链正确生效
- [ ] 测试用例: 火 → 冰 → 火 循环转换触发 Warning 并熔断
- [ ] 回归: 现有战斗测试全部通过

---

### Phase 3: 刺客 AI 修复 (Assassin AI) - "The Shadow's Edge"

> **目标**: 修复瞬移卡墙和背刺判定缺失
> **风险等级**: 🟠 MEDIUM (AI 行为)

| ID | 任务 | 状态 | 预估 | 依赖 |
|----|-----|------|-----|------|
| 3.1 | 创建 `TilemapCollisionSystem::IsPositionWalkable()` | ⬜ TODO | 1h | - |
| 3.2 | 实现 `FindSafeTeleportTarget()` 辅助函数 | ⬜ TODO | 1.5h | 3.1 |
| 3.3 | 重构 `ExecuteBackstab()` 使用安全瞬移 | ⬜ TODO | 1h | 3.2 |
| 3.4 | 实现 `CalculateBackstabMultiplier()` 方向判定 | ⬜ TODO | 1h | 1.3 |
| 3.5 | 集成方向判定到 `DamagePipeline` 或 `CombatSystem` | ⬜ TODO | 1h | 3.4 |
| 3.6 | 编写 `BackstabMechanicsTest.cpp` | ⬜ TODO | 1.5h | 3.3-3.5 |

**Phase 3 验证目标**:
- [ ] 手动测试: 刺客瞬移 50 次无卡墙
- [ ] 测试用例: dot = 0.4 时背刺倍率 = 1.0
- [ ] 测试用例: dot = 0.6 时背刺倍率 = BACKSTAB_DAMAGE_MULT

---

### Phase 4: GPU 物理双缓冲 (GPU Physics) - "The Barrier"

> **目标**: 消除 SSBO 竞态条件，参数化地图边界
> **风险等级**: 🟠 MEDIUM (渲染管线)

| ID | 任务 | 状态 | 预估 | 依赖 |
|----|-----|------|-----|------|
| 4.1 | 修改 `GPUPhysicsSystem` 添加双缓冲 SSBO 管理 | ⬜ TODO | 1.5h | 1.4 |
| 4.2 | 实现 `SwapBuffers()` 帧末缓冲交换 | ⬜ TODO | 0.5h | 4.1 |
| 4.3 | 重构 `physics.compute` 使用 Read/Write 缓冲分离 | ⬜ TODO | 1h | 4.1 |
| 4.4 | 添加 `mapBoundary` Uniform 并传入 C++ 常量 | ⬜ TODO | 0.5h | 4.3 |
| 4.5 | 移除 Shader 中所有硬编码 `5000.0` | ⬜ TODO | 0.25h | 4.4 |
| 4.6 | 编写 `GPUPhysicsRaceTest.cpp` (高密度模拟) | ⬜ TODO | 2h | 4.1-4.5 |

**Phase 4 验证目标**:
- [ ] `ScopedTimer("GPU Physics")` < 基线（无回归）
- [ ] 500 实体高密度模拟无视觉抖动
- [ ] 修改 `MAP_BOUNDARY` 常量后无需重新编译 Shader

---

### Phase 5: 集成测试 (Integration Testing) - "The Crucible"

> **目标**: 确保所有修复协同工作，无回归

| ID | 任务 | 状态 | 预估 | 依赖 |
|----|-----|------|-----|------|
| 5.1 | 运行全量测试套件 `NoMoreDayTests.exe` | ⬜ TODO | 0.5h | 2.6, 3.6 |
| 5.2 | 手动测试: 带转换的刺客战斗流 | ⬜ TODO | 1h | 5.1 |
| 5.3 | 性能回归测试: 战斗 + 物理管线 | ⬜ TODO | 1h | 4.6 |
| 5.4 | 修复发现的回归问题 | ⬜ TODO | TBD | 5.1-5.3 |

---

### Phase 6: 符文之语精确校验 (Runeword Tags) - "The Arcanist"

> **目标**: 实现装备子类型校验
> **风险等级**: 🟢 LOW (独立子系统，可延后)
> **可并行**: 此 Phase 与 Phase 2-5 无直接依赖

| ID | 任务 | 状态 | 预估 | 依赖 |
|----|-----|------|-----|------|
| 6.1 | 扩展 `RunewordDefinition` 添加 `allowedSubtypes` | ⬜ TODO | 0.5h | 1.5 |
| 6.2 | 更新 `loadRunewords()` 解析新字段 | ⬜ TODO | 0.5h | 6.1 |
| 6.3 | 重构 `checkForRuneword()` 添加子类型匹配 | ⬜ TODO | 1h | 6.2 |
| 6.4 | 更新 `assets/data/runewords.json` 添加测试数据 | ⬜ TODO | 0.5h | 6.3 |
| 6.5 | 编写 `RunewordValidationTest.cpp` | ⬜ TODO | 1h | 6.3, 6.4 |
| 6.6 | 更新 `ItemFactory` 设置 `weaponSubtype` | ⬜ TODO | 1h | 1.5 |

**Phase 6 验证目标**:
- [ ] 测试用例: 剑类符文无法镶嵌在斧头上
- [ ] 测试用例: 无子类型限制的符文可镶嵌在任意匹配大类装备上

---

## 风险登记

| 风险 | 影响 | 概率 | 缓解措施 |
|-----|-----|------|---------|
| 转换重构破坏现有平衡 | HIGH | MEDIUM | 保留旧代码路径作为 Feature Flag 回退 |
| 双缓冲 SSBO 显存翻倍 | MEDIUM | HIGH | 确认当前 VRAM 余量，必要时减少最大实体数 |
| Tilemap 碰撞检测性能 | LOW | LOW | 使用空间分区加速，或限制每帧瞬移次数 |
| 子类型字段存档兼容性 | LOW | LOW | `weaponSubtype` 默认值 `None` 兼容旧存档 |

---

## 工时汇总

| Phase | 预估工时 |
|-------|---------|
| Phase 1 | 2h |
| Phase 2 | 8h |
| Phase 3 | 7h |
| Phase 4 | 5.75h |
| Phase 5 | 2.5h + TBD |
| Phase 6 | 4.5h |
| **总计** | **~30h (4-5 工作日)** |

---

## 变更日志

| 日期 | 变更内容 |
|-----|---------| 
| 2026-01-22 | 创建初始计划，基于风险审计报告 |
