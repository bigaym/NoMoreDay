# 核心系统逻辑缺陷修复 - 实现计划

| 属性 | 值 |
|-----|---|
| **Track ID** | `core_risk_remediation_20260122` |
| **优先级** | **URGENT** |
| **状态** | 🚧 IN PROGRESS |

---

## 任务清单

### Phase 1: 基础设施准备 (Infrastructure)
- [ ] 1.1 `Common.hpp` 添加 `Conversion` 常量
- [ ] 1.5 `WeaponSubtype` 枚举添加

---

### Phase 2: 伤害转换重构 (Damage Conversion) - "The Chainbreaker"
- [ ] 2.1 重构 `DamagePipeline::Calculate` 的 Instance 处理循环
- [ ] 2.2 实现迭代式 Conversion 处理
- [ ] 2.5 添加 `MAX_CONVERSION_DEPTH` 熔断机制

---

### Phase 3: 刺客 AI 修复 (Assassin AI) - "The Shadow's Edge"
- [ ] 3.1 创建 `TilemapCollisionSystem::IsPositionWalkable()`
- [ ] 3.2 实现 `FindSafeTeleportTarget()` 辅助函数
- [ ] 3.4 实现 `CalculateBackstabMultiplier()` 方向判定

---

### Phase 4: GPU 物理双缓冲 (GPU Physics) - "The Barrier"
- [ ] 4.1 修改 `GPUPhysicsSystem` 添加双缓冲 SSBO 管理
- [ ] 4.2 实现 `SwapBuffers()` 帧末缓冲交换
- [ ] 4.3 重构 `physics.compute` 使用 Read/Write 缓冲分离
- [ ] 4.4 添加 `mapBoundary` Uniform

---

### Phase 6: 符文之语精确校验 (Runeword Tags)
- [ ] 6.3 重构 `checkForRuneword()` 添加子类型匹配
- [ ] 6.6 更新 `ItemFactory` 设置 `weaponSubtype`

---

## 工时汇总
| Phase | 预估工时 |
|-------|---------|
| Phase 1-6 | **~30h (4-5 工作日)** |

---