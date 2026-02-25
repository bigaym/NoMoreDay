# Combat Ailment Engine V1 — Specification

> Track ID: `combat_ailment_engine_v1_20260225`  
> Series: CS-M2-01 | Priority: P1 | Milestone: M2

---

## 1. Overview

建立统一的 `AilmentEngine`，为每种异常状态（Poison/Ignite/Bleed/Chill/Shock/Freeze 等）提供合同化的行为规范，替代 EffectSystem 中散落的临时处理逻辑。

## 2. Ailment Contract Schema

每种异常必须声明：

| 参数 | 类型 | 说明 |
|---|---|---|
| `max_stacks` | uint8 | 最大叠层数 |
| `refresh_policy` | enum | Refresh / Extend / Independent |
| `overwrite_policy` | enum | Strongest / Newest / Additive |
| `immunity_and_resistance` | float | 0.0=免疫, 1.0=满伤 |
| `tick_interval` | float | DoT tick 间隔（秒） |
| `damage_pool_policy` | enum | PerStack / Consolidated |
| `base_duration` | float | 基础持续时间 |

## 3. Target Architecture

```
AilmentEngine (新建)
├── AilmentRegistry — 加载/管理异常合同定义
├── AilmentApplier — 根据合同应用异常到目标
├── AilmentTickDriver — 统一 DoT tick 调度（替代 EffectSystem §6）
└── AilmentAdapter — 旧 BuffType 到新合同的兼容映射层
```

## 4. Acceptance Criteria

- [ ] 每种异常有单一权威合同声明。
- [ ] DoT 类异常通过 AilmentEngine tick 调度。
- [ ] 旧 `BuffType::DamageOverTime` 通过 Adapter 兼容运行无崩溃。
- [ ] 叠层/刷新/覆盖逻辑至少 3 种异常有测试覆盖。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 5. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| 与旧 buff 系统冲突 | 高 | AilmentAdapter 双轨兼容，逐步下线旧字段 |
| 异常种类扩展导致组合爆炸测试 | 中 | 合同参数化测试，矩阵覆盖 |
