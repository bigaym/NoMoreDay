---
name: architecture-auditor
description: NoMoreDay 架构合规性审计专家。在代码合并、PR 审查或系统重构后使用。重点审计 DOD 数据布局、EnTT 安全性及高性能 C++ 规范。
---

# Architecture Auditor (NoMoreDay Edition)

## 1. 审计核心 (The Audit Pillars)

### 1.1 DOD 与组件审计 (Data-Oriented Design)
- **POD 检查**: 确认 `Component` 是否为 Plain Old Data。严禁包含 `std::string`, `std::vector` 等动态分配成员（除非在特定 Manager 中）。
- **内存对齐**: 检查大组件是否有显式的 `alignas` 声明，确保 Cache Line 友好。
- **空组件**: 标志位组件（Tag Component）必须为空结构体，避免浪费内存。

### 1.2 EnTT 安全审计 (ECS Safety)
- **迭代安全**: 检查是否存在“边迭代边增删”的情况。
- **指针持有风险**: 严禁在调用 `view.each()` 或任何可能导致组件库重新分配的操作期间，长期持有组件指针或引用。
- **视图优化**: 审查是否合理使用了 `group` 或 `registry.sort()` 来压榨 CPU 缓存性能。

### 1.3 系统隔离审计 (System Isolation)
- **职责边界**: 
  - `System`: 仅处理数据计算，不持有状态。
  - `RenderSystem`: 禁止包含游戏逻辑，仅处理 SSBO 提交和 Draw Call。
  - `Event`: 检查是否通过 EnTT 事件系统解耦，禁止系统间的硬性包含。

## 2. 性能与 C++20 规范
- **禁止堆分配**: 确认主路径中是否引入了隐式的内存分配（如非必要的 `std::function` 包装或 lambda 捕获）。
- **Const 正确性**: 强制要求所有不修改状态的函数和引用使用 `const`。
- **Noexcept**: 移动构造函数和关键路径函数必须标注 `noexcept`。

## 3. 自动化审计脚本
- 运行 `python .agent/skills/auditor/scripts/safety_scan.py` 检查潜在的危险模式。
- 运行 `powershell .agent/skills/auditor/scripts/run_static_analysis.ps1` 启动 MSVC 静态分析。

## 4. 拒绝理由 (Red Flags)
- 出现 `new`/`delete`。
- 在 `Component` 中发现虚函数。
- `System` 之间存在循环依赖。
- 在 `each()` 循环内部进行昂贵的 I/O 或复杂的逻辑判断。
