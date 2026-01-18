# Spec: 测试套件重构与分类 (Test Suite Reorganization)

## 核心概念
对 `tests/` 目录进行系统化物理重构，并通过“逻辑合并”大幅减少零散的测试文件数量。重点在于将功能相似的测试（特别是功能/行为测试）整合为大型测试套件，确保整个项目仅需构建极少数（目标为 1 个）测试程序。

## 目标
1. **深度合并**:
   - 将零散的行为测试（如各技能测试）合并为 `functional/SkillBehaviors.hpp`。
   - 将零散的机制验证（如防御、数值计算）合并为 `unit/SystemMechanics.hpp`。
   - 将零散的玩法系统测试合并为 `integration/GameplaySystems.hpp`。
2. **构建极简化**: 严格遵循多头文件、单源文件（Single TU）模式，确保 `tests/main.cpp` 为唯一构建入口，避免生成多个 .exe 导致的重复链接。
3. **分类整理**: 按照 Unit, Integration, Functional, Performance, Tech 进行物理归档。

## 目录结构规划
```text
tests/
├── main.cpp                 # 统一入口 (唯一编译单元)
├── TestCommon.hpp           # 全局测试辅助
├── unit/                    # 单元测试 (Logic/POD/Mechanics)
│   ├── SystemMechanics.hpp  # 合并后的系统机制测试
│   └── BuffTests.hpp
├── integration/             # 集成测试 (Cross-Systems)
│   ├── GameplaySystems.hpp  # 合并后的复杂系统交互测试
│   └── SkillSystemTests.hpp
├── functional/              # 功能测试 (Behaviors/Skills)
│   └── SkillBehaviors.hpp   # 合并后的所有技能行为测试
├── performance/             # 性能测试 (Benchmarks)
│   ├── StatsBenchmark.cpp   # 保持为头文件或被 main.cpp 包含
│   └── DropSystemBenchmark.cpp
└── tech/                    # 技术验证 (Engine/GPU)
    ├── GPUFlowFieldTest.hpp
    └── EngineTechTests.hpp
```

## 处理逻辑
1. **内容整合**: 将多个 `.cpp` 或 `.hpp` 中的 `TEST_CASE` 代码块提取并合并到新的分类头文件中。
2. **移动文件**: 将剩余独立文件移动到对应子目录。
3. **消除多目标**: 检查 `tests/` 下是否还有独立编译的 `.cpp`（非 `main.cpp` 包含的），将其全部转为被包含模式。
4. **更新 Main**: 按照新路径更新 `tests/main.cpp`。

## 边缘情况
- **命名冲突**: 合并测试用例时需确保 `TEST_CASE` 的描述字符串不重复。
- **构建脚本**: 确保 CMake 不再为被合并的 `.cpp` 创建独立的 Executable。