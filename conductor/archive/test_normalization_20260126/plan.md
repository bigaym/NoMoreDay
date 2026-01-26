# 实现计划 (Plan) - 测试用例命名规范化

## 1. 准备阶段
- [ ] 备份 `tests/` 目录。
- [ ] 运行一次完整测试，记录基准结果（确认当前通过数为 107）。

## 2. 任务分解

### Task 1: 规范化 Unit 单元测试 (预计 1h)
- 遍历 `tests/unit/` 目录。
- 修改所有 `TEST_CASE` 名，前缀统一加 `[Unit]`。
- 涉及文件：`BuffTests.hpp`, `CombatFormulaTest.hpp`, `AttributePipelineTest.cpp` 等。

### Task 2: 规范化 Integration & Functional 测试 (预计 1h)
- 遍历 `tests/integration/` 和 `tests/functional/`。
- 修改 `TEST_CASE` 名，前缀分别加 `[Integration]` 或 `[Functional]`。
- 检查 `tests/main.cpp` 中的内联测试（如有）。

### Task 3: 规范化 Performance & Tech 测试 (预计 0.5h)
- 遍历 `tests/performance/` 和 `tests/tech/`。
- 修改 `TEST_CASE` 名，前缀分别加 `[Performance]` 或 `[Tech]`。
- 移除旧的 `Scenario A:` 等前缀，统一结构。

### Task 4: 处理特殊分类 Bugfix (预计 0.5h)
- 识别并处理 `CollisionReproTest.hpp` 等复现向测试，标记为 `[Bugfix]`。

## 3. 验证阶段
- [ ] 执行 `.\build.bat`。
- [ ] 运行 `./build/bin/NoMoreDayTests.exe`，确认通过数依然为 107。
- [ ] 验证过滤功能：`./build/bin/NoMoreDayTests.exe -tc="[Unit]*"` 是否仅运行单元测试。

## 4. 状态记录
- 当前进度：0%
- 风险点：`TEST_CASE` 跨多行可能导致正则表达式匹配不全。
