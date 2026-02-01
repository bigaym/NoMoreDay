---
description: 3种高性能银河星系背景渲染技术规格与实施计划
---

# Track: Galaxy Rendering High-Performance Prototypes

## 1. 技术规格书 (Technical Specifications)

本Track旨在为《NoMoreDay》的天赋星盘系统提供一个极具视觉冲击力且高性能背景。我们将实现三种基于不同原理的算法，并通过统一接口进行A/B/C对比。

### 公共参数 (Uniforms)
所有Shader必须支持以下Uniform接口以保证切换时的连续性：
- `float uTime`: 全局时间 (秒)
- `vec2 uResolution`: 视口分辨率 (像素)
- `vec2 uOffset`: 视口偏移 (逻辑单位，对应相机 Target)
- `float uZoom`: 视口缩放 (1.0 = 标准)

### 方案 A: 密度波程序化生成 (Procedural Density Wave)
*   **核心原理**: 基于 Lin-Shu 密度波理论，完全在 Fragment Shader 中通过数学模型计算像素颜色。
*   **文件**: `galaxy_procedural.fs`
*   **算法细节**:
    *   **坐标系**: 笛卡尔 -> 极坐标 $(r, \theta)$。
    *   **旋臂模型**: 对数螺旋方程 $ \theta(r) = \theta_0 + \frac{1}{\tan(\alpha)} \ln(r) $。
    *   **密度函数**: 使用多层高斯分布或余弦函数模拟旋臂横截面的恒星密度。
    *   **动态性**: 引入角速度 $\Omega(r)$，使得不同半径处的旋转速度不同（较差自转），产生流体感。
    *   **颜色映射**: 核心(Core) -> 暖色/高亮; 旋臂(Arms) -> 冷色/星尘; 介质(Medium) -> 暗色吸光带。
*   **优点**: 无限分辨率，无内存占用 (VRAM)，极高的数学美感。
*   **缺点**: 片元计算量大，若实现过细可能影响低端显卡填充率。

### 方案 B: GPU 实例化点云 (GPU Instanced Point Cloud)
*   **核心原理**: 模拟真实星系的离散结构，使用大量并行的 Billboard 粒子。
*   **文件**: 
    *   Vs: `galaxy_pointcloud.vs` (需新建，负责计算轨道位置)
    *   Fs: `galaxy_pointcloud.fs` (负责点渲染)
*   **算法细节**:
    *   **数据**: 不需要预先上传 buffer，直接利用 `gl_InstanceID` 在 Vertex Shader 中生成伪随机种子。
    *   **轨道计算**: 
        *   $r = \text{Random}(ID)$
        *   $\theta_{base} = \text{Random}(ID) * 2\pi$
        *   $\omega = \frac{G \cdot M}{r^{1.5}}$ (开普勒第三定律近似)
        *   $\theta_{final} = \theta_{base} + \omega \cdot uTime$
        *   此处叠加螺旋扰动以形成形状。
    *   **渲染**: 绘制 50,000+ 个点 (GL_POINTS 或 小 Quad)。
*   **优点**: 真实的粒子感，深度感强，顶点级负载一般远小于全屏片元负载。
*   **缺点**: 可能会有锯齿或摩尔纹，需处理好抗锯齿。

### 方案 C: 多层视差混合 (Layered Parallax)
*   **核心原理**: 传统的2.5D技术，通过多层不同速度旋转的纹理叠加产生深度错觉。
*   **文件**: `galaxy_parallax.fs`
*   **算法细节**:
    *   **图层 1 (Background)**: 极慢旋转的深空噪声或远处星场。
    *   **图层 2 (Nebula)**: 中速旋转，使用 FBM 噪声生成的扭曲星云。
    *   **图层 3 (Structures)**: 较快旋转，清晰的旋臂结构（可以是程序化生成的纹理采样）。
    *   **视差处理**: 每一层根据 `uOffset` 施加不同比例的位移，模拟远近视差。
*   **优点**: 艺术控制力最强，性能最稳定（只取决于纹理采样次数）。
*   **缺点**: 动态感不如物理模拟，且受限于纹理分辨率。

## 2. 实施计划 (Implementation Plan)

### Phase 1: 基础设施建设 (Infrastructure)
1.  **资源注册**: 在 `AssetRegistry` 中注册新的 Shader 资源。
2.  **渲染器扩展**: 修改 `AstrolabeRenderer`，增加 `m_renderMode` 状态和切换逻辑。
3.  **UI集成**: 在 `UIAstrolabe` 的 `DrawInternal` 中添加 Debug 按键 (F5/F6/F7)。

### Phase 2: 核心算法开发 (Core Development)
1.  **开发方案 A**: 编写 `galaxy_procedural.fs`。重点在于调整对数螺旋参数以匹配美术风格（紫色/金色基调）。
2.  **开发方案 B**: 编写 `galaxy_pointcloud.vs` 和 `.fs`。实现基于 ID 的伪随机轨道生成。
3.  **开发方案 C**: 编写 `galaxy_parallax.fs`。实现多层旋转混合。

### Phase 3: 调试与优化 (Tuning)
1.  **视觉对齐**: 确保三个方案在 `ResetView` (缩放 1.0) 下看起来大小一致，颜色风格统一。
2.  **性能分析**: 使用引擎内置的 FPS 计数器对比三种方案的开销。
3.  **最终交付**: 选定默认方案，但保留代码以便未来切换。

## 3. 验收标准
- [ ] 按下 `N` 键打开星盘时，背景显示新的银河效果。
- [ ] 银河随时间缓慢旋转。
- [ ] 拖拽/缩放视图时，背景有正确的互动（不应仅仅是静态图片）。
- [ ] FPS 保持在 60 以上 (参考值)。
