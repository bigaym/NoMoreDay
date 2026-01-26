# Performance Tuning Report - 2026-01-27

## 1. Executive Summary
本次调优针对游戏帧率徘徊在 80 FPS 左右、GPU 频率波动剧烈以及高 CPU 负载问题进行了系统化诊断与优化。通过分阶段解耦物理同步、优化 GPU 带宽利用率以及实施 ARPG 风格的 UI 裁剪策略，成功将核心系统（GPUEntitySystem）的 CPU 停顿时间从 **5.5ms** 降低至 **0.8ms** 以内，并初步稳定了 GPU 频率。

## 2. Problem Statement
*   **低帧率**: 物理步进与渲染步进耦合，导致每帧执行多次冗余的 GPU 数据上传。
*   **同步阻塞 (Stalls)**: 使用 `GL_MAP_COHERENT_BIT` 导致驱动程序频繁进行缓存一致性检查，造成 GPU 频率在 800M~2500M 间剧烈波动。
*   **带宽浪费**: 每帧全量同步 200,000 个实体的物理与视觉数据（约 25.6MB/frame）。
*   **CPU 瓶颈**: 物品标签与怪物 UI 缺乏 LoD (Level of Detail) 策略，大量的 `MeasureTextEx` 和分散的 Draw Call 消耗了大量 CPU 时间。

## 3. Optimization Phases

### Phase 1: 频率解耦 (Decoupling)
*   **改动**: 将 `GPUEntitySystem` 的逻辑更新 (`UpdateLogic`) 与 GPU 上传 (`UploadGPU`) 分离。
*   **结果**: 物理循环 (`fixedDt`) 现在只更新影子缓冲区，每渲染帧仅执行一次 GPU 上传。消除了在高帧率下的冗余带宽开销。

### Phase 2: 内存同步优化 (Explicit Sync)
*   **改动**: 移除 `PersistentBuffer` 的 `Coherent` 标志，切换至 `Explicit Flush` 模式。
*   **结果**: 
    *   消除了硬件层面的自动同步负担，GPU 频率趋于稳定。
    *   引入 `FlushRange`，仅刷新当前活跃实体的内存区域，大幅提升了 `memcpy` 后的硬件通知效率。

### Phase 3: 带宽与计算削减 (Load Reduction)
*   **改动**: 
    *   将 `MAX_ENTITIES` 从 200,000 压缩至 30,000。
    *   动态调整 MDI 剔除 (Cull) 的范围，仅处理当前活跃的槽位。
*   **结果**: `BatchUpload` 耗时从 5ms+ 降低至 **0.6ms** 左右。

### Phase 4: ARPG 风格 UI 策略 (The "Hidden Killer" Fix)
*   **改动**: 
    *   **怪物名称 LoD**: 普通小怪不再显示名称和复杂边框，仅在受损或被追踪时显示简易血条。只有精英及以上怪物显示全称与词缀。
    *   **物品标签配额**: 设置 `MAX_RENDER_LABELS = 64`，并优先保障稀有物品显示。
*   **结果**: `Render::ItemLabels` 的 CPU 开销从 **1.6ms** 优化至 **1.1ms** 以下，减少了大量字符串测量与批次提交。

## 4. Current Performance Baseline (2026-01-27)
*   **GPU::BatchUpload**: ~600us (Stable)
*   **Render::Entities (MDI)**: ~150us
*   **Render::ItemLabels**: ~1.1ms
*   **ParticleSystem::Update**: ~1.5ms - 2.0ms (Current Major Bottleneck)
*   **CPU Frequency**: 5490MHz (Stable under PBO)
*   **GPU Frequency**: 800M - 1000M (Low but Stable, suggesting CPU-GPU handoff is now more efficient but still task-limited)

## 5. Future Optimization Leads (Next Steps)

### 5.1. 粒子系统发射裁剪 (Emission Culling)
当前的粒子系统在每帧更新时仍有较大波动（峰值可达 4ms+）。
*   **计划**: 在 CPU 侧对发射器位置进行视口检查，屏幕外的粒子发射请求直接丢弃，不进入 `m_stagedParticles`。

### 5.2. UI 文本顶点化 (Vertex-Based Text Rendering)
*   **发现**: 即使限制了标签数量，`DrawTextEx` 的开销依然存在。
*   **计划**: 实现一个 UI Text SSBO，将所有标签文字一次性提交，由专门的字体 Shader 配合 Atlas 进行 MDI 渲染，彻底消除文字渲染的 Draw Call。

### 5.3. MDI 剔除重用 (Cull Memoization)
*   **发现**: 视锥剔除 Compute Shader 每帧都在处理 30,000 个实体。
*   **计划**: 当相机位移低于阈值时，直接重用上一帧的 `VisibleIDBuffer`，进一步减轻 GPU 计算压力。

### 5.4. 怪物地形粒子持久化与密度控制 (Terrain Particle Lifetime & Density)
*   **风险预警**: 发现怪物产生的地形粒子（如 "火焰之路"）会长时间渲染大量粒子。这可能成为一个“性能黑洞”，尤其是在多精英怪物同场或长时间战斗时，累积的粒子数量将拖慢整个渲染管线。
*   **分析**: 这类粒子通常具有长生命周期、高覆盖率的特点，且目前缺乏针对屏幕外或远距离地形效果的密度衰减机制。
*   **计划**:
    *   **距离衰减**: 对处于屏幕外一定距离的地形粒子发射器强制执行“提前关停”或“密度降级”策略。
    *   **生命周期硬上限**: 为所有怪物生成的环境粒子设置最大共存上限。
    *   **重叠合并**: 检测同一位置的重复地形效果，避免在同一坐标重复发射重叠的粒子，以减少过度绘制 (Overdraw)。

---
*Report compiled by Gemini CLI Agent.*
