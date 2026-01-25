---
name: codebase_investigator
description: 专为解决高复杂度问题设计的多智能体分析框架。通过模拟“专家委员会”模式，将任务拆解为不同领域的子任务，由虚拟专家（如架构师、渲染管线工程师、内存审计员）并行分析，最终汇总为深度解决方案。适用于：大型重构前调研、未定根因的复杂 Bug、跨系统依赖分析。
---

# Codebase Investigator (Multi-Agent Mode)

## 0. 必须执行：代码感知初始化 (MANDATORY)
在进行任何深入调查前，必须先初始化 C++ 代码感知工具，以便快速、准确地分析数据结构、函数调用和类继承关系：

1.  **初始化项目路径**:
    调用 `set_project_directory` 工具，并将路径设置为**当前项目的根目录**。
2.  **获取代码感知**:
    - 使用 `search_classes` 和 `get_class_info` 分析 C++ 类定义。
    - 使用 `search_functions` 和 `get_function_signature` 获取函数原型。
    - 使用 `find_callers` 和 `get_call_path` 追踪逻辑流。

## 1. 核心理念 (Core Philosophy)
当面对极其复杂的代码库问题、跨系统重构或深层架构隐患时，单一视角的分析往往存在盲区。本技能通过 **模拟多智能体协作 (Simulated Multi-Agent Collaboration)**，调用不同领域的“虚拟专家”对同一问题进行多维度会诊，最终由主探员汇总形成全景视角的解决方案。

## 2. 角色库 (Specialist Roster)

在启动调查时，根据任务性质从以下角色中指派 2-4 名专家：

### 🛠️ Systems Architect (技术架构师)
*   **视角**: 宏观架构、模块解耦、系统边界、Entity/Component/System (ECS) 合规性。
*   **关注点**: 依赖循环、接口设计、根据 `Common.hpp` 规范的代码组织。
*   **口头禅**: "这破坏了架构分层。" / "应该提取为独立组件。"

### 🎨 Rendering Engineer (图形工程师)
*   **视角**: OpenGL 4.3+ 管线、Shader 编程、GPU 内存管理 (SSBO/UBO)。
*   **关注点**: Draw Call 开销、Buffer 同步、Shader 复杂度、`GPUData.hpp` 规范。
*   **口头禅**: "由 Compute Shader 处理更高效。" / "这里有 GPU 同步阻塞风险。"

### 💾 DOD Specialist (数据导向专家)
*   **视角**: 内存布局、CPU 缓存友好性、SIMD 优化、EnTT 性能。
*   **关注点**: 结构体对齐 (Padding)、Cache Miss、分支预测、内存分配 (RAII)。
*   **口头禅**: "这不是 POD 类型。" / "主循环里有动态分配。"

### 🛡️ Safety Officer (安全审计官)
*   **视角**: 代码鲁棒性、边界情况 (Edge Cases)、多线程安全。
*   **关注点**: Use-After-Free、空指针解引用、死锁、竞态条件、错误处理。
*   **口头禅**: "如果 Component 被删除了怎么办？" / "线程不安全。"

### 📜 Legacy Archaeologist (代码考古学家)
*   **视角**: 历史代码一致性、现有逻辑复用。
*   **关注点**: 避免重复造轮子、理解“为什么当初这么写”、维护现有业务逻辑。

## 3. 调查工作流 (Investigation Protocol)

**执行者**: Lead Investigator (由当前 Agent 担任)

### Phase 1: 任务拆解 (Decomposition)
1.  **定义目标**: 明确要解决的核心难题。
2.  **组建团队**: 根据问题类型，指派合适的虚拟专家组合。
3.  **分配任务**: 为每位专家指定具体的分析文件或逻辑模块。

### Phase 2: 并行分析 (Parallel Analysis)
*模拟每位专家的视角，按顺序输出分析日志。*

**格式**:
> **[🕵️‍♂️ Role Name] Analysis Log**:
> *   **Focus**: [分析的具体文件/函数]
> *   **Observation**: [发现的问题或现象]
> *   **Technical Constraint**: [识别到的限制]
> *   **Proposal**: [初步建议]

### Phase 3: 综合研判 (Synthesis & Consensus)
*Lead Investigator 汇总各方观点，解决冲突，形成最终决议。*

1.  **冲突解决**: 当架构师（追求洁癖）与图形工程师（追求性能）冲突时，根据项目当前的 `Priority` (如性能优先) 进行裁决。
2.  **根因锁定**: 综合多方证据，锁定问题的根本原因。
3.  **行动路线**: 输出最终的 `Action Plan`。

## 4. 使用示例

### 启动指令
*"启用 Codebase Investigator 模式，调查 RenderSystem 的性能瓶颈。请指派 Rendering Engineer 和 DOD Specialist。" / "Use codebase_investigator"*

### 输出样例

```markdown
# 🕵️‍♂️ Codebase Investigation: RenderSystem Reform

## 1. Investigation Team
*   **Rendering Engineer**: Focus on OpenGL calls & Shader binding.
*   **DOD Specialist**: Focus on Entity iteration & Memory layout.

## 2. Specialist Findings

### [🎨 Rendering Engineer]
*   **Observed**: `RenderSystem::Update` 调用了 `glMapBuffer` 导致 CPU 等待。
*   **Risk**: GPU 强制同步，帧率剧烈波动。
*   **Advice**: 必须改为 `glMapBufferRange` 配合 `GL_MAP_UNSYNCHRONIZED_BIT` 或使用 Triple Buffering。

### [💾 DOD Specialist]
*   **Observed**: 渲染循环中直接访问了非连续的 `SpriteComponent` 内存。
*   **Risk**: Cache Miss 率极高。
*   **Advice**: 引入 `RenderBatch` 结构，预先将数据重排 (Packed) 到连续内存中。

## 3. Synthesis & Verdict (Lead Investigator)
**Root Cause**: 同步式 Buffer 更新与非连续内存访问共同导致了性能瓶颈。
**Consensus**: 优先解决 Buffer 同步问题 (Rendering Engineer 方案)，并在下一阶段优化内存布局 (DOD 方案)。

**Action Plan**:
1. [Rendering] 实现 Triple Buffering (参见 `tracks/performance/spec.md`)。
2. [Data] 重构 `SpriteComponent` 为 SoA 布局（暂缓，P2 优先级）。
```
