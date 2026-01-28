# 实现计划 (Plan): 静态变量安全性与热路径内存分配优化

## 阶段 1: 基础设施与线程安全修复 (Priority: High)

### Task 1.1: 修复 ItemFactory 线程安全风险
- **内容**: 将 `ItemFactory.cpp` 中的 `g_rng` 替换为 `thread_local` 实例，并确保在首次使用前正确初始化种子。
- **文件**: `src/game/systems/item/ItemFactory.cpp`
- **验证**: 编写单元测试，在并行 `tf::Executor` 中调用 `ItemFactory::CreateItem`。

### Task 1.2: StatsSystem 缓存并发保护
- **内容**: 
    - 为 `StatsSystem` 引入 `std::shared_mutex`。
    - 在 `GetStatWithTags` 的读写操作中添加 `std::shared_lock` 和 `std::unique_lock`。
- **文件**: `src/game/systems/combat/StatsSystem.hpp`, `src/game/systems/combat/StatsSystem.cpp`
- **验证**: 模拟多线程属性查询，使用 ThreadSanitizer 或压力测试确认无 Race Condition。

## 阶段 2: 热路径分配与性能优化 (Priority: Medium)

### Task 2.1: SkillSystem 迭代器优化
- **内容**: 
    - 修改 `SkillSystem::UpdateStates`，移除 `s_entities_scratch` 的全量分配。
    - 改为直接使用 `view.each` 迭代，或采用分块拷贝策略（如果 Hook 必须修改 Registry）。
- **文件**: `src/game/systems/skill/SkillSystem.cpp`
- **验证**: 基准测试 10,000 实体下的 `Update` 耗时。

### Task 2.2: AttributePipeline 逻辑扁平化
- **内容**: 
    - 优化 `Calculate` 函数，减少 `try_get` 调用。
    - 将 `SetTrack` 的处理从 `std::vector` 拷贝改为直接在原位处理（如果可能）。
- **文件**: `src/game/systems/stats/AttributePipeline.cpp`
- **验证**: 检查 `StatsDirty` 触发时的平均计算耗时。

## 阶段 3: 静态变量清理与单例重构 (Priority: Low)

### Task 3.1: 统一渲染系统静态资源
- **内容**: 
    - 确保 `RenderSystem` 中的所有静态 `vector` 在 `Shutdown` 时调用 `shrink_to_fit()` 或重置容量。
    - 检查 `s_labelInstanceBuffer` 的销毁时机。
- **文件**: `src/engine/render/RenderSystem.cpp`
- **验证**: 场景切换前后的内存快照对比。

### Task 3.2: 全局静态变量审计
- **内容**: 
    - 扫描项目中剩余的裸 `static` 变量，将其封装进 `Meyer's Singleton` 或移动到类成员。
- **范围**: 全局搜索 `static`（排除 `static constexpr` 和函数内 `static`）。

## 移交协议
- 方案已准备就绪，可以移交给 `feature-developer` 开始执行。