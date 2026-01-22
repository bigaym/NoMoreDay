# Implementation Plan: Combat Pipeline Fix

## Phase 1: 诊断与环境修复 (The Probe)
**预计工时**: 1h
**目标**: 修复 `CombatBalanceTest` 运行时的依赖缺失问题。

1.  **分析测试环境**: 检查 `TestCommon.hpp` 中的 `TestSetupScope`，确认是否初始化了 `SkillRegistry`。
2.  **修复测试用例**:
    *   在 `CombatBalanceTest` 中显式注册一个 ID=0 的测试技能。
    *   配置其 `weapon_damage_mult = 1.0f`。
3.  **验证**: 运行 `./build/bin/NoMoreDayTests.exe` 仅运行 `CombatBalanceTest`，确认通过。

## Phase 2: 系统增强 (The Safety Net)
**预计工时**: 1h
**目标**: 防止未来因缺少 Skill 定义导致的静默失败。

1.  **SkillRegistry 兜底**:
    *   修改 `SkillRegistry::GetSkill(id)`，当 id=0 且未注册时，返回一个静态的默认 `BasicAttack` 配置。
2.  **DamagePipeline 日志**:
    *   当 `skill_data` 为空且 `base_pool` 全 0 时，输出 `LOG_WARN`，提示可能配置错误。

## Phase 3: 全局验证 (The Audit)
**预计工时**: 0.5h

1.  **全量测试**: 运行所有单元测试，确保修改没有破坏其他依赖 `skill_data` 为空的逻辑（如果有）。
2.  **文档更新**: 更新相关 Spec。
