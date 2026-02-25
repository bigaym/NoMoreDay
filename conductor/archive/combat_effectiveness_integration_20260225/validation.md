# Validation — combat_effectiveness_integration_20260225

## Scope

- `added_damage_effectiveness` 接入 DamagePipeline 主公式。
- `trigger.effectiveness` 注入触发上下文并接入主公式。

## Verification Evidence

- Code implementation:
  - `DamagePipeline::DamageRequest` 扩展 `added_effectiveness` 与 `trigger_effectiveness` 字段（默认 `1.0f`）。
  - `DamagePipeline` 公式接入：
    - Added 侧：`CombatStats.flat_damage` 通过 `AddedEff` 缩放后注入实例流，并在 Increased/More 链路中参与统一结算。
    - Trigger 侧：`trigger_effectiveness` 在 `More` 后、Mitigation 前乘入每个伤害实例。
  - `DamagePipeline` 可从技能上下文读取 `SkillData.added_damage_effectiveness`，并与请求侧系数相乘。
  - `SkillSystem` 触发派发接入：
    - Trigger 执行体写入 `SkillExecution.trigger_effectiveness`。
    - 基于 `cast_id` 维护触发系数映射，`DamagePipeline` 通过 `source_entity`（`SkillExecution/Projectile/SwordArray/Channeling`）解析并消费。
  - 兼容调整：
    - `TriggerContract.effectiveness` 默认值修正为 `1.0f`。
    - `SkillData.added_damage_effectiveness` 默认值修正为 `1.0f`。
    - `CombatSystem::BuildLegacyAttackBasePool` 不再重复注入 `flat_damage`，避免与 Pipeline 新注入重复计算。

- Test coverage added:
  - New file: `tests/unit/EffectivenessIntegrationTests.cpp`
  - `[Unit] EffectivenessIntegration - AddedEff=1 keeps added damage`
  - `[Unit] EffectivenessIntegration - AddedEff scales added damage`
  - `[Unit] EffectivenessIntegration - TriggerEff=1 leaves direct cast unchanged`
  - `[Unit] EffectivenessIntegration - TriggerEff=0.5 halves damage`
  - `[Unit] EffectivenessIntegration - Combined AddedEff and TriggerEff multiply correctly`

- Verification commands:
  - `build.bat` → PASS
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS
