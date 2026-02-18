# GPU 渲染系统 V4 — 完整设计规格书

> **文档版本**: 1.0  
> **创建日期**: 2026-02-15  
> **定位**: V3 实施完成后的**画质与效率全面升级规范**，作为 V4 实施、验收与发布门禁的统一基线  
> **基线来源**: `ARPG渲染引擎V3-V5规划.md` + `GPU_Rendering_System_3.md` + 2026-02 技术检索

---

## 目录

1. [项目背景与目标](#1-项目背景与目标)
2. [版本对照表](#2-版本对照表)
3. [GPU 驱动文字渲染系统](#3-gpu-驱动文字渲染系统)
4. [GPU 驱动战利品渲染系统](#4-gpu-驱动战利品渲染系统)
5. [2D PBR 材质标准](#5-2d-pbr-材质标准)
6. [Clustered Forward+ 完整扩展](#6-clustered-forward-完整扩展)
7. [2.5D 高度图光影系统](#7-25d-高度图光影系统)
8. [GPU ABI 契约（V4）](#8-gpu-abi-契约v4)
9. [性能预算与 Pass 级分配](#9-性能预算与-pass-级分配)
10. [Quality Tier 矩阵（V4）](#10-quality-tier-矩阵v4)
11. [分阶段实施路线](#11-分阶段实施路线)
12. [风险清单与缓解策略](#12-风险清单与缓解策略)
13. [验收标准（DoD）](#13-验收标准dod)
14. [附录：关键数据结构](#14-附录关键数据结构)
15. [外部参考](#15-外部参考)

---

## 1. 项目背景与目标

### 1.1 前置条件

V3（`GPU_Rendering_System_3.md`）完成后，引擎将具备：

- 契约化 RenderGraph 管线（ABI / Binding / Frame Ownership 全治理）
- 2.5D Hybrid Shadow（SDF + Atlas）
- Clustered 2D Lighting（基础形态，256 光源）
- Material 2.0（Normal / Roughness / Specular 基础分支）
- VFX 与光影联动（schema v3）
- 四档 Quality Tier 自动降级与回退

### 1.2 V4 目标定义

> **核心命题**：虽然是 2D 贴图，但光影质感如同 3D 建模；同时彻底消灭 CPU 热路径瓶颈。

1. **GPU 驱动文字渲染**：MSDF + Compute Shader 排版，消灭伤害数字 CPU 瓶颈
2. **GPU 驱动战利品渲染**：MDI + GPU 物理避让，支持同屏 **1000+** 战利品零 CPU 负载
3. **2D PBR 材质标准**：Albedo / Normal / Mask 贴图管线，实现"同光异材"层次感
4. **Clustered Forward+ 完整体**：同屏 **4096** 动态光源，每个火花都能照亮周围物体
5. **2.5D 高度图光影**：基于高度场的实时阴影与视差遮挡，极大增强立体感

### 1.3 非目标

- 不迁移 Vulkan/Metal/DX12（保持 OpenGL 4.3+）
- 不引入完整 3D PBR 管线（保持 2D ARPG 定位）
- 不包含全局光照（GI 属于 V5 范畴）
- 不替换 Raylib 底层框架

### 1.4 V4 实施前依赖检查（承接 V3 未完成项）

以下依赖项来自 `v3_validation_and_release_gate_20260215` 收尾阶段的已知未闭环内容，
必须在 V4 编码实施前逐项确认：

| 依赖 ID | 来源 | 当前状态（2026-02-18） | 解除条件（进入稳定开发） | 验证证据 |
|---|---|---|---|---|
| `DEP-V3-F4.6` | `F4.6 perf_clustered_uplift` | 临时豁免中（`WVR-20260218-F4.6-001`），`clustered_128_improvement_pct=0.502 < 5.0` | 移除豁免，且连续 3 次 release perf 满足 `>=5.0%` | `bin/release_gate/v3_gate_report.json` + `conductor/validation/v3_gate_waivers.json` |
| `DEP-V3-F6.2` | `F6.2 screenshot_compare` | 截图基线未就绪（上游系统尚未全部接入），当前通过 `--allow-missing-screenshots` 降级 | 补齐 6 个关键场景 baseline/candidate 数据，截图比较转为无 warning | `conductor/validation/v3_screenshot_manifest.json` + `bin/release_gate/screenshots/screenshot_report.json` |

实施规则：
1. V4-A 开工前，先执行一次 `build.bat gate`，确认以上依赖状态与文档一致。
2. 若 `DEP-V3-F4.6` 仍处于豁免，必须在迭代计划中单列性能修复任务并绑定 `BUG-20260218-001`。
3. 若 `DEP-V3-F6.2` 尚未解除，V4 阶段验收不得宣称“视觉回归全绿”，仅可标记为“截图门禁待上游完成”。

---

## 2. 版本对照表

> 由于项目演进过程中各阶段实际覆盖范围与最初路线规划存在偏移，特此明确对照关系。

| 路线规划版本 | 路线规划核心目标 | 项目实际版本 | 说明 |
|:---:|---|:---:|---|
| V3 | GPU 驱动架构（MDI, Text, Loot, RenderGraph） | V2 已部分实现，**V4-A** 补完 | RenderGraph/MDI 在 V2 落地；Text/Loot 纳入 V4 第一阶段 |
| V4 | 画质升级（PBR, Clustered Forward+, HeightMap） | **V3**(基础) + **V4**(完整) | V3 覆盖 Clustered 基础/Material 2.0；本文档覆盖完整版 |
| V5 | 次世代 GI（Radiance Cascades, JFA, SPH） | **V5** | 见 `GPU_Rendering_System_V5.md` |

---

## 3. GPU 驱动文字渲染系统

> **解决的问题**：V2 文字渲染（PopupRenderer）依赖 CPU 逐帧重建顶点，在暴击连锁时成为帧率杀手。

### 3.1 MSDF 字体图集

#### 3.1.1 技术选型

采用 **Multi-Channel Signed Distance Field (MSDF)** 技术，使用开源库 `msdfgen` / `msdf-atlas-gen` 离线生成。

| 属性 | 规格 |
|------|------|
| 图集尺寸 | 4096×4096，单张覆盖全部常用汉字 + ASCII + 符号 |
| 通道格式 | RGB8（3 通道距离场）或 RGBA8（含 Alpha 辅助） |
| 压缩 | BC4/BC5 可选（Ultra 档用原始，Low 档用压缩） |
| 字形度量 | 离线导出为 JSON/二进制，包含 advance/bearing/size |

#### 3.1.2 Shader 解码逻辑

```glsl
// MSDF 边缘重建 — Fragment Shader
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// screenPxRange: 基于屏幕像素与 SDF 纹素的比值
float screenPxDistance = screenPxRange * (median(sample.r, sample.g, sample.b) - 0.5);
float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
```

**优势**：单张 4K 贴图支持无限缩放保持锐利，完美适配 ARPG 中忽大忽小的暴击数字。

#### 3.1.3 中文字形溢出策略

单张 4096² 在 32px 字形大小下可容纳约 16,384 字形。常用汉字约 6,763 字（GB2312），加上 ASCII/符号可一次装入。

若需覆盖更大字符集：
- **Primary Atlas**：常用 8K 字形（高频字 + ASCII + 数字 + 符号）
- **Secondary Atlas**：低频字形，LRU 按需动态加载
- **回退**：未命中的字形使用矩形占位符 + 日志警告

### 3.2 Compute Shader 排版管线

#### 3.2.1 数据流架构

```
┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  CPU: 事件触发    │ →   │  GPU: 排版 CS     │ →   │  GPU: MDI 绘制   │
│  TextCommand      │     │  Layout Kernel    │     │  Indirect Draw   │
│  {pos, stringId,  │     │  → 前缀和计算     │     │  → Instanced     │
│   value, color}   │     │  → 顶点生成       │     │    Quad 渲染     │
└──────────────────┘     │  → 原子计数       │     └──────────────────┘
                          └──────────────────┘
```

#### 3.2.2 GPU 数据结构

```glsl
// CPU → GPU 指令（16 bytes）
struct GPUTextCommand {
    vec2  worldPos;       // 8  世界坐标
    uint  stringId;       // 4  预定义字符串 ID 或动态字串 buffer 偏移
    uint  colorAndFlags;  // 4  颜色(24bit) + flags(8bit: 字体大小/动画类型)
};

// 字形度量缓冲区（只读 SSBO，初始化时上传）
struct GPUGlyphMetrics {
    vec4  uvRect;         // 16 图集中的 UV 矩形
    vec2  offset;         // 8  bearing 偏移
    vec2  size;           // 8  字形像素尺寸
    float advance;        // 4  进给量
    float reserved;       // 4
};
// Total: 40 bytes per glyph

// 排版输出（每字符一个 Quad 实例）
struct GPUTextQuad {
    vec2  screenPos;      // 8
    vec2  size;           // 8
    vec4  uvRect;         // 16
    uint  colorPacked;    // 4
    float opacity;        // 4
};
// Total: 40 bytes per quad
```

#### 3.2.3 并行前缀和排版算法

对于短字符串（伤害数字通常 < 10 字符），采用 **Warp-level Intrinsics** 实现微秒级排版：

```glsl
// Compute Shader — 每个 workgroup 处理一条 TextCommand
layout(local_size_x = 32) in; // 一个 warp

void main() {
    uint charIdx = gl_LocalInvocationID.x;
    uint cmdIdx  = gl_WorkGroupID.x;

    TextCommand cmd = textCommands[cmdIdx];
    uint charCode   = getChar(cmd.stringId, charIdx);

    // 1. 加载字形 advance
    float adv = (charIdx < stringLength) ? glyphMetrics[charCode].advance : 0.0;

    // 2. Warp-level inclusive prefix sum（利用 subgroup 指令）
    float xOffset = subgroupInclusiveAdd(adv) - adv;

    // 3. 生成 Quad 数据
    if (charIdx < stringLength) {
        GPUTextQuad quad;
        quad.screenPos   = cmd.worldPos + vec2(xOffset * fontSize, 0.0);
        quad.size        = glyphMetrics[charCode].size * fontSize;
        quad.uvRect      = glyphMetrics[charCode].uvRect;
        quad.colorPacked = cmd.colorAndFlags & 0xFFFFFF;
        quad.opacity     = computeOpacity(cmd, frameTime);

        // 4. 原子写入全局 RenderBuffer
        uint idx = atomicAdd(indirectArgs.instanceCount, 1);
        renderBuffer[idx] = quad;
    }
}
```

#### 3.2.4 动画系统

所有文字动画在 Compute Shader 中通过 `lifetime` 参数驱动，**CPU 零介入**：

| 动画类型 | 算法 | 参数 |
|----------|------|------|
| 飘字上升 | `y += speed * t` | speed, duration |
| 重力下落 | `y += v0*t - 0.5*g*t²` | v0, gravity |
| 淡出 | `opacity = 1.0 - smoothstep(fadeStart, duration, t)` | fadeStart |
| 缩放弹跳 | `scale = 1.0 + amplitude * sin(freq*t) * exp(-decay*t)` | amplitude, freq, decay |
| 暴击放大 | `scale = lerp(2.0, 1.0, easeOutBack(t))` | — |

### 3.3 迁移策略

1. **Phase 1**：新建 `GPUTextSystem`，与 `PopupRenderer` 并存，`render.gpuText.enabled` 切换
2. **Phase 2**：验证全 Tier 表现一致后，废弃 `PopupRenderer`
3. **回退**：Feature Flag 随时回退到 CPU 路径

---

## 4. GPU 驱动战利品渲染系统

### 4.1 MDI 自动合批管线

所有地面掉落物共享同一 Shader，通过 `glMultiDrawElementsIndirect` 单次调用绘制。

#### 4.1.1 剔除流程

```
LootBuffer (所有掉落物, SSBO)
    ↓ [FrustumCullCS]  视锥剔除 (BoundingSphere vs 6 Planes)
VisibleIndexBuffer (可见物品索引)
    ↓ [IndirectArgsCS]  更新 DrawElementsIndirectCommand.instanceCount
    ↓ [glMultiDrawElementsIndirect]
Screen Output
```

#### 4.1.2 间接命令结构

```cpp
struct DrawElementsIndirectCommand {
    uint32_t count;         // 索引数量
    uint32_t instanceCount; // 由 GPU 剔除 CS 动态写入
    uint32_t firstIndex;    // 索引偏移
    int32_t  baseVertex;    // 顶点偏移
    uint32_t baseInstance;  // 实例基准
};
```

### 4.2 GPU 力导向标签避让算法

#### 4.2.1 算法概述

将 CPU 上 $O(N^2)$ 的矩形碰撞检测替换为 GPU 上**基于网格空间划分的力导向布局**，在 2-3 帧迭代内收敛。

#### 4.2.2 空间网格划分

```
屏幕空间划分为 32×32 像素的 Cell
(1920×1080 → 60×34 = 2,040 个 Cell)

每个 Cell 维护：
├── labelCount (uint)    — 当前 Cell 内标签数量
└── labelIndices[MAX_LABELS_PER_CELL] — 标签索引列表（原子写入）
```

#### 4.2.3 三阶段 Compute Pipeline

```
Stage 1: GridHashCS (散列)
├── 每线程处理一个可见标签
├── 计算标签中心所在 Cell
└── atomicAdd 写入 Cell 的 labelIndices[]

Stage 2: RepulsionCS (斥力计算)
├── 每线程处理一个可见标签
├── 读取自身 Cell + 8 邻居 Cell 的标签列表
├── 对每个邻居标签计算斥力向量：
│   dir = normalize(self.pos - other.pos)
│   overlap = max(0, minDist - distance(self, other))
│   force += dir * overlap * stiffness
└── 写入 forceBuffer[]

Stage 3: PositionUpdateCS (位置更新)
├── labelOffset += force * damping
├── clamp 到屏幕边界
└── 仅修改显示偏移，不改逻辑坐标
```

#### 4.2.4 关键参数

| 参数 | 建议值 | 说明 |
|------|--------|------|
| `cellSize` | 32px | 网格粒度 |
| `stiffness` | 0.5 | 弹性系数 |
| `damping` | 0.7 | 阻尼系数（每帧衰减） |
| `minDist` | 标签高度 + 4px | 最小间距 |
| `maxIterPerFrame` | 1 | 每帧一次迭代，2-3 帧自然收敛 |
| `MAX_LABELS_PER_CELL` | 8 | Cell 容量上限，溢出丢弃最远标签 |

### 4.3 迁移策略

- `render.gpuLoot.enabled` Feature Flag 控制
- Low/Med 保留简化 CPU 路径
- High/Ultra 使用 GPU 管线

---

## 5. 2D PBR 材质标准

### 5.1 贴图规范

所有场景元素和角色将升级为四层贴图：

| 贴图层 | 格式 | 通道说明 | 美术要求 |
|--------|------|----------|----------|
| **Albedo** | RGBA8 | 基础颜色（去除烘焙光影） | 纯色彩信息，无明暗 |
| **Normal** | RGB8 (线性) | 切线空间法线（Z 朝屏幕外为主） | 手绘或 3D 预渲染导出 |
| **Mask** | RGBA8 | R=Roughness, G=Metallic, B=Height/AO, A=Emission | 单张复用四通道 |
| **Detail** (可选) | RG8 | 细节法线缩放 | Ultra 档专属 |

### 5.2 2D PBR 光照模型 — BRDF-Lite

#### 5.2.1 设计原则

2D Sprite 的法线主要朝 Z 轴（屏幕外），边缘根据 Normal Map 偏转。采用简化的 Cook-Torrance，但**不是完整 PBR**——保留美术参数驱动风格，计算过程物理可解释。

#### 5.2.2 核心 Shader

```glsl
vec3 brdfLite(vec3 N, vec3 L, vec3 V, vec3 albedo, float roughness, float metallic) {
    vec3 H = normalize(L + V);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float NdotV = max(dot(N, V), 0.001);

    // NDF: GGX (Trowbridge-Reitz)
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * denom * denom);

    // Geometry: Schlick-GGX 简化
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = (NdotV / (NdotV * (1.0 - k) + k))
            * (NdotL / (NdotL * (1.0 - k) + k));

    // Fresnel: Schlick + 2D 边缘抑制
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F  = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);
    F *= mix(1.0, 0.3, step(0.7, 1.0 - NdotV)); // 抑制纸片人边缘高光

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NdotL;
}
```

#### 5.2.3 2D 专用修正

| 问题 | 解决方案 |
|------|----------|
| Sprite 边缘不自然的菲涅尔高光 | Fresnel 乘以 `mix(1.0, 0.3, step(0.7, 1-NdotV))` 抑制 |
| 法线主方向过于统一 | Roughness 做 bias: `roughness = roughness * 0.8 + 0.1` |
| 2D 精灵没有真实背面 | 禁用 backface 光照计算 |

#### 5.2.4 Sprite-Based AO

- **离线烘焙**：美术阶段根据 Sprite 深度信息烘焙 AO 到 Mask.B 通道
- **运行时合成**：`finalAO = min(bakedAO, ssaoSample) * aoIntensity`

### 5.3 美术资产管线升级

```
原始 Sprite (Albedo only)
    ↓
┌───────────────────────────────────┐
│  离线工具链 (Python/C++ CLI)      │
│  ├── Height Map 生成 (AI 辅助)    │
│  ├── Normal Map 从 Height 导出    │
│  ├── AO 从 Height 烘焙           │
│  ├── Roughness/Metallic 手绘/AI   │
│  └── Mask 四通道打包              │
└───────────────────────────────────┘
    ↓
Texture2DArray (Albedo层 + Normal层 + Mask层)
    ↓
MaterialManager → SSBO<GPUMaterialDataV3>
```

### 5.4 与 V3 Material 2.0 的关系

V3 Material 2.0 已引入 `normalMapSlot`、`roughness`、`specular` 等字段。V4 在此基础上：
1. 将 Schema 升级为 `material_schema_version: 3`
2. 增加 `fresnelControl`、`uvParams`、`detailParams` 控制
3. GPU 结构从 `GPUMaterialDataV2`(128B) 升级为 `GPUMaterialDataV3`(128B, 字段重组)
4. v2→v3 JSON 自动映射默认值，禁止崩溃

---

## 6. Clustered Forward+ 完整扩展

### 6.1 从 V3 基础到完整体

| 维度 | V3 基础 | V4 完整 |
|------|---------|---------|
| 光源上限 | 256 | **4096** |
| 分簇维度 | tile(x,y) + layer(z) | tile(x,y) + layer(z) + **类型分组** |
| 光源类型 | Point + Spot + Ambient | + **AreaLight** + **LineLight** |
| 阴影耦合 | 独立 Pass | **紧耦合**（shadow map index in light data） |
| 剔除精度 | AABB 粗筛 | AABB + **球体精筛** + **锥体精筛** |

### 6.2 Cluster 布局

```
tile = 16×16 px
zSlices = 8 (渲染层/高度带)
1920/16 × 1080/16 × 8 = 120 × 68 × 8 = 65,280 clusters
每 cluster: header(16B) + indices(64×4B=256B) = 272B
总计: ~17MB SSBO — 可接受
```

### 6.3 Light Culling Compute Shader

```glsl
layout(local_size_x = 16, local_size_y = 16) in;

shared uint s_lightCount;
shared uint s_lightIndices[MAX_LIGHTS_PER_CLUSTER];

void main() {
    ivec2 tileId = ivec2(gl_WorkGroupID.xy);
    uint  zSlice = gl_WorkGroupID.z;
    vec4 tileAABB = computeTileAABB(tileId, zSlice);

    if (gl_LocalInvocationIndex == 0) s_lightCount = 0;
    barrier();

    // 每线程检测一批光源
    uint lightsPerThread = (totalLights + 255) / 256;
    for (uint i = 0; i < lightsPerThread; ++i) {
        uint lightIdx = gl_LocalInvocationIndex * lightsPerThread + i;
        if (lightIdx >= totalLights) break;

        GPULightV2 light = lightBuffer[lightIdx];

        if (sphereAABBIntersect(light.position, light.radius, tileAABB)) {
            if (light.type == LIGHT_SPOT && !coneAABBIntersect(light, tileAABB))
                continue;
            uint idx = atomicAdd(s_lightCount, 1);
            if (idx < MAX_LIGHTS_PER_CLUSTER)
                s_lightIndices[idx] = lightIdx;
        }
    }
    barrier();

    // 写入全局 cluster 数据
    if (gl_LocalInvocationIndex == 0) {
        uint clusterIdx = tileId.y * tilesX * zSlices + tileId.x * zSlices + zSlice;
        uint writeCount = min(s_lightCount, MAX_LIGHTS_PER_CLUSTER);
        clusterHeaders[clusterIdx].offset = atomicAdd(globalOffset, writeCount);
        clusterHeaders[clusterIdx].pointCount = writeCount;
        // 溢出统计
        if (s_lightCount > MAX_LIGHTS_PER_CLUSTER)
            atomicAdd(overflowCounter, 1);
    }
}
```

### 6.4 回退策略

`clusteredLightingV4Enabled=false` 或执行失败时，回退到 V3 的 256 光源全遍历路径。

---

## 7. 2.5D 高度图光影系统

### 7.1 全局高度场

```
高度信息来源：
├── 地形 Tile → 基础高度（地图编辑器导出）
├── 静态物体 → 遮挡高度（障碍物、墙壁）
├── 动态实体 → 实时高度（Sprite 的 Mask.B 通道）
└── 合成为 R16F 全局高度纹理（按 chunk 增量更新）
```

### 7.2 光线步进阴影（Raymarching Shadow）

```glsl
float shadowRaymarch(vec2 fragWorldPos, float fragHeight,
                     vec2 lightPos, float lightHeight) {
    vec2 dir = normalize(lightPos - fragWorldPos);
    float dist = length(lightPos - fragWorldPos);
    float stepSize = 2.0;
    int   maxSteps = 64;
    float shadow = 1.0;

    for (int i = 1; i <= maxSteps; ++i) {
        float t = float(i) * stepSize;
        if (t >= dist) break;

        vec2  samplePos = fragWorldPos + dir * t;
        float sampleHeight = textureLod(heightMap, worldToUV(samplePos), 0).r;
        float rayHeight = mix(fragHeight, lightHeight, t / dist);

        if (sampleHeight > rayHeight) {
            float penumbra = (sampleHeight - rayHeight) / (lightHeight - fragHeight);
            shadow = min(shadow, 1.0 - smoothstep(0.0, 0.3, penumbra));
        }
    }
    return shadow;
}
```

### 7.3 自投影（Self-Shadow）

利用 Sprite 自身 Height Map（Mask.B）实现角色内部投影（手臂遮挡身体、武器投影到肩甲）：

```glsl
float selfShadow(vec2 uv, vec2 lightDir2D, sampler2D maskTex) {
    float height = texture(maskTex, uv).b;
    float shadow = 1.0;
    for (int i = 1; i <= 8; ++i) {
        vec2 offset = lightDir2D * float(i) * 0.003;
        float sampleH = texture(maskTex, uv + offset).b;
        if (sampleH > height + float(i) * 0.01)
            shadow *= 0.7;
    }
    return shadow;
}
```

### 7.4 视差遮挡映射（POM）

为地面 Tile 和大型场景物体添加视差效果：

```glsl
vec2 parallaxOcclusionMapping(vec2 uv, vec3 viewDir, sampler2D heightMap) {
    const int numLayers = 16;
    float layerDepth = 1.0 / float(numLayers);
    float currentDepth = 0.0;
    vec2 deltaUV = viewDir.xy / viewDir.z * heightScale / float(numLayers);
    vec2 currentUV = uv;
    float currentHeight = texture(heightMap, currentUV).b;

    for (int i = 0; i < numLayers; ++i) {
        if (currentDepth >= currentHeight) break;
        currentUV -= deltaUV;
        currentHeight = texture(heightMap, currentUV).b;
        currentDepth += layerDepth;
    }

    // 二分精修
    vec2 prevUV = currentUV + deltaUV;
    float beforeH = texture(heightMap, prevUV).b - (currentDepth - layerDepth);
    float afterH  = currentHeight - currentDepth;
    float weight  = afterH / (afterH - beforeH);
    return mix(currentUV, prevUV, weight);
}
```

### 7.5 Quality Tier 适配

| Tier | Raymarching | Self-Shadow | POM |
|------|:-----------:|:-----------:|:---:|
| Low | Off | Off | Off |
| Med | Off | Off | Off |
| High | 16 steps | Basic (4 steps) | Off |
| Ultra | 64 steps + 软阴影 | Full (8 steps) | 16 layers |

---

## 8. GPU ABI 契约（V4）

### 8.1 版本策略

`GPU_ABI_VERSION: 3 → 4`

### 8.2 新增/变更结构

| 结构 | 动作 | 变更说明 |
|------|------|----------|
| `GPUMaterialDataV3` | 替换 V2 | 字段重组 + fresnelControl/uvParams |
| `GPULightV2` | 替换 V1 | 增加 type/shadowMapIndex/priority |
| `GPUTextCommand` | 新增 | 文字渲染指令 |
| `GPUTextQuad` | 新增 | 排版输出 |
| `GPUGlyphMetrics` | 新增 | 字形度量 |
| `GPULootInstance` | 新增 | 战利品实例 |
| `GPUClusterHeaderV2` | 替换 V1 | 增加类型分组计数 |

### 8.3 强制规则

1. 所有新增/变更结构必须由 ABI 生成链路产出（`tools/render_abi/`）
2. 禁止手写重复 GLSL struct
3. `static_assert(sizeof(...))` 必须存在
4. C++ 与 GLSL 的 `GPU_ABI_VERSION` 必须匹配
5. ABI 变更必须递增版本号 + 变更日志

---

## 9. 性能预算与 Pass 级分配

### 9.1 三档目标（维持 V2/V3 不降）

| 场景 | 帧率阈值 | 帧预算 |
|------|:--------:|:------:|
| 常规战斗 | ≥ 270 FPS | ≤ 3.70ms |
| 高强度战斗 | ≥ 180 FPS | ≤ 5.56ms |
| 极限压力 | ≥ 144 FPS | ≤ 6.94ms |

### 9.2 V4 Pass 预算

| Pass | 常规 | 高压 | 极限 | 备注 |
|------|:---:|:---:|:---:|------|
| ScenePass (PBR 升级) | 1.3ms | 1.8ms | 2.2ms | Normal/Mask 采样 +~0.2ms |
| LightCullingPass (V4) | 0.20ms | 0.40ms | 0.60ms | 4096 光源 |
| ShadowPass | 0.40ms | 0.90ms | 1.30ms | 沿用 V3 |
| LightingPass (BRDF-Lite) | 0.70ms | 1.10ms | 1.40ms | PBR +~0.1ms |
| HeightShadowPass | 0.30ms | 0.60ms | 0.90ms | **新增** |
| VFXPass | 0.50ms | 0.80ms | 1.00ms | 沿用 |
| GPUTextPass | 0.05ms | 0.10ms | 0.15ms | **新增**，替代 CPU |
| GPULootPass | 0.05ms | 0.10ms | 0.20ms | **新增**，替代 CPU |
| PostProcess | 0.30ms | 0.50ms | 0.80ms | 沿用 |
| Composite | 0.10ms | 0.15ms | 0.20ms | 沿用 |
| **渲染总计** | **3.90ms** | **6.45ms** | **8.75ms** | — |

> ⚠️ 高压/极限场景超出帧预算，需要自动降级（见 §10）。

### 9.3 超预算自动降级顺序

1. HeightShadow: 64 steps → 16 steps → Off
2. Bloom Mip 层数下降
3. Distortion Off
4. 动态光源上限: 4096 → 1024 → 256
5. PBR 降级为 Albedo-only
6. Clustered 回退到全遍历
7. Self-Shadow Off
8. POM Off

---

## 10. Quality Tier 矩阵（V4）

| 能力 | Low | Medium | High | Ultra |
|------|:---:|:------:|:----:|:-----:|
| PBR 材质 | Albedo only | Albedo + Normal | Full BRDF-Lite | Full + Detail Normal |
| Clustered Lighting | Off | 256 lights (V3) | 1024 lights | 4096 lights |
| Height Shadow | Off | Off | Basic (16 steps) | Full (64 + self-shadow) |
| POM | Off | Off | Off | 16 layers |
| GPU Text | CPU fallback | GPU basic | GPU full | GPU + 动画全开 |
| GPU Loot Layout | CPU simple | CPU simple | GPU force-directed | GPU + HDR glow |

---

## 11. 分阶段实施路线

### Phase V4-A（4-6 周）：GPU 驱动子系统

| 任务 | 描述 | 依赖 |
|------|------|------|
| MSDF 工具链 | msdfgen 集成，4K 图集生成 | 无 |
| GPUTextSystem | Compute 排版 + MDI 绘制 + 动画 | MSDF 图集 |
| GPULootSystem | MDI 合批 + GPU 力导向避让 | V3 MDI 基础 |
| Feature Flag | 双路径并存验证 | — |

**验收标准**：GPU 文字/战利品渲染在 High/Ultra 下视觉与 CPU 路径一致，性能提升 ≥ 10x。

### Phase V4-B（6-8 周）：2D PBR 材质

| 任务 | 描述 | 依赖 |
|------|------|------|
| Material Schema V3 | GPUMaterialDataV3 + ABI V4 更新 | V3 ABI 链路 |
| Normal/Mask 管线 | Texture2DArray 分层 + 加载 | Schema |
| BRDF-Lite Shader | entity_mdi.frag + particle.frag 升级 | 管线 |
| 美术工具链 | Height→Normal→AO 离线生成脚本 | — |

**验收标准**：至少 3 类 Sprite（玩家/怪物/场景物体）有 Normal+Mask 贴图，光照呈现"同光异材"层次感。

### Phase V4-C（8-10 周）：高级光影

| 任务 | 描述 | 依赖 |
|------|------|------|
| Clustered Forward+ V4 | 4096 光源 + 多类型剔除 + 溢出统计 | V3 Clustered |
| HeightShadowPass | 高度图光线步进 + 自投影 | PBR 材质 |
| POM | 地面 Tile 视差遮挡 | HeightMap |
| 地图编辑器升级 | 光源放置 + 高度图烘焙 | — |

**验收标准**：同屏 1000+ 光源无漏光，高度阴影在 3 个场景类型（洞穴/森林/城镇）呈现明显差异。

### 依赖关系图

```
V3 完成
  ↓
V4-A (GPU Text + Loot)
  ↓
V4-B (PBR 材质)  ← 可与 V4-A 后半段并行
  ↓
V4-C (高级光影)
  ↓
V5 可开始（见 GPU_Rendering_System_V5.md）
```

---

## 12. 风险清单与缓解策略

| ID | 风险 | 影响 | 概率 | 缓解 |
|:---:|------|------|:---:|------|
| V4-R01 | MSDF 中文字形超出单张图集 | 文字渲染不全 | 中 | Primary/Secondary 双图集 + LRU 动态加载 |
| V4-R02 | 力导向标签避让不收敛 | 标签闪烁 | 低 | 阻尼衰减 + 最大位移限制 + 3 帧后强制锁定 |
| V4-R03 | 2D PBR 法线方向过于统一 | 全屏高光/过暗 | 中 | Roughness bias + Fresnel 抑制 + 美术指南 |
| V4-R04 | 4096 光源 Cluster 溢出 | 漏光 | 中 | 固定优先级裁剪 + 溢出日志 + 降级到 1024 |
| V4-R05 | 高度图精度不足致阴影锯齿 | 视觉瑕疵 | 中 | 软阴影 smoothstep + 采样 Mip 自适应 |
| V4-R06 | POM 采样不足致拉伸 | 视觉错误 | 低 | 自适应层数 + 视角阈值关闭 |
| V4-R07 | ABI V4 迁移导致 V3 回归 | 渲染异常 | 中 | V3→V4 兼容映射 + layout 快照测试 |

---

## 13. 验收标准（DoD）

### 13.1 功能门禁

1. GPU Text 在 Low→Ultra 全档位下输出正确（含中文、数字、特殊符号）
2. GPU Loot 在 1000+ 物品场景下标签无严重重叠
3. PBR 材质在至少 3 类 Sprite 上呈现层次差异
4. 4096 光源场景下无漏光、无 Cluster 溢出报错（允许统计计数）
5. 高度阴影在 3+ 场景类型下视觉可辨

### 13.2 性能门禁

1. 常规场景 ≥ 270 FPS（Ultra 档, 4070S@2K）
2. 高压场景 ≥ 180 FPS（允许自动降级）
3. 极限场景 ≥ 144 FPS（允许自动降级）
4. 各新增 Pass 不超过预算表 +10%

### 13.3 契约门禁

1. ABI V4 layout 测试通过
2. Binding 冲突检查通过
3. RenderGraph 合同验证通过
4. Material Schema V3 兼容 V2 JSON

### 13.4 稳定性门禁

1. 30 分钟压力运行无崩溃、无显存持续增长
2. Feature Flag 双路径切换无黑帧
3. Tier 自动降级无抖动

### 13.5 回退门禁

1. `render.v4.enabled = false` 可完整回退到 V3
2. 回退后 V3 全功能正常

---

## 14. 附录：关键数据结构

### 14.1 GPUTextCommand (16 bytes)

```cpp
struct GPUTextCommand {
    glm::vec2 worldPos;       // 8
    uint32_t  stringId;       // 4
    uint32_t  colorAndFlags;  // 4
};
static_assert(sizeof(GPUTextCommand) == 16);
```

### 14.2 GPUGlyphMetrics (40 bytes)

```cpp
struct GPUGlyphMetrics {
    glm::vec4 uvRect;         // 16
    glm::vec2 offset;         // 8
    glm::vec2 size;           // 8
    float     advance;        // 4
    float     reserved;       // 4
};
static_assert(sizeof(GPUGlyphMetrics) == 40);
```

### 14.3 GPULootInstance (32 bytes)

```cpp
struct GPULootInstance {
    glm::vec2 worldPos;       // 8
    glm::vec2 labelOffset;    // 8
    uint32_t  itemId;         // 4
    uint32_t  rarityColor;    // 4
    float     glowIntensity;  // 4
    uint32_t  flags;          // 4
};
static_assert(sizeof(GPULootInstance) == 32);
```

### 14.4 GPUMaterialDataV3 (128 bytes)

```cpp
struct alignas(16) GPUMaterialDataV3 {
    glm::vec4 baseColor;              // 16
    glm::vec4 emissiveAndIntensity;   // 16
    glm::vec4 pbrParams;              // 16  (roughness, metallic, ao, heightBias)
    glm::vec4 textureSlots;           // 16  (albedoIdx, normalIdx, maskIdx, detailIdx)
    glm::vec4 fresnelControl;         // 16  (f0Override, rimSuppress, roughnessBias, reserved)
    glm::vec4 uvParams;               // 16  (tilingScale.xy, scrollSpeed.xy)
    glm::vec4 reserved[2];            // 32
};
static_assert(sizeof(GPUMaterialDataV3) == 128);
```

### 14.5 GPULightV2 (64 bytes)

```cpp
struct alignas(16) GPULightV2 {
    glm::vec2 position;         // 8
    float     radius;           // 4
    float     intensity;        // 4
    glm::vec4 color;            // 16
    glm::vec4 spotParams;       // 16  (direction.xy, innerCone, outerCone)
    uint32_t  type;             // 4
    uint32_t  shadowMapIndex;   // 4
    uint32_t  priority;         // 4
    uint32_t  flags;            // 4
};
static_assert(sizeof(GPULightV2) == 64);
```

### 14.6 GPUClusterHeaderV2 (16 bytes)

```cpp
struct GPUClusterHeaderV2 {
    uint32_t offset;        // 光源索引列表起始偏移
    uint32_t pointCount;    // 点光源数量
    uint32_t spotCount;     // 聚光灯数量
    uint32_t areaCount;     // 面光源数量
};
static_assert(sizeof(GPUClusterHeaderV2) == 16);
```

---

## 15. 外部参考

### MSDF 字体渲染
1. msdfgen — Multi-channel SDF generator: <https://github.com/Chlumsky/msdfgen>
2. msdf-atlas-gen — Atlas generator: <https://github.com/Chlumsky/msdf-atlas-gen>

### GPU 驱动渲染
3. Vulkan Guide — GPU Driven Rendering: <https://vkguide.dev/docs/gpudriven/gpu_driven_rendering/>
4. PowerVR — GPU-Driven Rendering: <https://www.imgtec.com/blog/gpu-driven-rendering/>

### Clustered Forward+
5. SIGGRAPH 2012 — Tiled and Clustered Forward Shading: <https://research.chalmers.se/publication/161726>
6. Wicked Engine — Clustered Forward: <https://wickedengine.net/2018/01/10/tiled-and-clustered-forward-rendering/>

### 2D PBR
7. LearnOpenGL — PBR Theory: <https://learnopengl.com/PBR/Theory>
8. LearnOpenGL — Normal Mapping: <https://learnopengl.com/Advanced-Lighting/Normal-Mapping>
9. ivan-resetnikov — 2D PBR OpenGL Demo: <https://github.com/ivan-resetnikov/demo-2D-PBR-renderer-opengl>

### Compute Shaders
10. LearnOpenGL — Compute Shaders: <https://learnopengl.com/Guest-Articles/2022/Compute-Shaders/Introduction>

---

> **修订说明（2026-02-15）**  
> 本文档为 V4 首版设计规格书，从 `GPU_Rendering_System_V4_V5.md` 拆分而来。  
> V4 定位为 V3 完成后的**画质与效率全面升级**，可独立交付与验收。  
> 后续 V5（全局光照）见 `GPU_Rendering_System_V5.md`。
