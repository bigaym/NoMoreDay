# 🌌 星系天赋系统审计修复 - 实施计划

| 属性 | 值 |
|------|-----|
| **Track ID** | `astrolabe-polish_20260204` |
| **依赖 Spec** | `spec.md` (V1.0) |
| **优先级** | **HIGH** |
| **状态** | ✅ COMPLETED |
| **预计总工时** | 10-12h (1.5-2 工作日) |

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 预计工时 | 状态 |
|------|------|----------|----------|------|
| **Phase 1** | 测试基础修复 | 测试数据一致性修复 | 0.5h | ✅ 已完成 |
| **Phase 2** | 核心逻辑测试 | AstrolabeSystemTests.cpp | 2h | ✅ 已完成 |
| **Phase 3** | 誓约二次确认 | 对话框 + 长按机制 | 3h | ✅ 已完成 |
| **Phase 4** | 解锁反馈 | 失败原因提示 UI | 2h | ✅ 已完成 |
| **Phase 5** | GPU 特效 (可选) | talent_node.fs Shader | 2h | ➖ 跳过 |
| **Phase 6** | 清理与验证 | API 清理 + 全量测试 | 1h | ✅ 已完成 |

---

## Phase 1: 测试基础修复 (FIX-2) ✅

**目标**: 修复测试数据与 JSON 不一致问题

### Task 1.1: 更新 TalentLayoutTests.cpp 节点 ID ✅
- [x] 打开 `tests/unit/TalentLayoutTests.cpp`
- [x] 将 `node 1001` 改为 `node 1100`
- [x] 将 `node 3001` 改为 `node 1300`
- [x] 移除对 `effects[0].value == "SwordHeartComponent"` 的检查

### Task 1.2: 验证测试通过 ✅
- [x] 编译项目: `.\build.bat`
- [x] 运行测试: `.\build\bin\Release\NoMoreDayTests.exe --test-case="*TalentLayout*"`
- [x] 确认所有 TalentLayout 相关测试 PASS

---

## Phase 2: 核心逻辑测试 (FIX-3) ✅

**目标**: 为 AstrolabeSystem 添加单元测试覆盖

### Task 2.1: 创建 AstrolabeSystemTests.cpp ✅
- [x] 在 `tests/unit/` 创建 `AstrolabeSystemTests.cpp`
- [x] 复制 Spec §4.2 的测试用例代码
- [x] 添加必要的头文件引用 (修正为 Stats.hpp)

### Task 2.2: 更新 CMakeLists.txt ✅
- [x] 确认 `tests/CMakeLists.txt` 使用 GLOB 自动加载

### Task 2.3: 处理 CombatStats 依赖 ✅
- [x] 在测试中为 player entity emplace `CombatStats` 组件

### Task 2.4: 验证测试通过 ✅
- [x] 编译项目: `.\build.bat`
- [x] 运行测试: `.\build\bin\RelWithDebInfo\NoMoreDayTests.exe -ts=AstrolabeSystemTests`
- [x] 确认所有 6 个测试用例 PASS

---

## Phase 3: 誓约二次确认 (FIX-1) ✅

**目标**: 实现长按 2 秒确认 + 弹出对话框

### Task 3.1: 添加静态成员声明 ✅
- [x] 在 `UIAstrolabe.hpp` 添加状态变量与函数声明

### Task 3.2: 初始化静态成员 ✅
- [x] 在 `UIAstrolabe.cpp` 初始化静态成员

### Task 3.3: 实现 DrawVowDialog 函数 ✅
- [x] 实现对话框渲染与长按计时逻辑

### Task 3.4: 修改 ProfessionStar 点击逻辑 ✅
- [x] 修改为打开对话框而非直接调用 `takeVow`

### Task 3.5: 在 DrawInternal 末尾调用对话框渲染 ✅
- [x] 在 Tooltip 之后、DrawInternal 结束前调用

### Task 3.6: 手动验证 ✅
- [x] 验证对话框弹出、长按确认、取消及进度条衰减逻辑

---

## Phase 4: 解锁反馈 (FIX-4) ✅

**目标**: 添加解锁失败时的 UI 提示

### Task 4.1: 添加失败原因枚举 ✅
- [x] 在 `AstrolabeSystem.hpp` 添加 `UnlockFailReason`

### Task 4.2: 实现 tryUnlockNode 函数 ✅
- [x] 在 `AstrolabeSystem.cpp` 实现详细原因检查逻辑

### Task 4.3: 添加 UI 失败提示变量 ✅
- [x] 在 `UIAstrolabe` 添加消息缓存与计时器

### Task 4.4: 修改节点点击逻辑 ✅
- [x] 调用 `tryUnlockNode` 并设置失败提示

### Task 4.5: 添加失败提示渲染 ✅
- [x] 实现屏幕居中偏下的红色半透明背景提示框

### Task 4.6: 手动验证 ✅
- [x] 验证各种解锁失败情况下的文字反馈

---

## Phase 5: GPU 特效 (可选 - P3) ➖

**目标**: 创建节点 Shader (如时间允许)

### Task 5.1: 创建 talent_node.fs ➖
- [ ] 考虑到系统已达到设计验收标准，且 Phase 1-4 修复已显著提升体验，此可选任务暂时延后，保持 CPU 渲染以确保稳定性。

---

## Phase 6: 清理与验证 (FIX-6) ✅

**目标**: API 清理 + 全量回归测试

### Task 6.1: 处理冗余 applyTalentStats ✅
- [x] 在 `AstrolabeSystem.hpp` 中添加 `[[deprecated]]` 标注

### Task 6.2: 全量编译验证 ✅
- [x] 完整编译成功

### Task 6.3: 全量测试验证 ✅
- [x] 运行所有测试 (127/127) 全部 PASS
- [x] 特别修复了 `GameplaySystems.cpp` 和 `TalentDataTest.cpp` 中的回归项

### Task 6.4: 手动回归测试 ✅
- [x] 验证星盘 UI 整体交互稳定性

---

*计划状态更新: 任务圆满完成。回归测试已全绿。*
*最后更新: 2026-02-04*