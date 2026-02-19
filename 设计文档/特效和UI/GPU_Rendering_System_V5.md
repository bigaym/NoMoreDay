# GPU 渲染系统 V5 — 完整设计规格书

> **文档版本**: 1.0  
> **创建日期**: 2026-02-15  
> **定位**: V4 实施完成后的**次世代全局光照预研与落地规范**  
> **基线来源**: `ARPG渲染引擎V3-V5规划.md` + 2026-02 Radiance Cascades / JFA 技术检索

---

## 目录

1. [项目背景与目标](#1-项目背景与目标)
2. [前置条件与 V4 产出依赖](#2-前置条件与-v4-产出依赖)
3. [JFA 距离场生成 Pass](#3-jfa-距离场生成-pass)
4. [辐射级联全局光照](#4-辐射级联全局光照)
5. [GPU 流体模拟（探索）](#5-gpu-流体模拟探索)
6. [与 V4 管线的集成点](#6-与-v4-管线的集成点)
7. [GPU ABI 契约（V5）](#7-gpu-abi-契约v5)
8. [性能预算与 Pass 级分配](#8-性能预算与-pass-级分配)
9. [Quality Tier 矩阵（V5）](#9-quality-tier-矩阵v5)
10. [分阶段实施路线](#10-分阶段实施路线)
11. [风险清单与缓解策略](#11-风险清单与缓解策略)
12. [验收标准（DoD）](#12-验收标准dod)
13. [附录：关键数据结构](#13-附录关键数据结构)
14. [外部参考](#14-外部参考)

---

## 1. 项目背景与目标

### 1.1 定位

V5 是引擎渲染技术栈的**最远期规划**，核心目标是在 2D 平面上实现全动态的全局光照（GI），让光线在墙壁、地板和物体之间真实反弹。

与 V4 的本质区别：
- **V4 = 画质层**：升级材质与光源系统，提升直接光照的质感（成熟技术，交付确定性高）
- **V5 = 算法层**：引入间接光照（GI），是渲染模型的范式变革（前沿研究，含预研性质）

### 1.2 V5 目标定义

> **核心命题**：2D ARPG 中首次实现无噪点、实时、全动态的全局光照——光线在场景中多次反弹，阴影与光晕自然过渡。

1. **辐射级联（Radiance Cascades）GI**：无噪点实时 2D 全局光照
2. **JFA 距离场**：为辐射级联提供高效的空间跳跃数据结构
3. **GPU 流体模拟（探索）**：SPH 血液/水面与 GI 交互

### 1.3 非目标

- 不迁移 Vulkan/Metal（保持 OpenGL 4.3+）
- 不引入 3D 光线追踪硬件加速（RT Cores）
- 不做屏幕空间反射（SSR）——2D 游戏无需
- 本阶段不改变 V4 已有的直接光照管线

### 1.4 预研声明

V5 包含前沿研究技术（辐射级联、Holographic RC），实施过程中如发现性能或质量无法达标，有权：
1. 降低级联层数或分辨率
2. 限制 GI 为仅 Ultra 档专属
3. 回退到 V4 纯直接光照（无 GI），作为**可接受的最终状态**

---

## 2. 前置条件与 V4 产出依赖

V5 启动需要 V4 以下产出**已完成并通过验收**：

| V4 产出 | V5 依赖原因 |
|---------|------------|
| PBR 材质管线（Normal/Mask） | GI 需要 Emission 通道标识发光体 |
| 全局高度场（R16F HeightMap） | 遮挡体提取依赖高度信息 |
| Clustered Forward+ V4 | GI 结果需要与直接光照叠加合成 |
| RenderGraph V4 Pass 序列 | GI Pass 需要插入到正确位置 |
| ABI V4 生成链路 | V5 结构体继承 V4 治理 |

**如果 V4 未全部完成**，V5 可以在以下条件下提前启动 JFA 预研：
- V4-A（GPU Text/Loot）已完成
- V4-B（PBR 材质）至少 Schema 与 Emission 通道已落地

---

## 3. JFA 距离场生成 Pass

### 3.1 Jump Flood Algorithm 概述

JFA 以 $O(\log N)$ 复杂度在 GPU 上生成 2D 距离场（SDF），是辐射级联光线行进的加速结构。核心思想：每个像素通过多轮迭代，以指数递减的步长向邻居传播"最近遮挡体坐标"。

### 3.2 算法流程

```
输入: OccluderMask (R8) — 遮挡体像素=1, 空白=0

Pass 0: SeedInitCS
├── 遮挡体像素 → 写入自身坐标 (x, y)
└── 空白像素 → 写入哨兵值 (0xFFFF, 0xFFFF)

Pass 1..log₂(N): JumpFloodCS (stepSize = N/2, N/4, ..., 1)
├── 每线程读取自身 + 8 邻居（距离 = stepSize）
├── 选择最近的 seed 坐标
└── 写入输出纹理（ping-pong 双缓冲）

Pass +1: JFA补偿 (stepSize = 2, 1) — 修正近场精度
├── 额外 2 轮小步长迭代
└── 修正 JFA 已知的对角线近似误差

Final: DistanceCS
├── 从最终 seed 坐标计算欧氏距离
├── 叠加符号（遮挡体内 = 负，外 = 正）
└── 输出 R16F SDF 纹理
```

### 3.3 遮挡体提取

```
遮挡体来源：
├── 静态地形：地图 chunk 的碰撞层 → 提前烘焙到 OccluderMask
├── 大型障碍物：墙壁/柱子 → 碰撞组件投影到 2D 掩码
├── 动态遮挡体：仅限关键实体（Boss 体型），按帧更新
└── 合成策略：
    ├── 静态层：仅在 chunk 加载时更新
    ├── 动态层：每帧覆写到掩码上
    └── 最终 OccluderMask = 静态 OR 动态
```

### 3.4 核心 Compute Shader

```glsl
// JumpFloodCS — 核心迭代 Pass
layout(local_size_x = 16, local_size_y = 16) in;

uniform int u_stepSize;

layout(rg16f, binding = 0) readonly  uniform image2D seedIn;
layout(rg16f, binding = 1) writeonly uniform image2D seedOut;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 texSize = imageSize(seedIn);

    vec2 bestSeed = imageLoad(seedIn, pos).xy;
    float bestDist = distance(vec2(pos), bestSeed);

    // 检查 8 个邻居
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;

            ivec2 neighbor = pos + ivec2(dx, dy) * u_stepSize;
            neighbor = clamp(neighbor, ivec2(0), texSize - 1);

            vec2 neighborSeed = imageLoad(seedIn, neighbor).xy;

            // 跳过未初始化的哨兵
            if (neighborSeed.x >= float(texSize.x)) continue;

            float d = distance(vec2(pos), neighborSeed);
            if (d < bestDist) {
                bestDist = d;
                bestSeed = neighborSeed;
            }
        }
    }

    imageStore(seedOut, pos, vec4(bestSeed, 0.0, 0.0));
}
```

```glsl
// DistanceCS — 最终距离计算
layout(local_size_x = 16, local_size_y = 16) in;

layout(rg16f,  binding = 0) readonly  uniform image2D seedFinal;
layout(r16f,   binding = 1) writeonly uniform image2D sdfOut;
layout(r8,     binding = 2) readonly  uniform image2D occluderMask;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    vec2 seed = imageLoad(seedFinal, pos).xy;
    float dist = distance(vec2(pos), seed);

    // 符号：遮挡体内为负
    float mask = imageLoad(occluderMask, pos).r;
    float signedDist = (mask > 0.5) ? -dist : dist;

    imageStore(sdfOut, pos, vec4(signedDist, 0.0, 0.0, 0.0));
}
```

### 3.5 性能特征

| 分辨率 | Pass 数 (含 JFA+1) | 预估耗时 (GTX 1060) | 预估耗时 (RTX 4070S) |
|--------|:---:|:---:|:---:|
| 512×512 | 11 | ~1.5ms | ~0.4ms |
| 1024×1024 | 12 | ~4.0ms | ~1.0ms |
| 1920×1080 | 13 | ~5.5ms | ~1.4ms |
| 960×540 (half-res) | 12 | ~2.0ms | ~0.5ms |

### 3.6 优化策略

1. **Half-res 运行**：High 档以半分辨率运行 JFA，上采样到全分辨率
2. **帧间隔更新**：静态场景下 SDF 每 2-4 帧更新一次
3. **增量更新**：仅动态遮挡体变化的 chunk 区域局部重算
4. **JFA+1 自适应**：当溢出计数为 0 时跳过补偿 Pass

---

## 4. 辐射级联全局光照

### 4.1 核心原理

辐射级联利用**半影假设**：距离越远的光源，空间分辨率需求越低，但角度分辨率要求越高。将 GI 计算分解为多个级联层，每层负责不同距离范围的光照传输。

**关键优势**：
- 计算成本与场景复杂度/光源数量无关（geometry-agnostic）
- 天然无噪点（近场高分辨率处理细节，远场低分辨率处理环境光）
- 无需传统光追的时域降噪（TAA/Denoiser）

### 4.2 级联架构设计

| 级联层 | 空间分辨率 | 探针间距 | 射线数/探针（Balanced） | 射线长度 | 角度覆盖 |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **L0** | 屏幕 1:1 | 1px | 4 | 0-4px | 4×90° |
| **L1** | 1/2 | 2px | 4 | 4-8px | 4×90° |
| **L2** | 1/4 | 4px | 8 | 8-16px | 8×45° |
| **L3** | 1/8 | 8px | 8 | 16-32px | 8×45° |
| **L4** | 1/16 | 16px | 12 | 32-64px | 12×30° |
| **L5** | 1/32 | 32px | 12 | 64-128px+ | 12×30° |

> 默认采用 `Balanced` 角分辨率增长策略。V5-A 固定该 profile，V5-B 才允许评估更激进配置（例如 L4/L5=16）。

### 4.3 渲染管线

```
┌──────────────────────────────────────────────────────┐
│        Radiance Cascades Pass (RenderGraph 节点)       │
│                                                        │
│  1. OccluderExtractCS → OccluderMask (R8)              │
│  2. JFA Pipeline → SDF (R16F)          [§3]           │
│  3. 级联计算（自顶向下合并）：                           │
│     L5 → L4 → L3 → L2 → L1 → L0                      │
│  4. L0 输出 → RadianceMap (RGBA16F, 屏幕分辨率)        │
│  5. GI 合成：LitHDR += RadianceMap * giIntensity       │
│                                                        │
│  每级操作:                                              │
│  ├── 发射射线（利用 SDF 空间跳跃加速）                   │
│  ├── 采样上一级辐射度（双线性空间+角度插值）              │
│  ├── 累加发光体贡献（从 Emission 通道读取）              │
│  └── 写入级联纹理                                       │
└──────────────────────────────────────────────────────┘
```

#### RenderGraph 中的位置

```
Scene → LightCulling → Shadow → Lighting → [Radiance Cascades] → Volumetric → VFX → ...
                                                    ↑
                                            插入在 Lighting 之后、
                                            Volumetric 之前
```

### 4.4 射线行进与 SDF 加速

```glsl
// SDF 加速的射线行进
vec4 traceRaySDF(vec2 origin, vec2 dir, float minDist, float maxDist,
                 sampler2D sdfTex, sampler2D emissiveTex, vec2 screenSize) {
    float t = minDist;
    vec4 result = vec4(0.0); // rgb = radiance, a = occlusion

    for (int i = 0; i < 32; ++i) {
        if (t >= maxDist) break;

        vec2 pos = origin + dir * t;
        vec2 uv  = pos / screenSize;

        // 越界检测
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) break;

        float sdfDist = texture(sdfTex, uv).r;

        if (sdfDist < 0.5) {
            // 命中遮挡体表面 → 采样发光
            vec3 emissive = texture(emissiveTex, uv).rgb;
            result.rgb = emissive;
            result.a   = 1.0; // 完全遮挡
            break;
        }

        // SDF 空间跳跃：安全步进 sdfDist
        t += max(sdfDist, 1.0);
    }

    return result;
}
```

### 4.5 级联合并算法

核心难点：低分辨率级联的辐射度需要无缝融合到高分辨率级联。

```glsl
// 从 Level N+1 合并到 Level N
vec4 mergeCascade(vec2 probeWorldPos, int level,
                  sampler2D upperLevelTex, sampler2D sdfTex, sampler2D emissiveTex,
                  vec2 screenSize) {
    int numRays = getCascadeRayCount(level); // 例如 [4,4,8,8,12,12]
    float baseInterval = 4.0; // 基础射线长度（像素）
    float nearRange = pow(2.0, float(level)) * baseInterval;
    float farRange  = nearRange * 2.0;

    vec4 totalRadiance = vec4(0.0);

    for (int r = 0; r < numRays; ++r) {
        float angle = (float(r) + 0.5) / float(numRays) * 2.0 * PI;
        vec2 dir = vec2(cos(angle), sin(angle));

        // 1. 当前级别射线追踪（近场）
        vec4 nearResult = traceRaySDF(probeWorldPos, dir, nearRange / 2.0, nearRange,
                                       sdfTex, emissiveTex, screenSize);

        if (nearResult.a < 1.0) {
            // 2. 未遮挡 → 从上一级获取远场辐射度
            vec2 upperProbeSize = screenSize / pow(2.0, float(level + 1));
            vec2 upperUV = probeWorldPos / screenSize;

            // 双线性插值（空间 + 角度）
            vec4 farResult = texture(upperLevelTex, upperUV);

            // 合并：近场贡献 + 远场透射
            totalRadiance.rgb += nearResult.rgb + farResult.rgb * (1.0 - nearResult.a);
        } else {
            // 遮挡 → 仅近场贡献
            totalRadiance.rgb += nearResult.rgb;
        }
    }

    return totalRadiance / float(numRays);
}
```

### 4.6 时域稳定性

为减少帧间闪烁，对 GI 输出做时域混合：

```glsl
vec3 finalGI = mix(currentFrameGI, previousFrameGI, temporalWeight);
// temporalWeight = 0.85~0.95（越高越稳定，但响应越慢）
```

- 相机移动时自动降低 `temporalWeight`（快速响应）
- 静止时提高 `temporalWeight`（最大稳定性）
- 场景光源突变时重置 temporal history

### 4.7 Holographic Radiance Cascades（前沿探索）

2025 年 arXiv 论文提出的单次（single-shot）、场景无关的辐射传输算法：

| 属性 | 标准 RC | Holographic RC |
|------|---------|----------------|
| 计算模式 | 逐级合并（6 pass） | 单次全局计算 |
| 耗时 (512²) | ~1.2ms | **~1.85ms** |
| 耗时 (1024²) | ~3.5ms | **~7.67ms** |
| 质量 | 优秀 | 与参考解无法区分 |
| 成熟度 | 多个开源实现 | 仅论文阶段 |

**V5 策略**：
1. V5-A 阶段使用标准 RC 作为主方案
2. V5-B 阶段评估 Holographic RC 的工程可行性
3. 若 Holographic RC 在我们的场景下性能更优，可替代标准 RC

### 4.8 发光体管理

GI 系统需要知道"哪些表面是发光的"。发光体来源：

| 来源 | 数据通道 | 更新频率 |
|------|----------|----------|
| 场景灯光（火把/水晶） | V4 LightManager 投影到 Emissive Buffer | 每帧 |
| 自发光材质（岩浆/魔法纹路） | Material Mask.A (Emission) 通道 | 静态 |
| VFX 粒子（火焰/闪电） | 粒子 emissive 参数写入 Emissive Buffer | 每帧 |
| 技能特效 | VFX Sequencer 驱动 | 事件触发 |

**Emissive Buffer**：RGBA16F，屏幕分辨率，存储场景中所有发光面的辐射度。

---

## 5. GPU 流体模拟（探索）

### 5.1 SPH 基础方案

使用 Smoothed Particle Hydrodynamics 模拟血液飞溅、水面波纹。

#### 核心循环（Compute Shader）

```
1. NeighborSearchCS: 空间哈希网格邻居搜索
2. DensityCS:       SPH 核函数（Poly6）计算密度/压力
3. ForceCS:         压力梯度 + 粘性 + 重力 + 表面张力（Spiky 核）
4. IntegrateCS:     Leapfrog 积分更新位置
5. RenderCS:        粒子 → 屏幕空间 metaball / sprite
```

#### 空间哈希网格

与 V4 战利品标签避让（§4.2）共用相同基础设施：
- 网格粒度：流体粒子的 2×smoothingRadius
- Cell 容量：32 粒子/Cell
- 哈希方案：Morton code / Z-order curve

### 5.2 与 GI 的交互

| 交互方式 | 实现 |
|----------|------|
| 流体作为发光体 | 岩浆/魔法流体的粒子 emissive 注入 Emissive Buffer |
| 流体作为遮挡体 | 高密度液面区域更新 OccluderMask（影响 JFA） |
| GI 反弹到流体 | 流体表面从 RadianceMap 采样间接光，影响流体渲染颜色 |

### 5.3 性能约束

- 粒子上限：10,000（Ultra 档）/ 5,000（High 档）
- 目标帧预算：< 1.5ms
- 超预算时自动降低粒子数或切换为简化粒子特效（不做 SPH，仅视觉）

### 5.4 V5 范围界定

SPH 在 V5 中定位为**技术探索**：
- 原型验证可行性与视觉效果
- 若效果/性能不满意，可不纳入正式发布
- 不阻断 V5 的 GI 核心交付

---

## 6. 与 V4 管线的集成点

### 6.1 RenderGraph Pass 序列（V5 完整版）

```
Scene → LightCulling → Shadow → Lighting
    → [OccluderExtract] → [JFA] → [RadianceCascades] → [GI Composite]
    → Volumetric → VFX → UIWorld → PostProcess → Distortion → Composite
```

新增 Pass（方括号）：
1. **OccluderExtractPass**：从场景数据生成 OccluderMask
2. **JFAPass**：距离场生成（多 sub-pass）
3. **RadianceCascadesPass**：GI 核心计算（6 级联 sub-pass）
4. **GICompositePass**：将 RadianceMap 叠加到 LitHDR

### 6.2 与 V4 特性的协同

| V4 特性 | V5 协同方式 |
|---------|------------|
| BRDF-Lite | GI 提供间接光照的漫反射分量，与直接光照的 BRDF-Lite 叠加 |
| Height Shadow | 高度场同时服务于遮挡体提取 + 直接光阴影 |
| Clustered Lighting | V4 直接光 + V5 间接光 = 完整光照 |
| Material Emission | 材质 Emission 通道驱动 GI 发光源 |
| GPU Loot | 战利品 HDR Glow 在 GI 影响下更自然 |

### 6.3 Feature Flag 控制

```cpp
// V5 全局开关
render.gi.enabled = true/false          // 总开关
render.gi.cascadeLevels = 4/6           // 级联层数
render.gi.halfResolution = true/false   // 半分辨率模式
render.gi.temporalWeight = 0.9          // 时域混合
render.gi.sdfUpdateInterval = 1/2/4     // SDF 更新频率（帧）
render.fluid.enabled = true/false       // 流体开关
render.fluid.maxParticles = 10000       // 流体粒子上限
```

---

## 7. GPU ABI 契约（V5）

### 7.1 版本策略

`GPU_ABI_VERSION: 4 → 5`

### 7.2 新增结构

| 结构 | 用途 | 大小 |
|------|------|:---:|
| `RadianceCascadeConfig` | 级联参数 | 32B |
| `GPUFluidParticle` | SPH 粒子 | 48B |
| `GPUFluidConfig` | 流体参数 | 32B |

### 7.3 复用结构（来自 V4，不变更）

- `GPUMaterialDataV3` — 读取 Emission 通道
- `GPULightV2` — 投影到 Emissive Buffer

### 7.4 强制规则

延续 V4 全部 ABI 规则，额外要求：
1. JFA 纹理格式（RG16F, R16F）必须在 `RenderConstants.hpp` 注册
2. 级联纹理数组的层数/格式由 `RadianceCascadeConfig` 集中管控
3. SDF / Emissive / RadianceMap 的 binding 不得与 V4 现有 Pass 冲突

---

## 8. 性能预算与 Pass 级分配

### 8.1 预算策略

V5 新增 Pass 的帧预算**不是额外叠加**，而是需要通过以下方式腾出空间：

1. GI 开启时自动降低直接光照复杂度（减少 Cluster 光源上限）
2. GI 提供的间接光使得部分直接光源可移除（美术减光）
3. 帧间隔更新 SDF（非每帧）
4. Half-res 运行（High 档）

### 8.2 V5 新增 Pass 预算

| Pass | 常规 (270FPS) | 高压 (180FPS) | 极限 (144FPS) | 备注 |
|------|:---:|:---:|:---:|------|
| OccluderExtractPass | 0.10ms | 0.15ms | 0.20ms | 轻量 |
| JFA Pass | 0.40ms | 0.60ms | 0.80ms | 13 sub-pass @1080p |
| RadianceCascadesPass | 1.20ms | 1.80ms | 2.50ms | 6 级联 |
| GI CompositePass | 0.05ms | 0.08ms | 0.10ms | 轻量叠加 |
| SPH FluidPass | 0.30ms | 0.60ms | 0.80ms | 10k 粒子 |
| **V5 新增总计** | **2.05ms** | **3.23ms** | **4.40ms** | — |

### 8.3 帧预算腾挪策略

| 被降级的 V4 Pass | 节省 | 触发条件 |
|-----------------|:---:|----------|
| Clustered 光源: 4096→1024 | ~0.15ms | GI 开启 |
| HeightShadow: 64→16 steps | ~0.30ms | GI 开启（GI 提供软阴影替代） |
| VFX 粒子上限降 30% | ~0.20ms | GI + 极限场景 |
| POM 关闭 | ~0.20ms | GI + 极限场景 |
| SDF 每 2 帧更新 | ~0.20ms/帧均摊 | 静态/低动态场景 |
| **可腾挪总计** | **~1.05ms** | — |

### 8.4 最终帧预算（V4 + V5 联合）

| 场景 | V4 总计 | V5 新增 | V4 降级节省 | 实际总计 | 帧预算 | 满足？ |
|------|:---:|:---:|:---:|:---:|:---:|:---:|
| 常规 | 3.90ms | 2.05ms | -1.05ms | **4.90ms** | 3.70ms | ⚠️ 需 half-res |
| 高压 | 6.45ms | 3.23ms | -1.05ms | **8.63ms** | 5.56ms | ⚠️ 需 half-res + 降级 |
| 极限 | 8.75ms | 4.40ms | -1.05ms | **12.10ms** | 6.94ms | ❌ 仅 Ultra 允许超帧 |

> **结论**：V5 GI 在 Ultra 档为"尽力而为"特性，常规/高压场景必须使用 half-res 级联 + 帧间隔更新。极限场景下 GI 自动关闭或降为 4 级联。

---

## 9. Quality Tier 矩阵（V5）

| 能力 | Low | Medium | High | Ultra |
|------|:---:|:------:|:----:|:-----:|
| GI | Off | Off | 4-cascade half-res | 6-cascade full-res |
| JFA SDF | Off | Off | Half-res, 每 2 帧 | Full-res, 每帧 |
| SDF 动态遮挡体 | Off | Off | 关键实体 only | 全部 |
| GI temporal blend | — | — | 0.92 | 0.88 |
| SPH Fluid | Off | Off | Off | 10k particles |
| Emissive Buffer | Off | Off | Half-res | Full-res |

---

## 10. 分阶段实施路线

### Phase V5-A（12-16 周）：GI 基础设施与原型

| 任务 | 描述 | 依赖 |
|------|------|------|
| 遮挡体提取 | OccluderExtractPass + 静态/动态合成 | V4 HeightMap |
| JFA SDF Pass | Compute Shader 距离场 + JFA+1 补偿 | 遮挡体 |
| Emissive Buffer | 场景发光体投影 | V4 Material Emission |
| 辐射级联原型 | 4 级联 + half-res + 时域混合 | JFA + Emissive |
| GI Composite | 叠加到 LitHDR | RC 原型 |
| 性能基线 | 验证 half-res 下满足帧预算 | 全部 |

**验收标准**：4 级联 half-res GI 在至少 2 个场景（洞穴/城镇）呈现可辨的间接光照和色溢效果。

### Phase V5-B（16-20 周）：完整体与探索

| 任务 | 描述 | 依赖 |
|------|------|------|
| 完整 6 级联 | 全分辨率 + 合并优化（Ultra 专属） | V5-A |
| SDF 增量更新 | 动态场景优化（chunk 局部重算） | V5-A |
| SPH 流体原型 | Compute 流体 + GI 交互 | V5-A |
| Holographic RC 评估 | 论文复现 + 性能对比 | V5-A |
| 全链路验收 | 性能/稳定性/视觉回归 | 全部 |

**验收标准**：Ultra 档 6 级联 full-res 在常规场景下帧率 ≥ 180 FPS（允许 V4 自动降级配合）。

### 依赖关系图

```
V4 全部完成（或至少 V4-B Material Emission 落地）
  ↓
V5-A: 遮挡体提取 → JFA → Emissive Buffer → RC 原型 → GI Composite
  ↓
V5-B: 完整 6 级联 + SDF 优化 + SPH 探索 + Holographic RC 评估
```

---

## 11. 风险清单与缓解策略

| ID | 风险 | 影响 | 概率 | 缓解 |
|:---:|------|------|:---:|------|
| V5-R01 | JFA 精度不足导致 GI 漏光 | 视觉错误 | 中 | JFA+1 补偿 + 误差阈值检测 + 降级到 JFA+2 |
| V5-R02 | 辐射级联在复杂场景仍有噪点 | 画面品质 | 低 | L0 射线增至 8 + temporal 权重增至 0.95 |
| V5-R03 | 帧预算不足（尤其极限场景） | 性能 | **高** | GI 强制 half-res + 减少级联 + 帧间隔 + 极限场景关闭 GI |
| V5-R04 | 时域混合导致运动拖影 | 视觉瑕疵 | 中 | 相机移动时降低 temporal + motion-aware reprojection |
| V5-R05 | Emissive Buffer 与 VFX 频繁写入竞争 | 性能/正确性 | 中 | VFX 写入使用独立 sub-buffer + 合并阶段原子操作 |
| V5-R06 | SPH 流体不稳定（粒子逃逸/爆炸） | 视觉错误 | 中 | Leapfrog 积分 + CFL 条件限制 + 粒子回收机制 |
| V5-R07 | OpenGL 4.3 计算能力天花板 | 架构限制 | 低 | 预研 Vulkan 迁移路径（V6 范畴，不阻断 V5） |
| V5-R08 | Holographic RC 无法在 OGL 4.3 高效实现 | 探索失败 | 中 | 使用标准 RC 作为主方案，Holographic 仅为奖励目标 |

### 兜底策略

**如果 V5 GI 最终无法达到可接受的性能/质量水平**：
1. GI 降级为仅 Ultra 档、仅洞穴/室内场景启用
2. 用 V4 的多光源 + 体积光近似间接光照效果
3. 回退到 V4 作为正式发布的渲染基线（V4 已具备 D4/PoE 级直接光照）
4. GI 技术储备保留，等待 Vulkan 迁移（V6）后重新启用

---

## 12. 验收标准（DoD）

### 12.1 功能门禁

1. JFA SDF 在 1080p 下精度误差 < 2px（与精确距离场对比）
2. 4 级联 half-res GI 在至少 3 个场景类型下呈现可辨间接光照
3. 6 级联 full-res GI 在洞穴场景下呈现明显的颜色溢出（color bleeding）
4. 时域混合在相机移动时无可察觉拖影
5. GI 与 V4 直接光照叠加后无过曝/过暗

### 12.2 性能门禁

1. Ultra 档常规场景 ≥ 180 FPS（允许 V4 联合降级）
2. High 档（half-res GI）常规场景 ≥ 270 FPS
3. 极限场景下 GI 自动关闭后性能恢复到 V4 水平
4. JFA 单独耗时 ≤ 1.5ms @1080p (4070S)
5. 6 级联单独耗时 ≤ 2.5ms @1080p (4070S)

### 12.3 契约门禁

1. ABI V5 layout 测试通过
2. 新增 binding 不与 V4 Pass 冲突
3. RenderGraph 合同验证通过（含 GI Pass 链）
4. SDF 纹理格式在 RenderConstants 注册

### 12.4 稳定性门禁

1. 30 分钟 GI 压力运行无显存持续增长
2. GI 开关切换无黑帧
3. SDF 增量更新无累积误差

### 12.5 回退门禁

1. `render.gi.enabled = false` 后完整回退到 V4 纯直接光照
2. 回退后 V4 全功能正常、性能恢复
3. `render.fluid.enabled = false` 后 SPH 资源完全释放

---

## 13. 附录：关键数据结构

### 13.1 RadianceCascadeConfig (32 bytes)

```cpp
struct RadianceCascadeConfig {
    uint32_t numLevels;         // 4  级联层数 (4 or 6)
    uint32_t raysPerProbe;      // 4  L0 基准射线数（每级射线由 profile 派生）
    float    baseInterval;      // 4  基础射线长度（像素）
    float    temporalWeight;    // 4  时域混合权重 (0.85-0.95)
    uint32_t halfResolution;    // 4  L0 是否 half-res (0/1)
    uint32_t sdfUpdateInterval; // 4  SDF 更新间隔（帧数）
    float    giIntensity;       // 4  GI 强度乘数
    uint32_t reserved;          // 4
};
static_assert(sizeof(RadianceCascadeConfig) == 32);
```

### 13.2 GPUFluidParticle (48 bytes)

```cpp
struct GPUFluidParticle {
    glm::vec2 position;     // 8
    glm::vec2 velocity;     // 8
    float     density;      // 4
    float     pressure;     // 4
    glm::vec4 color;        // 16  (rgb + emissive intensity)
    float     lifetime;     // 4
    uint32_t  flags;        // 4   type, active, etc.
};
static_assert(sizeof(GPUFluidParticle) == 48);
```

### 13.3 GPUFluidConfig (32 bytes)

```cpp
struct GPUFluidConfig {
    float    smoothingRadius;  // 4
    float    restDensity;      // 4
    float    stiffness;        // 4
    float    viscosity;        // 4
    glm::vec2 gravity;         // 8
    float    surfaceTension;   // 4
    uint32_t maxParticles;     // 4
};
static_assert(sizeof(GPUFluidConfig) == 32);
```

---

## 14. 外部参考

### Radiance Cascades
1. Jason Bourg — Radiance Cascades: <https://jason.today/rc>
2. radiance-cascades.com: <https://radiance-cascades.com/>
3. Holographic Radiance Cascades (arXiv 2025): <https://arxiv.org/abs/2505.02041>

### JFA 距离场
4. Rong, G. and Tan, T.-S. — Jump Flooding in GPU with Applications to Voronoi Diagram and Distance Transform (I3D 2006): <https://doi.org/10.1145/1111411.1111431>
5. Rong, G. and Tan, T.-S. — Variants of Jump Flooding Algorithm for Computing Discrete Voronoi Diagrams (ISVD 2006): <https://www.comp.nus.edu.sg/~tants/jfa-variants.html>
6. Cao, T.-T. et al. — Parallel Banding Algorithm to Compute Exact Distance Transform with the GPU (I3D 2010): <https://doi.org/10.1145/1730804.1730818>

### SPH 流体
7. Müller et al. — Particle-Based Fluid Simulation for Interactive Applications: <https://matthias-research.github.io/pages/publications/sca03.pdf>

### OpenGL Compute
8. Khronos OpenGL Wiki — glMemoryBarrier: <https://wikis.khronos.org/opengl/GlMemoryBarrier>
9. OpenGL Shading Language 4.60 Specification (memory model / barriers): <https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf>

---

> **修订说明（2026-02-15）**  
> 本文档为 V5 首版设计规格书，从 `GPU_Rendering_System_V4_V5.md` 拆分而来。  
> V5 定位为 V4 完成后的**次世代 GI 预研与落地**，含明确的预研声明与兜底策略。  
> 后续实施需基于 V4 实际性能基线修订 §8 帧预算。  
> V4 规范见 `GPU_Rendering_System_V4.md`。
