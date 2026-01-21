# NoMoreDay C++20 & Architecture Standards

## 1. 命名规范
- **类/结构体**: `PascalCase` (如 `RenderSystem`)。
- **函数**: `PascalCase` (遵循项目现有风格)。
- **变量**: `camelCase` (如 `entityCount`)。
- **成员变量**: `m_` 前缀 (如 `m_registry`)。
- **常量/宏**: `k` 前缀或 `UPPER_SNAKE` (如 `kMaxEntities`)。

## 2. C++20 特性使用
- **Concepts**: 优先使用 Concept 约束模板参数。
- **Ranges**: 在不影响性能的前提下，使用 `std::views` 简化数据管道。
- **Immediate Functions**: 复杂的预计算应标注为 `consteval`。

## 3. EnTT 特定模式
- **Registry 传递**: 全局唯一，通过 `SharedContext` 或 `System` 构造函数传递引用。
- **组件访问**: 优先使用 `view<T...>()`，尽量避免直接使用 `get<T>(entity)` 以提升批处理效率。

## 4. 异常处理
- **项目原则**: 核心路径禁用 `try-catch`。使用断言 (`assert` 或自定义 `NMD_ASSERT`) 捕获编程错误。
- **错误码**: 逻辑失败应返回 `std::optional` 或 `Result<T>`。
