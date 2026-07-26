# NoMoreDay 实施流程

## 适用范围

在修改 C++、构建配置、资源、脚本或任务状态前加载本文件。本文件同时也是代码规则的承载入口：实施时阅读本文件即可获得必要的编码约束；完整与版本化细则见 `conductor/code_standard.md`（V2.1）与 `conductor/code_styleguides/`，作为深度参考。

## 开始前

1. 读取当前 Track 的 `*-design.md`/`spec.md`、`*-plan.md`/`plan.md` 和关联验证记录；没有 Track 时确认任务边界与验收方式。
2. 查阅相关记忆，使用代码图谱确认调用链、数据流和影响范围。
3. 从 `*-plan.md` 的原子任务中选择一个并标记为 `[~]`；不要同时推进互相依赖的多个任务。
4. 先确定最小改动和对应验证命令，再开始编辑。

## 实施循环

1. 只实现当前原子任务需要的变更，遵守下述代码规则、资源所有权和性能约束。
2. 若实现推翻了已批准的合同或范围，暂停并先更新设计与计划。
3. 每完成一个可验证单元，运行测试与验证流程中最窄的足够检查。
4. 验证通过后更新任务状态、验证记录和必要的导航索引；保留失败、例外和剩余风险。
5. 完成后向用户交付可审计的变更、验证证据和边界。除非用户明确要求，不执行 git commit。

## 实施边界

- 不修改无关用户变更，不以重构名义扩大范围。
- 主循环和渲染路径必须满足既有分配、线程、RenderGraph 与回退约束。
- 代码、数据或构建变更必须进入测试与验证流程；纯文档变更至少检查链接、索引和术语一致性。

## 代码规则（按需合并自 `code_standard.md` 与 `code_styleguides/`）

实施 C++ 或构建代码时遵守以下要点；完整/版本化细则以 `conductor/code_standard.md` 为准，Python 工具以 `conductor/code_styleguides/python.md` 为准。

### 通用与安全

- 性能优先：面向数据设计（DOD）、缓存友好；不在主循环/渲染热路径堆分配或字符串比较。
- 零 UB：禁止 Use-After-Free 与内存泄漏；资源全部 RAII，禁用裸 `new`/`delete`，用 `std::unique_ptr`/`std::shared_ptr` 或 EnTT 内部管理。
- 显式意图：单参构造加 `explicit`；忽略返回值会出错的函数加 `[[nodiscard]]`。
- 默认 `const`：不修改状态的变量与方法一律 `const`。

### 命名

- 文件 `PascalCase.cpp/.hpp`，目录 `snake_case`。
- 类型/类/结构体 `PascalCase`；函数/方法 `PascalCase`。
- 局部变量与参数 `camelCase`；成员 `m_camelCase`；常量 `UPPER_SNAKE_CASE` 或 `kPascalCase`。
- `enum class` 类型名与值均 `PascalCase`。

### 布局

- 一律以 `.clang-format` 格式化；头文件用 `#pragma once`。
- 头文件尽量减少 `#include`，优先前置声明以降低编译时间。
- 用 C++20 concepts 约束模板而非 SFINAE；数据处理优先 `std::ranges`；能用 `constexpr`/`consteval` 的移到编译期。

### ECS 与 DOD

- Component 必须 POD/标准布局，无虚函数与非平凡析构；逻辑放 System，不放 Component。
- 禁止在热路径/核心逻辑用字符串比较做分支；JSON 字符串在加载时经 registry 映射为 `enum class`/整型 ID；只读串用 `std::string_view`。
- EnTT 安全：`registry.try_get<T>(e)` 得到的组件指针在组件池变动后失效；不要跨"可能增删组件/创建销毁实体"的操作持有组件指针，小 POD 先拷到栈。
- 避免全局可变状态，全局状态放 `GameContext` 或 EnTT singleton。

### 转型与并发

- 禁用 `dynamic_cast`（生产代码）；数值/指针转换用 `static_cast`；底层内存 `reinterpret_cast` 仅限序列化等并注释，位级类型双关优先 `std::bit_cast`。
- 并行用 taskflow 图，禁用裸 `std::thread`；对同一组件类型的写入系统不得并发执行。

### 测试与提交

- 新行为/修复/合同变更在 `tests/` 对应层级留回归防护；`REQUIRE` 用于继续执行所必需的前置条件，`CHECK` 用于值断言。
- 不在生产代码加入只为测试服务的硬编码分支、facade mock 或 dummy data；测试替身只存在于测试代码。
- Git 使用 Conventional Commits（`feat:`/`fix:`/`refactor:` 等）；构建一律 `./build.bat`。

### Python 工具（`scripts/`）

- 行宽 80；4 空格缩进；公开 API 加类型注解；docstring 用三双引号，含 `Args:`/`Returns:`/`Raises:`。
- 函数/方法/变量 `snake_case`，类 `PascalCase`，常量 `ALL_CAPS_WITH_UNDERSCORES`。
- 禁用可变默认参数；禁用裸 `except:`；可执行脚本含 `main()` 并由 `if __name__ == '__main__':` 调用。
- 改动以 pylint 自检；编辑时优先贴合既有风格。