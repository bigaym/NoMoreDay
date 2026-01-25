# 调试分析报告

## 1. 根因分析 (Root Cause)
经过对日志和代码变更 (`git diff`) 的深度分析，定位到 `HoloBladeRenderSystem::Render` 函数中存在性能瓶颈。
具体原因如下：
1.  **高频内存分配**: 每一帧都会构建一个 `std::map<unsigned int, std::vector<entt::entity>>`。这会导致每一帧都进行多次动态内存分配 (malloc/free)，尤其是在 `std::vector` 需要扩容时。
2.  **数据拷贝**: 即使使用了 Instancing，当前逻辑仍在 CPU 端进行了大量的容器操作和数据搬运。
3.  **日志显示**: 帧生成时间 (FrameTime) 从预期的 ~2.5ms (400FPS) 上升到了 ~7-10ms (100-140FPS)，增加了约 5-7ms 的 CPU 开销，这与大量小对象内存分配的特征相符。

## 2. 证据 (Evidence)
1.  **代码变更**: `git diff` 显示 `HoloBladeRenderSystem.cpp` 中确实存在构建局部 `std::map` 的逻辑。虽然之前版本可能也存在，但结合本次性能回退的报告，此处是最大的疑点。
2.  **日志相关性**: 日志中 `HoloBladeRenderSystem` 初始化后 FPS 开始记录，且数值偏低。
3.  **理论分析**: 在游戏主循环 (Hot Path) 中使用 `std::map` 和 `std::vector` 的构造/析构是高性能 C++ 开发的禁忌 (Anti-pattern)。

## 3. 修复方案 (Proposed Solution)
重构 `HoloBladeRenderSystem::Render` 的批处理逻辑，移除 `std::map`，改用“扁平化排序”策略：
1.  **移除 `std::map`**: 不再使用 map 进行分组。
2.  **使用持久化 Vector**: 在 `HoloBladeInternal` 中添加 `std::vector<std::pair<unsigned int, entt::entity>> renderQueue` 作为成员变量，每帧 `clear()` 而不是重新构造，利用 `capacity` 避免内存分配。
3.  **排序**: 将所有需要渲染的实体放入 `renderQueue`，然后根据 `textureID` 进行 `std::sort`。
4.  **线性扫描**: 遍历排序后的队列，生成 DrawBatch。

此方案将内存分配复杂度从 O(N) (多次小分配) 降低为 O(0) (复用内存)，仅保留 O(N log N) 的排序开销 (对于已近乎有序的数据，std::sort 极快)。

## 4. 验证计划 (Verification Plan)
1.  应用修复代码。
2.  编译项目。
3.  请求用户运行游戏并观察 FPS 是否恢复至 400 左右。
