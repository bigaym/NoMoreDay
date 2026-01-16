# 特效设计文档：剑修 (Blade Ascendant)

## 1. 概述 (Overview)
*   **关联系统**: 剑修职业技能 (3.1 - 3.9)
*   **视觉风格**: **灵动水墨 (Ethereal Ink)**
    *   **关键词**: 锋利 (Sharp)、流动 (Fluid)、残留 (Lingering)、极简 (Minimalist)。
    *   **配色**: 天青白 (Pale Cyan #C3F8F5) 为主能量色，搭配水墨黑 (Ink Black #1A1A1A) 与 纯白 (White) 高光。
    *   **核心元素**: 剑影残像、流动的气韵、空间扭曲。

## 2. 剑意系统特效 (Sword Intent System)

### 2.1 剑意积攒 (Accumulation 0-9 Stacks)
*   **表现**: 随着剑意层数增加，角色周围的“气场”逐渐增强。
*   **Visual Breakdown**:
    *   **UI**: 屏幕底部或角色头顶显示 10 把微型飞剑图标（UI_Sword_Icon）。每获得一层，点亮一把。
    *   **角色光环**: 
        *   **1-3 层**: 角色脚下有淡淡的墨迹旋涡。
        *   **4-7 层**: 墨迹旋涡加速，并有微弱的青色粒子上升。
        *   **8-9 层**: 武器开始发出不稳定的闪烁光芒，粒子密度增加。

tiji### 2.2 剑压巅峰 (Max Stack - 10 Layers)
*   **表现**: 达到 10 层时，角色进入“人剑合一”的过载状态。
*   **Visual Breakdown**:
    *   **全屏特效**: 屏幕边缘出现轻微的青色水墨晕染 (Vignette) 和 空间扭曲 (Distortion)，模拟剑气压迫感。
    *   **角色状态**:
        *   **爆发**: 达成瞬间，角色周围爆发一圈向外扩散的冲击波 (`vfx_circle_shockwave.png`)。
        *   **持续**: 角色全身被高亮的青白光焰包裹 (Rim Light + Bloom)，武器拖出长长的残影轨迹 (即时不移动)。
        *   **环境**: 地面周围的草/灰尘被持续向外吹飞 (GPU Particles)。

### 2.3 神通释放 (Empowered Burst)
*   **表现**: 消耗 10 层剑意释放强化技能时的瞬间反馈。
*   **Visual Breakdown**:
    *   **聚气**: 所有光焰瞬间“坍缩”吸入武器中 (Reverse Shockwave)。
    *   **释放**: 技能特效带有 **黑白反色 (Inverted Color)** 或 **水墨爆裂 (Ink Splatter)** 的强化视觉风格，伴随微小幅度屏幕震动 (Screen Shake)。

## 3. 技能特效分解 (Skill VFX Breakdown)

### 3.1 流云刺 (Flowing Thrust)
*   **表现**: 极速突进，身后留下被切开的气流。
*   **Visual Breakdown**:
    *   **起手**: 角色武器高亮闪烁 (0.1s)。
    *   **冲刺**: 
        *   **拖尾 (Trail)**: 使用 `TrailRenderer` 生成一条平滑的蓝白色带，纹理使用 `trail_mask.png`，带有轻微的 `distortion_normal.png` 扰动。
        *   **残影**: 沿路径每隔 0.1s 生成一个半透明的角色网格快照 (Mesh Snapshot)，快速淡出。
    *   **结束**: 目标点出现十字星芒闪光 (Cross Flare)。

### 3.2 裂空斩 (Rending Wave)
*   **表现**: 挥出一道半月形剑气，带有明显的边缘锐度。
*   **Visual Breakdown**:
    *   **投射物**: 
        *   **核心**: 一个弯曲的半月模型 (Crescent Mesh)，边缘发光 (Fresnel)。
        *   **波纹**: 投射物前方附带空气扭曲效果 (Distortion)，使用屏幕后处理或抓屏纹理。
    *   **命中**: 产生类似“玻璃破碎”的粒子碎片效果。
*   **Tech**: Mesh Emitter, Distortion Shader.

### 3.3 灵剑决 (Blade Formation)
*   **表现**: 身后悬浮数把灵体飞剑，自动索敌射出。
*   **Visual Breakdown**:
    *   **待机**: 
        *   使用 **Instanced Rendering** 渲染 5-10 把半透明飞剑。
        *   飞剑上下轻微浮动 (Sine Wave)，周围有淡淡的 `energy_noise.png` 辉光。
    *   **发射**: 飞剑加速变亮，拖出细长的光带（Ribbon Trail）。
*   **Tech**: Instanced Static Mesh, Sine Wave positioning in Vertex Shader.

### 3.4 剑气护体 (Blade Ward)
*   **表现**: 快速旋转的剑影形成护盾。
*   **Visual Breakdown**:
    *   **护盾**:
        *   不使用实体模型，而是使用一个 Billboard 或 Cylinder。
        *   Shader 使用 **极坐标 (Polar Coordinates)** 旋转 `trail_noise.png`，模拟高速旋转的动态模糊感。
    *   **格挡**: 触发时，护盾对应方向亮度瞬间爆发 (Bloom)，并弹出火花粒子。

### 3.5 万剑归宗 (Infinite Blades)
*   **表现**: 漫天剑雨落下，覆盖全屏或大范围。
*   **Visual Breakdown**:
    *   **预警**: 地面出现大量随机的淡蓝色光圈 (Decals)，提示落点。
    *   **下落**:
        *   使用 **Compute Shader** 驱动的粒子系统。
        *   每一滴“雨”实际上是一把拉伸的剑模型。
    *   **落地**: 插入地面并在 0.5s 后溶解 (Dissolve Shader)。
*   **Tech**: Compute Shader Particles, Indirect Draw.

### 3.6 剑阵·诛仙 (Sword Array: Execution)
*   **表现**: 巨大的圆形法阵，边界有巨剑插入。
*   **Visual Breakdown**:
    *   **法阵**: 地面绘制复杂的符文圆环 (Rotator Shader)，缓慢旋转。
    *   **边界**: 4-8 把巨剑从天而降插入法阵边缘 (Impact Dust)。
    *   **内部**: 阵内有随机的 `slashing` 刀光闪烁。

### 3.7 心剑·无影 (Mind Blade)
*   **表现**: 引导激光束。
*   **Visual Breakdown**:
    *   **引导**: 角色手中聚集能量球，向内坍缩 (Reverse Dissolve)。
    *   **发射**: 
        *   一条极细的高亮核心线。
        *   外层包裹较宽的淡色光晕。
        *   使用 `ScrollUV` 让光束纹理高速流动。

### 3.8 御剑·回旋 (Blade Boomerang)
*   **表现**: 旋转的飞剑。
*   **Visual Breakdown**:
    *   **本体**: 飞剑模型绕自身 Y 轴高速自转。
    *   **轨迹**: 螺旋状的拖尾 (Spiral Trail)。
    *   **折返点**: 稍微停顿，产生一个扩散的冲击波圈 (Shockwave)。

### 3.9 绝影闪 (Phantom Flash)
*   **表现**: 消失并在原地留下残影。
*   **Visual Breakdown**:
    *   **消失**: 角色瞬间隐形 (Alpha = 0)。
    *   **残影**: 原地生成一个纯黑色的墨水风格残影 (Ink Silhouette)，边缘带有青色勾边。
    *   **重现**: 在新位置通过“墨水汇聚”的方式出现。

## 3. 资源需求清单 (Asset Requirements)

### 3.1 纹理 (Textures)
| 文件名 | 用途 | 类型 |
| :--- | :--- | :--- |
| `vfx_trail_smooth.png` | 刀光、冲刺拖尾 | Grayscale + Alpha |
| `vfx_noise_cloud.png` | 能量辉光、溶解、角色光环 | Noise |
| `vfx_circle_shockwave.png` | 冲击波、护盾旋转 | Radial Gradient |
| `vfx_scratch_mask.png` | 刀光纹理 | Directional Noise |
| `vfx_rune_array.png` | 剑阵地面法阵 | Mask |
| `vfx_ink_splatter.png` | 剑意爆发、水墨爆裂 | Alpha Mask |
| `ui_sword_icon.png` | 剑意层数UI图标 | Sprite |

### 3.2 Shaders
*   `vfx_trail.vert/frag`: 处理拖尾网格，支持 UV 滚动和顶点颜色淡出。
*   `vfx_distortion.frag`: 屏幕空间扭曲。
*   `vfx_dissolve.frag`: 基于噪声图的阈值溶解。
*   `vfx_particle_compute.glsl`: 剑雨粒子模拟。
*   `vfx_aura.frag`: 处理角色边缘光和剑意光环。

## 4. 实施计划 (Action Plan)
1.  **Phase 1**: 编写基础 VFX Shaders (Trail, Dissolve, Simple Particle).
2.  **Phase 2**: 使用 Python 脚本生成上述清单中的占位纹理。
3.  **Phase 3**: 在 `Game/Systems/RenderSystem` 中集成 `TrailManager`。
4.  **Phase 4**: 实现 **剑意系统 (Sword Intent)** 的状态可视化逻辑 (State -> Visual mapping) 及 UI。
