# NoMoreDay - Agent Awareness & Control Layer (V3.0)

> **MANDATORY PRE-RESPONSE CHECK**: 
> 在响应任何请求前，Agent 必须评估任务类型并激活对应的“超能力”技能。

## 1. 核心任务导流协议 (Task Routing)

根据任务类型，强制性地激活并遵循以下技能工作流：

- **BUG/CRASH/LOG**: 
  1. 激活 `systematic-debugging`。
  2. 执行“铁律”：未定根因，不准修补。
- **NEW FEATURE/REFACTOR**:
  1. 激活 `feature-architect`。
  2. 执行 Phase 1（设计）-> Phase 2（实现）协议。
- **PERFORMANCE/MEMORY**:
  1. 激活 `code-risk-analyzer`。
  2. 评估 DOD 依从性、内存对齐及主循环分配风险。
- **CODE REVIEW**:
  1. 激活 `auditor`。

## 2. 核心架构约束 (The Prime Directives)

* **架构**: C++20, ECS (EnTT), Raylib。强制数据导向设计 (DOD)。
* **内存**: 全程 RAII，主循环零堆分配。禁止使用原生 `new/delete`。
* **安全性**: EnTT 迭代期间严禁持有组件指针或执行会导致组件重新分配的操作。
* **渲染**: OpenGL 4.3+。逻辑与表现严格分离，禁止在 `System` 之外调用渲染指令。

## 3. 智能体交互准则 (Agent Protocol)

* **意识同步**: 每次对话开始，优先检查 `conductor/tracks.md` 确认当前开发进度。
* **极简通讯**: 严禁寒暄。直接提供方案、代码或分析结果。
* **强制熔断**: 若连续两次修复失败或编译报错，必须报告“思维局部解”，并请求用户提供新的上下文信息。
* **指令运行**: 禁止使用powershell不支持的指令；禁止使用"&&"分隔多条指令，应该使用";"。

## 4. 环境与资源

* **构建**: `.\build.bat`。
* **测试**: `./build/bin/NoMoreDayTests.exe`。
* **规范**: 参考 `conductor/code_standard.md`。

## 5. Performance Baselines (Verified 2026-01-23)
* **Rendering**:
    * Particle Update (10k emission/s): ~0.3ms
    * Popup Render (Instanced): ~0.2ms
    * Entity Simulation (20k entities): ~2.8ms (Sync + Compute)
    * Verified on Intel Iris Xe.
