---
name: project-navigator
description: 引导智能体进入实现规划阶段。在编写代码前使用此技能，确保与技术标准和需求保持一致。
---

# 项目导航员 (Project Navigator)

## 目标
通过在编写代码*之前*强制执行架构标准并澄清需求，防止技术债务和构建失败。

## 指令
1.  **智能探索与检查**:
    -   **上下文**: 使用 `analyze {mode:'directory', max_depth:2}` 可视化当前工作环境。
    -   **参考搜索**: 使用 `search {keyword:'<Component/System Name>'}` 查找类似组件是如何定义和使用的。
    -   **文件定位**: 使用 `find {type:'code', pattern:'*<feature>*'}` 快速定位相关的源文件。
    -   **预检**: 运行环境验证脚本：
        `python .agent/skills/project-navigator/scripts/preflight_check.py`

2.  **需求守卫**:
    -   如果任务模糊（例如“修复 Bug”），请勿继续。
    -   要求用户澄清：
        -   **组件**: 涉及哪些 POD 结构体？
        -   **系统**: 哪些 `update()` 循环需要更改？
        -   **安全**: 是否存在指针失效风险？

3.  **实现规划**:
    -   提出一个简短的计划。
    -   识别必要的 `Common.hpp` 常量（禁止使用魔法数字）。

4.  **标准强制执行**:
    -   **EnTT**: 禁止在注册表操作之间保留组件引用。
    -   **C++20**: 使用 Concepts 和 Ranges。
    -   **Taskflow**: 针对并行进行设计。

## 约束
-   必须首先执行 `preflight_check.py`。
-   没有所有权语义的原始指针是不允许的。
-   禁止使用 `new`/`delete`。