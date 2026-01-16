# OpenGL 4.3 技术能力与限制速查

## 核心特性 (Core Features)

### 1. Compute Shaders
*   **用途**: 通用计算，非常适合粒子系统模拟、流场计算、剔除（Culling）。
*   **限制**: 
    *   Workgroup 大小限制（通常 1024）。
    *   Shared Memory 大小限制（通常 32KB - 48KB）。
*   **最佳实践**: 
    *   利用 `glDispatchCompute` 处理大规模并行任务。
    *   使用 `imageLoad`/`imageStore` 或 SSBO 进行数据读写。

### 2. Shader Storage Buffer Objects (SSBO)
*   **用途**: 存储大量结构化数据（如粒子状态、变换矩阵、光照数据）。
*   **优势**: 比 Uniform Blocks (UBO) 容量大得多（通常 128MB+ vs 64KB）。
*   **注意**: 读写需要注意内存屏障 (`glMemoryBarrier`) 以避免竞态条件。

### 3. Indirect Rendering (`glMultiDrawArraysIndirect`)
*   **用途**: GPU 驱动的渲染命令生成。配合 Compute Shader 做剔除，可以极大减少 CPU 开销。
*   **场景**: 草地、成群怪物、粒子渲染。

### 4. Texture Views & Arrays
*   **用途**: `GL_TEXTURE_2D_ARRAY` 非常适合粒子动画序列帧或地形纹理层，避免频繁切换纹理绑定。

## 常见特效实现方案

### 粒子系统 (Particle System)
*   **方案**: 双缓冲 SSBO (Current/Next State)。
*   **更新**: Compute Shader 计算位置、速度、生命周期。
*   **渲染**: `glDrawArraysInstanced` 或 `glDrawArraysIndirect` (如果粒子数量动态变化)。

### 技能特效 (Skill VFX)
*   **拖尾 (Trails)**: 使用环形缓冲区 (Ring Buffer) 存储历史位置，构建 Triangle Strip Mesh。
*   **刀光 (Slashes)**: 使用带有流动 UV 的扭曲 Mesh，配合溶解 Shader。
*   **AOE 指示器**: 投射贴图 (Projective Texture) 或 SDF (Signed Distance Field) 渲染圆环/扇形。

### 界面 (UI)
*   **SDF 字体**: 使用有向距离场渲染清晰的文字（放大不失真）。
*   **Batching**: 尽可能合并 Draw Calls。

## 性能红线 (Red Flags)
*   **避免**: 在渲染循环中频繁读回数据到 CPU (`glReadPixels`, mapping buffers)。
*   **避免**: 这里的 `glBegin`/`glEnd` (已废弃，严禁使用)。
*   **注意**: 透明物体的 Overdraw（过度绘制）。大量叠加的半透明粒子会导致帧率骤降。
