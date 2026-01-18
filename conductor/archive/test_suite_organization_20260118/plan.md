# Plan: 测试套件重构与分类 (Test Suite Reorganization)

## 阶段 1: 现状审计与分类 (Audit)
- [ ] 审计 `tests/` 下所有文件，确定其归属类别。
- [ ] 检查 `TestCommon.hpp` 是否包含硬编码路径。

## 阶段 2: 物理结构调整与合并 (Execution & Consolidation)
- [ ] 创建子目录结构。
- [ ] **合并功能测试**:
  - 将 `TestBladeBoomerang.cpp`, `TestRendingWave.cpp`, `TestBladeWard.cpp`, `TestBladeFormation.cpp` 等合并为 `functional/SkillBehaviors.hpp`。
  - 将 `TestDefenseMechanics.cpp`, `TestHeirloomSystem.cpp` 等合并为 `unit/SystemMechanics.hpp`。
  - 将 `TestPolishSystems.cpp`, `TestShadowSystem.cpp` 等合并为 `integration/GameplaySystems.hpp`。
- [ ] **移动并清理**: 将剩余独立性较强的测试文件移动到对应子目录，并根据需要将 `.cpp` 修改为 `.hpp` 以适配单源编译。


## 阶段 3: 代码与构建适配 (Refactoring)
- [ ] 更新 `tests/main.cpp`: 修改所有 `#include` 路径以匹配新位置。
- [ ] 检查并更新测试文件内部的 `#include` (如引用 `TestCommon.hpp` 的路径)。
- [ ] (可选) 检查 `CMakeLists.txt` 或构建脚本，确保新目录被包含在包含路径中。

## 阶段 4: 验证与清理 (Verification)
- [ ] 运行 `build.bat` 执行完整构建。
- [ ] 运行 `build/bin/tests/NoMoreDayTests.exe` 确保所有测试用例正常通过。
- [ ] 删除 `tests/` 根目录下冗余的临时文件。
