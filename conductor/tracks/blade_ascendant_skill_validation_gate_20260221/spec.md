# Blade Ascendant Skill Validation Gate - 规格说明书

> **Track ID**: `blade_ascendant_skill_validation_gate_20260221`  
> **状态**: Pending  
> **依赖**: `blade_ascendant_skill_rendering_integration_20260221`  
> **验收依据**: `设计文档/特效和UI/BladeAscendant_SkillVFX_Design_v2.md`

---

## 1. 目标

为剑修技能系统重构提供独立门禁，覆盖功能、合同、性能、稳定性、回退五类验证。

## 2. 门禁维度

- 功能门禁
  - 9 技能基础可施放
  - 关键分支行为正确
  - 御剑步/剑意交互正确
- 合同门禁
  - Trigger 上限、Transmuter 互斥、范围声明完整
- 性能门禁
  - 不引入显著帧时间回归（与当前主线基线对比）
- 稳定性门禁
  - 高频施法、切图、切 tier 无崩溃/断言
- 回退门禁
  - Low tier 与特效禁用路径可玩

## 3. 测试矩阵

- Unit
  - `SkillContractValidation*`
  - `SkillTriggerGuard*`
  - `SwordIntentRuntime*`
- Integration
  - `SkillBehaviorRegistry*`
  - `RenderGraph ownership path for skill VFX`
- Performance
  - `ctest --test-dir build -C Release -L performance --output-on-failure`

## 4. Bug 挂接规则

- 本 track 新发现问题统一挂接 `conductor/bug_registry.md`
- 若性能失败与本任务无关，标注“非本任务阻塞”，并记录证据与关联 bug id

## 5. 验收标准

- [ ] `build.bat` 通过
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` 通过
- [ ] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` 通过
- [ ] 合同检查无 error（warning 需具备豁免说明）
- [ ] 回退路径验证通过并有 validation 证据
