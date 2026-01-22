# NoMoreDay 项目开发工作流 (V3.3)

## 0. 核心原则 (The Prime Directives)
1. **计划为准 (Plan-Driven)**: 所有的开发任务必须在 `plan.md` 中追踪，并通过 `tracks.md` 进行索引。
2. **架构一致性 (Architectural Integrity)**: 方案必须符合 C++20、ECS (EnTT)、OpenGL 4.3+ 及数据导向设计 (DOD)。
3. **主循环零分配 (Zero-Allocation Loop)**: 代码更新阶段严禁动态内存分配。
4. **测试即质量 (Quality by Testing)**: 关键逻辑必须包含复现式测试。
5. **审计优先 (Audit First)**: Agent 负责实现和重构，用户负责最终审计。**严禁在无用户指令的情况下进行 git commit**。
6. **环境感知**: 适配 MSVC (Release) 与 G++ (Release) 的双编译器开发，注意二进制路径差异。

---

## 1. 任务标准工作流 (Standard Task Workflow)

所有原子任务遵循以下生命周期：

### Step 1: 任务选择与准备
*   **选择任务**: 从 `plan.md` 中按顺序选择下一个任务。
*   **标记状态**: 将任务标记从 `[ ]` 改为 `[~]`（进行中）。
*   **激活技能**: 根据任务类型激活对应的技能（`feature-architect`, `systematic-debugging` 等）。

### Step 2: 逻辑实现与构建
*   **原子开发**: 实现业务逻辑，确保遵循 `Common.hpp` 和 `GPUData.hpp` 规范。
*   **暂存更改**: 允许使用 `git add` 暂存代码。
*   **编译验证**: 执行 `.\build.bat Release`。
*   **上下文保护 (Context Saving)**: 在任务初期或进行大规模重构时，编译输出可能会产生海量报错。**严禁将完整错误流直接输出到终端控制台**（会消耗大量上下文）。必须将输出重定向至文本文件（如 `build_log.txt`），然后通过 `view_file` 读取前 100 行或关键错误段落进行修复。

### Step 3: 测试验证
*   **编写测试**: 在 `tests/` 下对应的子目录编写测试代码。
*   **执行测试**: 注意编译器造成的路径差异。

### Step 4: 任务交付 (Audit & Feedback)
1. **提交审计**: 开发完成后，通知用户“任务已就绪，等待审计”。
2. **审计修正**: 根据用户的审计报告或反馈建议完善代码。
3. **严禁自动提交**: 只有在用户明确下达指令后方可 `commit`。

---

## 2. 阶段性存盘协议 (Checkpointing Protocol)

**触发点**: 当一个 Phase 结束，且用户通过了所有的审计和修正。

### Step 1: 自动化验证
*   执行全量自动化测试，并向用户报告结果。

### Step 2: 用户手动验收
*   提供步骤化的手动验证方案，**暂停**并等待确认。

### Step 3: 创建存盘点 (仅限用户授权)
*   由用户执行或在用户授权下执行 Checkpoint 提交。

---

## 3. 质量关卡 (Quality Gates)

在任务就绪（Audit Ready）前，必须满足：
- [ ] 代码通过 Release 配置编译，无新增 Warning。
- [ ] 自动化测试全部通过。
- [ ] 无魔法数字，常量已归档。
- [ ] 变更已暂存，编译日志已按需处理。

---

## 4. 常见开发指令与路径

### 路径差异说明
- **MSVC 默认路径**: `.\build\bin\Release\NoMoreDayTests.exe`
- **G++ 默认路径**: `.\build\bin\NoMoreDayTests.exe`

### 操作指令
```powershell
# 标准构建 (Release)
.\build.bat Release

# 上下文保护构建 (重定向输出)
.\build.bat Release > build_log.txt 2>&1

# 读取重定向日志的前 100 行 (PowerShell)
Get-Content build_log.txt -TotalCount 100

# 执行测试 (自动适配环境)
$testPath = if (Test-Path ".\build\bin\Release\NoMoreDayTests.exe") { ".\build\bin\Release\NoMoreDayTests.exe" } else { ".\build\bin\NoMoreDayTests.exe" }
& $testPath "[combat]"

# 暂存所有变更
git add .
```

## 5. 持续改进
- 优化海量报错时的关键词过滤策略。
- 减少重复编译次数，合并逻辑后再执行统一验证。
