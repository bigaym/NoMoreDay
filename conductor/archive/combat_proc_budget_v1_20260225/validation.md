# Validation — combat_proc_budget_v1_20260225

## Verification Evidence

- Budget manager data model & config loading → PASS
  - 新增 `ProcBudgetConfig` / `ProcBudgetRuntime` / `ProcBudgetManager`，支持五维预算（击回、回蓝、异常触发、触发技能、事件派发）。
  - 新增配置文件 `assets/data/proc_budget_config.json`，运行时从 JSON 加载，缺失时回退默认值。
  - 关键文件：`src/game/systems/combat/ProcBudgetManager.hpp`、`src/game/systems/combat/ProcBudgetManager.cpp`、`assets/data/proc_budget_config.json`

- Budget enforcement & deterministic downsampling → PASS
  - `RequestProc` 采用确定性 token-bucket（按秒补充、按请求扣减），`RequestEventEmit` 采用每帧计数上限。
  - 超预算时记录帧级降采样率日志：`[ProcBudget] frame=... dim=... allowed=... denied=... drop_rate=...`
  - 关键文件：`src/game/systems/combat/ProcBudgetManager.cpp`、`src/game/systems/combat/CombatEventDispatcher.cpp`

- Integration wiring → PASS
  - `SkillSystem` 触发分发接入 `trigger_proc_per_sec` 预算。
  - `DamagePipeline` 命中收益路径（life/mana on hit）通过 `OnSkillHit` 统一接入预算。
  - `AilmentEngine` 施加路径接入 `ailment_proc_per_sec` 预算。
  - `CombatEventDispatcher::Dispatch` 接入 `event_emit_per_frame` 预算。
  - 关键文件：`src/game/systems/skill/SkillSystem.cpp`、`src/game/systems/combat/CombatEventDispatcher.cpp`、`src/game/systems/combat/AilmentEngine.cpp`、`src/game/states/GameplayState.cpp`

- Test coverage → PASS
  - 新增单测：`tests/unit/ProcBudgetManagerTests.cpp`（阈值、确定性降采样、每帧重置）。
  - 新增集测：`tests/integration/ProcBudgetIntegrationTests.cpp`（高频多召唤命中收益封顶、事件派发帧预算封顶）。
  - `tests/TestCommon.hpp` 增加预算管理器测试隔离重置，避免跨测试污染。

- Build/Test gate → PASS
  - 2026-02-25: `build.bat` PASS
  - 2026-02-25: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS
