# Implementation Plan: Render Layer Batching Optimization

## Phase 1: Infrastructure & Shader Development (The Foundation)
**Goal**: 建立 GPU 数据结构基础，编写并验证 SDF 渲染 Shader。
**Est. Time**: 2h

- [ ] **Task 1.1**: Update `GPUData.hpp`
    - 定义 `GPULabelInstance` 结构体 (64字节对齐)。
    - 添加 `static_assert` 验证。
- [ ] **Task 1.2**: Create Shaders
    - `assets/shaders/ui/label_instanced.vs`: 处理实例化顶点变换。
    - `assets/shaders/ui/label_instanced.fs`: 实现 SDF 圆角矩形渲染算法。
- [ ] **Task 1.3**: Extend `RenderSystem` (Data Only)
    - 在 `RenderSystem` 类中添加 `ComputeBuffer m_labelInstanceBuffer`。
    - 添加 `Shader m_labelShader` 及相关 Uniform Locations 缓存。

## Phase 2: Pipeline Refactoring (The Separation)
**Goal**: 拆分渲染循环，实现“收集-上传-绘制”流水线。
**Est. Time**: 3h

- [ ] **Task 2.1**: Implement Collection Logic
    - 重构 `RenderSystem::Render`。
    - 创建 `struct TextRenderCmd` 用于缓存文本绘制指令。
    - 编写 Pass 1: 遍历实体，进行剔除，同时填充 `labelBuffer` (GPU数据) 和 `textQueue` (CPU数据)。
- [ ] **Task 2.2**: Implement Batch Rendering
    - Pass 2 (Background): 上传 `labelBuffer`，调用 `DrawArraysInstanced`。
    - Pass 3 (Text): 遍历 `textQueue`，调用 `DrawTextEx`。
- [ ] **Task 2.3**: Cleanup & Integration
    - 移除旧的 `DrawRectangle` / `DrawRectangleLinesEx` 立即模式代码。
    - 确保 `LabelCacheComponent` 逻辑依然有效（用于文本尺寸计算）。

## Phase 3: Verification & Polish (The Proof)
**Goal**: 验证性能收益，微调视觉效果。
**Est. Time**: 1h

- [ ] **Task 3.1**: Performance Test
    - 在高掉落场景下对比优化前后的 FrameTime。
    - 确认 Draw Call 数量。
- [ ] **Task 3.2**: Visual Tuning
    - 调整 Shader 中的 `borderWidth` 和 `cornerRadius` 参数，使其与原版 UI 风格一致。
    - 确保颜色混合（Alpha Blending）正确。

---

## Risk Assessment
- **Shader Compatibility**: 需确保 SDF 算法在不同分辨率下边缘清晰，避免 `fwidth` 相关的潜在伪影。
- **Z-Fighting**: 由于所有背景在一个 Draw Call，所有文本在另一个，需确保文本层 Z 值始终高于背景层（或依靠绘制顺序）。目前采用 Painter's Algorithm（后画覆盖先画），顺序正确即可。
