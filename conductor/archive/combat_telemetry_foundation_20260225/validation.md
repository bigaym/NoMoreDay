# Validation — combat_telemetry_foundation_20260225

## Scope

- 新增 `CombatTelemetry` 子系统（单例、滑动窗口、输出格式化、编译/运行时开关）。
- 覆盖 5 类指标：`DamagePipeline` / `StatsSystem` / `CombatEventDispatcher` / Trigger Guard / Summon。
- 接入帧驱动入口：`GameplayState::OnUpdate` 调用 `CombatTelemetry::BeginFrame(dt)`。
- 新增性能基准：`[Performance] CombatTelemetry - Instrumentation Overhead`。

## Verification Evidence

- Metrics infrastructure → PASS
  - 新增 `src/game/systems/combat/CombatTelemetry.hpp/.cpp`。
  - 提供 `avg/p95/p99` 滑动窗口统计与日志输出摘要。
  - CMake 新增 `COMBAT_TELEMETRY_ENABLED` 编译开关（默认 ON）。
  - 运行时开关支持 `SetRuntimeEnabled/SetOutputEnabled` 及环境变量读取。

- Instrumentation points → PASS
  - `DamagePipeline::Calculate` / `CalculateBatch`：计时采集。
  - `StatsSystem::GetStatWithTags`：调用数、缓存命中、读写锁等待。
  - `CombatEventDispatcher::Dispatch`：每帧总量 + 类型分布。
  - `SkillSystem` Trigger Guard：尝试数、拦截数、深度分布、每秒速率。
  - `Summon`：活跃实体计数、目标切换频率、事件量、预算命中率。

- Overhead benchmark (<0.1ms) → PASS
  - CTest `performance` 详细日志：
    - `[Benchmark] CombatTelemetry Overhead: baseline=0.0003ms, telemetry=0.0005ms, overhead=0.0002ms`
  - 结果：`0.0002ms < 0.1ms`。

- Build/Test gate → PASS
  - `build.bat` PASS
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS
  - `ctest --test-dir build -C RelWithDebInfo -L performance --output-on-failure` PASS
  - 提交前回归复验：`build.bat` PASS；`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

## Notes

- 实施阶段出现两次编译失败并已闭环修复：
  - `SummonSystem` 使用 `view.size_hint()` 与当前 EnTT 版本不兼容，改为线性计数。
  - 新增性能测试文件与 Unity Build 发生匿名命名冲突，已在 `tests/CMakeLists.txt` 将其加入 `SKIP_UNITY_BUILD_INCLUSION`。
