# 渲染性能优化方案：海量掉落物标签 (Instanced Item Labels)

## 1. 问题诊断
当前 `RenderSystem` 在渲染掉落物时，每一帧都在 CPU 端执行以下操作（假设 N=100+ 物品）：
1.  **立即模式绘图 (Immediate Mode)**: 调用 `DrawRectangleRec` (背景) 和 `DrawRectangleLinesEx` (边框)。
2.  **顶点生成**: Raylib 在 CPU 端为每个矩形生成顶点数据，并可能导致多次 GL 状态切换或 Buffer 更新。
3.  **Overhead**: 当物品数量较多时，成百上千次的小型 Draw Call 和顶点数据传输成为 CPU 瓶颈。

## 2. 优化目标
将 N 个物品的背景与边框渲染合并为 **1 次 Draw Call**。

## 3. 技术方案：GPU Instancing + SDF 渲染

### 3.1 核心思想
不再提交几何体顶点，而是提交**实例数据**。使用一个通用的 `Quad` (四边形) 作为几何体，在 Fragment Shader 中通过数学公式（SDF - 有向距离场）动态绘制圆角矩形和边框。

### 3.2 数据结构 (Aligns with std430)
在 `GPUData.hpp` 中定义实例结构（64字节对齐）：

```cpp
struct GPULabelInstance {
    Vector2 position;     // 屏幕/世界坐标 (中心或左上角)
    Vector2 size;         // 宽/高
    Vector4 bgColor;      // 背景色 (RGBA)
    Vector4 borderColor;  // 边框色 (RGBA)
    float borderWidth;    // 边框宽度
    float cornerRadius;   // 圆角半径 (可选，或硬编码)
    float padding[2];     // 补齐 64 字节
};
```

### 3.3 Shader 实现 (SDF)
- **Vertex Shader**: 根据 `gl_InstanceID` 读取实例数据，计算顶点位置。
- **Fragment Shader**:
    - 计算像素到矩形边缘的距离 (Distance)。
    - 使用 `smoothstep` 混合背景色、边框色和透明度。
    - **优势**: 无需抗锯齿 (AA) 开销，自动获得完美的抗锯齿圆角和边框，且缩放不失真。

### 3.4 渲染管线改造 (`RenderSystem.cpp`)
1.  **剔除 (Culling)**: 保留现有的 CPU 视锥剔除逻辑。
2.  **收集 (Collection)**: 遍历可见物品，将数据填入 `std::vector<GPULabelInstance>`。
3.  **上传 (Upload)**: 将 Vector 数据一次性上传至 `ComputeBuffer` (Instance Buffer)。
4.  **绘制 (Draw)**: 使用 `glDrawArraysInstanced` 绘制 Quad。
5.  **文本层**: 文本仍使用 Raylib 绘制（Raylib 对文本有批处理），但在背景层合并后，整体开销将大幅下降。

## 4. 预期收益
- **Draw Calls**: 背景与边框绘制次数从 `2 * N` 降至 `1`。
- **CPU 开销**: 消除大量 `DrawRectangle*` 带来的顶点生成开销。
- **显存带宽**: 仅传输必要的属性数据（Pos, Size, Color），而非三角形顶点。

## 5. 执行计划
1.  **定义数据**: 修改 `src/engine/render/GPUData.hpp` 添加 `GPULabelInstance`。
2.  **编写 Shader**: 创建 `assets/shaders/ui/label_instanced.vs` 和 `.fs`。
3.  **实现渲染器**: 在 `RenderSystem` 中集成实例化渲染逻辑。
4.  **验证**: 在高密度掉落场景下测试帧率稳定性。
