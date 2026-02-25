# Validation — combat_event_consistency_20260225

## Scope

- CalculateBatch 事件载荷统一使用 final_damage。

## Verification Evidence

- Event consistency tests (`tests/unit/EventConsistencyTests.cpp`) → PASS
  - Covered: single-target一致性、batch多目标 final_damage 一致性、mitigation 后值一致性
- `build.bat` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PASS

## Notes

- 首轮 `ctest` 出现 `HazardSystemTests` 崩溃，根因是新增测试注册的事件 handler 捕获局部引用且未解绑，导致后续用例触发悬空回调。
- 已在 `EventConsistencyTests.cpp` 中改为 RAII 生命周期管理（析构时 `Unregister`），随后重新执行完整验证链并全部通过。
