# 技术规格书 (Spec) - 测试用例命名规范化

## 1. 目标
统一 NoMoreDay 项目中所有测试用例 (`TEST_CASE`) 的命名格式，提升 `doctest` 过滤和结果分析的效率，确保测试报告具有良好的可读性。

## 2. 规范定义
所有 `TEST_CASE` 必须遵循以下 **三段式** 结构：
`"[类别] 模块名 - 详细描述"`

### 2.1 类别 (Category)
使用方括号包裹，用于大类过滤：
- `[Unit]`: 针对单个类或函数的单元测试。
- `[Integration]`: 多个系统协作的集成测试。
- `[Functional]`: 针对游戏逻辑、行为树、技能效果的功能性验证。
- `[Performance]`: 性能基准测试（Benchmark）。
- `[Tech]`: 引擎底层技术验证（OpenGL, SIMD 等）。
- `[Bugfix]`: 针对特定 Bug 修复的回归测试。

### 2.2 模块名 (Module)
对应代码库中的核心系统或类名（如 `GPUEntitySystem`, `BuffRegistry`, `CombatFormula`）。

### 2.3 详细描述 (Description)
使用中划线 `-` 连接，简洁描述该测试用例的具体验证点。

### 示例转换：
- **旧**: `"BuffRegistry Lookups"` -> **新**: `"[Unit] BuffRegistry - Registry Lookup Logic"`
- **旧**: `"Scenario A: Particle Stress Test"` -> **新**: `"[Performance] ParticleSystem - Scenario A Stress Test"`
- **旧**: `"[World] TilemapCollisionSystem Logic"` -> **新**: `"[Unit] TilemapCollisionSystem - Basic Logic"`

## 3. 验收标准 (AC)
1. `tests/` 目录下所有 `TEST_CASE` 均已更新为新规范。
2. 运行 `./build/bin/NoMoreDayTests.exe --list-test-cases` 时，输出结果整齐划一。
3. 能够通过分类前缀进行过滤运行，例如：`./build/bin/NoMoreDayTests.exe -tc="[Performance]*"`。
4. 项目编译通过，且所有 107 个测试用例均保持通过状态。
