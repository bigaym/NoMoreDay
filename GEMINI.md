# NoMoreDay - 核心规则与上下文 (V2.0)

## 1. 项目概览 (Project Identity)
*   **类型**: 高性能 2D 类暗黑 Roguelite ARPG。
*   **架构**: C++20, ECS (EnTT), Raylib, Taskflow (异步), xsimd (向量化)。
*   **性能**: 支持 10,000+ 实体同屏，强制数据导向设计 (DOD)。

## 2. 技术栈核心 (Tech Stack)
*   **内存**: mimalloc (分配), RAII (生命周期)。严禁原生 `new`/`delete`。
*   **组件**: POD 类型，严禁包含复杂逻辑。
*   **渲染**: OpenGL 4.3+, Compute Shaders, SSBO, GPU Instancing。
*   **数据**: nlohmann/json 序列化，spdlog 日志。

## 3. 核心工程原则 (Engineering Principles)
> 详细规则请参考关联技能 (`developer`, `auditor`, `bug-fixer`, `code-risk-analyzer`)。

*   **安全性**: 对 UB、UAF、内存泄漏零容忍。在 EnTT 操作中严禁持有组件指针。
*   **性能**: 主循环禁止堆分配。最大化缓存局部性，避免热点路径中的虚函数。
*   **配置**: 逻辑常量 $\to$ `Common.hpp`；渲染常量 $\to$ `GPUData.hpp`。

## 4. 智能体交互协议 (Agent Protocol)
### 4.1 沟通准则 (Communication)
*   **极简主义**: 严禁废话、寒暄或指令复述。直接输出技术方案。
*   **精确提问**: 面对歧义指令必须挂起任务，列出候选意图供用户选择。
*   **置信度**: 对不确定推论标注 `[置信度: Low/Mid/High]`。

### 4.2 任务流程 (Workflow)
*   **设计优先 (Phase 1)**: 涉及架构变更时，先提交 Spec/Plan。
*   **授权实施 (Phase 2)**: 获准后执行代码修改。禁止“边说边做”。
*   **根因分析**: 修复 Bug 必须追溯至架构或第一性原理，拒绝逻辑补丁。
*   **强制熔断**: 连续两次失败后必须主动报告“思维局部解”并请求新线索。

## 5. 资源与环境 (Build & Structure)
*   **构建/测试**: `.\build.bat`；测试集位于 `build/bin/tests/`。
*   **脚本**: `scripts/` (Python 3.10+)。
*   **目录**: `src/app` (入口), `src/engine` (渲染/物理), `src/game` (逻辑), `src/systems` (ECS), `assets/` (资源)。
