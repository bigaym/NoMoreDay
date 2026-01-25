# Rendering Layer Batching Optimization (Instanced UI Labels)

## 1. 核心目标 (Core Objectives)

**背景**: 当前 `RenderSystem` 采用立即模式（Immediate Mode）处理海量掉落物标签（Item Labels）。在高密度掉落场景下（如地图词缀增加掉落率），每一帧产生的成百上千次 `DrawRectangle` 和 `DrawRectangleLinesEx` 调用导致 CPU 瓶颈，造成帧率严重下降（FrameTime 从 ~2.5ms 激增至 10ms+）。

**目标**:
1.  **性能**: 在 500+ 可见掉落物的情况下，UI 渲染开销降低至 < 0.5ms。
2.  **批处理**: 实现 **Instanced SDF Rendering**，将所有背景框合并为 **1 次 Draw Call**。
3.  **分层管线**: 重构渲染流程，严格分离 **Geometry Pass** (GPU Instanced) 和 **Text Pass** (Raylib Batch)，消除状态切换开销。
4.  **视觉质量**: 使用 SDF (Signed Distance Field) 渲染完美的抗锯齿圆角矩形和边框，支持无损缩放。

---

## 2. 技术规格 (Technical Specifications)

### 2.1 数据结构 (Data Structures)

#### 2.1.1 GPU Instance Data (`GPUData.hpp`)
定义符合 std430 布局的实例结构，用于 SSBO 传输。

```cpp
/**
 * @brief Structure for GPU Item Label instances (Instanced UI).
 * STRICTLY 64 BYTES (16 * 4) for alignment.
 */
struct GPULabelInstance {
  Vector2 position;               // 8  - Screen/World coords (Top-Left or Center)
  Vector2 size;                   // 8  - Width, Height
  Vector4 bgColor;                // 16 - Background Color (RGBA)
  Vector4 borderColor;            // 16 - Border Color (RGBA)
  float borderWidth;              // 4  - Border width in pixels
  float cornerRadius;             // 4  - Radius in pixels
  float padding[2];               // 8  - Padding to 64 bytes
  
  GPULabelInstance() = default;
};

// Static assert for alignment safety
static_assert(sizeof(GPULabelInstance) == 64, "GPULabelInstance must be 64 bytes");
```

### 2.2 Shader 逻辑 (SDF Rendering)

#### Vertex Shader (`label_instanced.vs`)
- 输入: `gl_InstanceID` (索引 SSBO)。
- 逻辑: 生成 Quad 顶点，根据 `GPULabelInstance` 数据进行变换。
- 输出: `fragTexCoord`, `fragSize`, `fragBorderWidth`, `fragCornerRadius`, `fragColor` 等。

#### Fragment Shader (`label_instanced.fs`)
- 核心算法: SDF (Signed Distance Field) for Rounded Rectangle。
- 公式:
  ```glsl
  float sdRoundedBox(vec2 p, vec2 b, float r) {
      vec2 q = abs(p) - b + r;
      return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
  }
  ```
- 混合逻辑:
  - 计算距离 `d = sdRoundedBox(...)`。
  - `alpha = 1.0 - smoothstep(0.0, 1.0, d)` (抗锯齿边缘)。
  - `borderAlpha = 1.0 - smoothstep(borderWidth - 1.0, borderWidth, abs(d))`。
  - 混合背景色与边框色。

### 2.3 渲染管线重构 (`RenderSystem.cpp`)

#### 新增 Render Pass 流程
1.  **Cull & Collect (CPU)**:
    - 遍历所有 `ItemComponent` 和 `GoldComponent`。
    - 执行视锥剔除 (Frustum Culling)。
    - 将通过测试的数据分别填入：
        - `std::vector<GPULabelInstance> labelBuffer`: 用于 GPU 背景渲染。
        - `std::vector<TextRenderCmd> textQueue`: 用于后续 Raylib 文本渲染。
2.  **Upload (CPU -> GPU)**:
    - 将 `labelBuffer` 上传至 `ComputeBuffer` (Instance Buffer)。
3.  **Geometry Pass (GPU)**:
    - `BeginShaderMode(labelShader)`
    - `DrawArraysInstanced(GL_TRIANGLES, 0, 6, instanceCount)`
    - `EndShaderMode()`
4.  **Text Pass (Raylib)**:
    - 遍历 `textQueue`，调用 `DrawTextEx`。
    - 由于背景层已完成，此时 Raylib 可连续批处理所有文本，无状态切换中断。

---

## 3. 验收标准 (Acceptance Criteria)

### 3.1 功能验收
- [ ] 掉落物名称显示正确，位置对齐无误。
- [ ] 稀有度颜色（背景/边框）显示正确。
- [ ] 支持圆角矩形渲染，且边缘平滑（无锯齿）。
- [ ] 文本层清晰叠加在背景层之上。

### 3.2 性能验收
- [ ] **Draw Calls**: 无论掉落物数量（1~1000），背景层 Draw Call 固定为 **1**。
- [ ] **Frame Time**: 在 500+ 掉落物场景下，`RenderSystem` 总耗时 < 1.0ms。
- [ ] **Stability**: 显存占用稳定，无内存泄漏（SSBO 复用）。

### 3.3 兼容性
- [ ] 确保在 OpenGL 4.3 环境下正常工作（依赖 SSBO）。
- [ ] 窗口缩放时 UI 布局自适应正确。
