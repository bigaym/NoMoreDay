---
name: code-risk-analyzer
description: 深度分析 NoMoreDay 的底层技术风险。专注于 UB、UAF、线程竞争、GPU 资源泄漏及极端情况下的性能瓶颈。在涉及多线程、Shader 或复杂内存管理时必选。
---

# Low-Level Risk Analyzer (NoMoreDay)

## 0. 风险探测初始化 (MANDATORY)
在评估风险前，必须建立精确的代码语义索引：

1.  **初始化**: 调用 `set_project_directory`（**路径为当前项目根目录**）。
2.  **符号追踪**:
    - 使用 `get_function_signature` 检查多线程函数的异常安全性。
    - 使用 `search_symbols` 快速定位所有直接操作内存或 GPU 缓冲区的低级函数。

## 1. 内存风险深度扫描 (Memory Risks)
- **UAF (Use-After-Free)**: 特别针对 EnTT 实体销毁后的残留引用进行扫描。
- **内存抖动**: 监控主循环中是否存在频繁的“分配-释放”循环（即使是小内存块），这会导致 mimalloc 缓存失效。
- **指针悬挂**: 检查是否存在跨帧保存组件地址的危险行为。

## 2. 并发与同步风险 (Concurrency)
- **Taskflow 冲突**: 分析任务依赖图，寻找可能的资源竞争点。
- **原子性操作**: 检查关键计数器是否正确使用了 `std::atomic` 或内存屏障。
- **死锁预测**: 扫描嵌套锁或跨系统同步等待。

## 3. GPU 与 Shader 风险 (GPU/Rendering)
- **SSBO 越界**: 检查 C++ 结构体与 Shader `std430` 布局的字节对齐是否严格一致（16字节对齐规则）。
- **资源泄漏**: 监控纹理和缓冲区的生命周期，确保护照 (`UnloadTexture`, `UnloadShader`) 在析构函数中被调用。
- **同步原语**: 检查 Compute Shader 写入后，是否正确执行了内存屏障 (`glMemoryBarrier`)。

## 4. 极端性能风险 (Edge-Case Performance)
- **缓存行伪共享**: 在多线程修改相邻组件数据时，识别潜在的 False Sharing。
- **分支预测失败**: 识别热点循环中复杂的 if-else 嵌套，建议采用查表法或位运算优化。
- **SIMD 对齐**: 检查 `xsimd` 操作的数据地址是否满足对齐要求。

## 5. 分析流程
1. **静态扫描**: 分析代码中的敏感关键词 (`reinterpret_cast`, `volatile`, `atomic`).
2. **逻辑推理**: 模拟多线程竞争场景或极端内存分配压力。
3. **性能锚定**: 使用 `tests/performance` 下的基准测试进行压力验证。