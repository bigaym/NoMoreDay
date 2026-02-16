# GPU 渲染系统 3.0 — 完整设计规格书

> **文档版本**: 3.0  
> **创建日期**: 2026-02-15  
> **定位**: GPU Rendering System 2.0 的迭代升级规范，作为后续 V3 实施、验收与发布门禁的统一基线。  
> **基线来源**: `GPU_Rendering_System_2.md` + 2026-02 渲染 Track 收尾结果（RenderGraph Contract、GPU ABI 治理、Tier 自动降档、VFX 材质补完）。

---

## 目录

1. [项目背景与目标](#1-项目背景与目标)
2. [V2 现状与 V3 关键缺口](#2-v2-现状与-v3-关键缺口)
3. [V3 核心设计决策（锁定）](#3-v3-核心设计决策锁定)
4. [总体架构：RenderGraph V3](#4-总体架构rendergraph-v3)
5. [阴影系统：2.5D Hybrid Shadow](#5-阴影系统25d-hybrid-shadow)
6. [多光源扩展：Clustered 2D Lighting](#6-多光源扩展clustered-2d-lighting)
7. [材质光照深度：Material 2.0](#7-材质光照深度material-20)
8. [VFX 与光影联动：内容生产层](#8-vfx-与光影联动内容生产层)
9. [Quality Tier 与自动降级顺序](#9-quality-tier-与自动降级顺序)
10. [GPU ABI 契约（V3）](#10-gpu-abi-契约v3)
11. [Binding Registry 与 Pass 命名域](#11-binding-registry-与-pass-命名域)
12. [Frame Ownership 与目标生命周期](#12-frame-ownership-与目标生命周期)
13. [GL 状态与同步契约](#13-gl-状态与同步契约)
14. [资产 Schema、兼容与热重载规则](#14-资产-schema兼容与热重载规则)
15. [性能预算与门槛](#15-性能预算与门槛)
16. [验证、发布门禁与回退策略](#16-验证发布门禁与回退策略)
17. [分阶段实施路线（Step A-F）](#17-分阶段实施路线step-a-f)
18. [Conductor Track 拆分与交付要求](#18-conductor-track-拆分与交付要求)
19. [风险清单与缓解策略](#19-风险清单与缓解策略)
20. [验收标准（DoD）](#20-验收标准dod)
21. [附录：关键数据结构（建议形态）](#21-附录关键数据结构建议形态)
22. [外部参考（一手资料）](#22-外部参考一手资料)

---

## 1. 项目背景与目标

### 1.1 背景

V2 已完成 HDR、后处理、动态光照、体积光、扭曲、材质、VFX 序列器和 Tier 自动降级等核心能力，渲染系统已从单体渲染函数演进为契约化 RenderGraph 管线。

### 1.2 V3 目标定义

在 **不切换图形 API（OpenGL 4.3+）** 的前提下，将 Ultra 档观感提升到“2D 视角下接近 D4/PoE2 氛围层次”的工程水平，核心是协同补齐：

1. 阴影体系（SDF + Hybrid）
2. 材质受光深度（Normal/Roughness/Specular）
3. 多光源可扩展性（Clustered Culling）
4. 内容化生产效率（VFX 与光影参数联动）

### 1.3 非目标

1. 不迁移到 Vulkan/Metal。
2. 不引入重量级第三方渲染框架替换现有引擎。
3. 不追求 3D 全 PBR 复刻，仅追求 2D ARPG 视觉上限。

---

## 2. V2 现状与 V3 关键缺口

### 2.1 已具备能力

1. RenderGraph 合同验证已落地。
2. GPU ABI/Biding 治理链路已落地。
3. Tier 自动降级有验证基线。
4. VFX 与材质主流程可用并通过近期回归。

### 2.2 关键缺口

1. `shadowResolution/shadow` 相关配置存在，但阴影管线未完整成型。
2. 光照模型仍偏简化，多光源扩展性在高压场景下风险较高。
3. 材质受光深度不足，难以形成“同光异材”的层次感。
4. VFX 对光影/材质的时间线控制能力不足，内容生产效率受限。

---

## 3. V3 核心设计决策（锁定）

1. **架构延续**: 保持单主 RenderGraph，不新增并行主渲染管线。  
2. **性能红线**: 三档目标不降低（270 / 180 / 144 FPS）。  
3. **契约优先**: ABI、Binding、Frame Ownership、Schema 全部强校验。  
4. **兼容策略**: 资产允许 N/N-1，关键契约变更必须版本递增。  
5. **回退兜底**: V3 全程可 feature-flag 回退到 V2。  
6. **内容优先级**: 参数前置到材质/VFX schema，减少 shader 硬编码分叉。

---

## 4. 总体架构：RenderGraph V3

### 4.1 锁定 Pass 顺序

`Scene -> LightCulling -> Shadow -> Lighting -> Volumetric -> VFX -> UIWorld -> PostProcess -> Distortion -> Composite`

### 4.2 阶段职责

1. `Scene`: 实体/地表/基础世界内容渲染到 HDR。
2. `LightCulling`: 计算每 cluster 光源列表（可关闭并回退）。
3. `Shadow`: SDF/Atlas 组合阴影构建与解析。
4. `Lighting`: 结合 shadowFactor 与 BRDF-lite 进行主光照。
5. `Volumetric`: 体积光散射与能见度塑形。
6. `VFX`: 特效层并可驱动部分光影参数。
7. `UIWorld`: 世界空间 UI 与战斗提示层。
8. `PostProcess`: Bloom/Tonemap/Color grade 等。
9. `Distortion`: 热浪/冲击等屏幕扭曲。
10. `Composite`: 唯一屏幕输出阶段。

### 4.3 主体约束

1. 最终合成前，不允许硬编码输出到 `FBO 0`。
2. 所有资源读写由 RenderGraph 显式声明。
3. 离屏与默认帧缓冲路径必须等价可验证。

---

## 5. 阴影系统：2.5D Hybrid Shadow

### 5.1 档位策略

1. `Medium`: 阴影关闭。
2. `High`: SDF Shadow。
3. `Ultra`: Hybrid（关键光 Atlas + 非关键光 SDF）。

### 5.2 新增 Pass

1. `ShadowPreparePass`
2. `ShadowBuildPass`
3. `ShadowResolvePass`

### 5.3 关键实现要点

1. 静态遮挡体按 chunk 缓存。
2. 动态遮挡体仅更新 camera 邻域。
3. Atlas 仅分配给 top N 关键光（`priority + screen influence`）。
4. Atlas 溢出采用确定性淘汰并记录计数日志。
5. 任一 Shadow Pass 失败时，自动回退到 V2 Lighting 路径。

### 5.4 光照整合

`light_accumulation` 主路径升级为：

`attenuation * shadowFactor * BRDF-lite`

---

## 6. 多光源扩展：Clustered 2D Lighting

### 6.1 算法形态

1. Cluster 维度：`tile(x,y) + layer(z)`。
2. `z` 来自渲染层/高度带，不依赖真实 3D 深度。

### 6.2 Pass 设计

1. `LightCullingPass`（Compute）：构建 cluster -> light list。
2. `LightingPass`：按当前像素 cluster 查询局部光列表。

### 6.3 裁剪策略

1. AABB 粗筛。
2. 半径/锥体精筛。
3. `maxLightsPerCluster` 固定上限，溢出按优先级裁剪并计数。

### 6.4 回退策略

`clusteredLightingEnabled=false` 或执行失败时，回退到 V2 全光遍历路径。

---

## 7. 材质光照深度：Material 2.0

### 7.1 Schema 升级

`material_schema_version: 2`

### 7.2 新增字段

1. `normalMapSlot`
2. `roughness`
3. `specular`
4. `ao`
5. `heightBias`
6. `detailNormalScale`

### 7.3 GPU 结构

`GPUMaterialDataV2` 采用 128B 对齐，统一由 ABI 生成链路治理。

### 7.4 Shader 策略

1. `entity_mdi.frag` 与 `particle.frag` 接入 BRDF-lite 分支。
2. 点光/聚光统一采用 `N·L` 与 half-vector。
3. 保持 non-PBR 美术参数风格，但计算过程物理可解释。

### 7.5 资源与降级

1. Normal/Roughness 使用 Texture2DArray 分层。
2. Low/Medium 自动关闭高阶材质分支。
3. v1 材质 JSON 自动映射默认值，禁止崩溃。

---

## 8. VFX 与光影联动：内容生产层

### 8.1 Schema 升级

`vfx_schema_version: 3`

### 8.2 新事件类型

1. `ShadowPulse`
2. `LightProfileBlend`
3. `MaterialPhaseShift`

### 8.3 事件执行策略

每事件支持 `tierPolicy`：

1. `strict`
2. `degrade`
3. `skip`

失败必须结构化日志，不允许 silent ignore。

### 8.4 工具链要求

1. VFX 预览场景（时间线可视化 + 热重载 diff）。
2. 序列预算估计器（粒子/灯光/阴影成本预估）。
3. 至少 12 个 V3 模板序列交付并覆盖降级行为。

---

## 9. Quality Tier 与自动降级顺序

### 9.1 档位能力矩阵（核心项）

| 能力 | Low | Medium | High | Ultra |
|---|---:|---:|---:|---:|
| Shadow | Off | Off | SDF | Hybrid |
| Clustered Lighting | Off | Optional | On | On |
| Material 2.0 高阶分支 | Off | Off | Partial | Full |
| Volumetric 高质量 | Off | Basic | On | Full |
| Distortion | Off | Off/Basic | On | On |

### 9.2 推荐自动降级顺序

1. 降低 Bloom 级别。
2. 关闭 Distortion。
3. 限制动态光数量。
4. 关闭 Clustered 高压参数。
5. Hybrid 阴影降级为 SDF。
6. 关闭高阶材质分支。

---

## 10. GPU ABI 契约（V3）

### 10.1 版本策略

`GPU_ABI_VERSION: 2 -> 3`

### 10.2 强制规则

1. C++ 结构与 GLSL 结构由同一生成链路产出。
2. 禁止手写重复 GLSL struct（绕过生成链路）。
3. 所有结构变化必须同步快照与 layout 测试。
4. ABI 不匹配必须显式错误并阻断继续。

---

## 11. Binding Registry 与 Pass 命名域

### 11.1 全局域

用于长期驻留资源，集中在 `RenderConstants` 管理。

### 11.2 Pass 局部域

1. 每个 Pass 局部 binding 独立且不可跨 Pass 假设复用。
2. 禁止字面量 binding（如硬写 `BindBase(4)`）。
3. 冲突检查纳入 CI 门禁。

---

## 12. Frame Ownership 与目标生命周期

### 12.1 单帧所有权规则

1. `Scene` 写 `HDRScene`
2. `Shadow/Lighting` 读写照明链路中间目标
3. `PostProcess` 写 `LDR` 链路
4. `Composite` 才允许写最终屏幕目标

### 12.2 资源生命周期

1. 持久资源：随 resize 安全重建。
2. 临时资源：走 pool 申请与回收。
3. 禁止未声明资源读写。

---

## 13. GL 状态与同步契约

### 13.1 Pass 边界契约

每个 Pass 必须在进入/退出时满足基线状态约束，避免状态泄漏。

### 13.2 关键同步点

Compute -> Fragment 的依赖路径必须显式插入 `glMemoryBarrier` 对应屏障位，尤其是：

1. `LightCullingPass` 输出 -> `LightingPass` 读取
2. `ShadowBuildPass` 输出 -> `ShadowResolve/Lighting` 读取
3. VFX/Distortion 读写图像依赖

### 13.3 rlgl 互操作规范

进入自定义 GL 阶段前统一 flush，离开时恢复基线状态，防止 Raylib batch 干扰。

---

## 14. 资产 Schema、兼容与热重载规则

### 14.1 必须版本化的资产

1. `material_schema_version`（升级到 2）
2. `vfx_schema_version`（升级到 3）
3. 相关纹理数组清单版本

### 14.2 兼容策略

1. 允许 N/N-1。
2. N-2 及更老版本应给出明确错误或迁移提示。
3. 非法字段和缺失字段必须可诊断，不得无声失败。

### 14.3 热重载安全

采用“新资源验证通过再原子替换”的双缓冲句柄策略。

---

## 15. 性能预算与门槛

### 15.1 三档目标（维持不降）

1. 常规场景：`>= 270 FPS`（<= 3.70ms）
2. 高压场景：`>= 180 FPS`（<= 5.56ms）
3. 极限场景：`>= 144 FPS`（<= 6.94ms）

### 15.2 V3 Pass 预算（新增项）

| Pass | 常规 | 高压 | 极限 |
|---|---:|---:|---:|
| LightCullingPass | 0.15ms | 0.30ms | 0.45ms |
| Shadow\* | 0.40ms | 0.90ms | 1.30ms |
| LightingPass | 0.60ms | 1.00ms | 1.30ms |

### 15.3 专项目标

1. `>=128 lights` 场景下，Clustered Lighting 使 Lighting 平均耗时下降 `>=25%`。
2. High 档材质新增开销 `<=0.6ms`。
3. High/Ultra 阴影新增开销分别 `<=0.8ms / <=1.3ms`。

---

## 16. 验证、发布门禁与回退策略

### 16.1 功能门禁

1. 默认/离屏/多分辨率路径全部通过图形回归。
2. Resize + Alt+Tab + 热重载路径稳定。

### 16.2 契约门禁

1. ABI layout 与版本通过。
2. Binding 冲突检查通过。
3. RenderGraph 合同验证通过。
4. Schema 版本校验通过。

### 16.3 稳定性门禁

30 分钟压力运行无显存持续增长、无 context restore 黑屏。

### 16.4 发布与回退

1. 灰度开关：`render.v3.enabled`。
2. 若出现黑屏或性能回归 `>10%`：自动回退 V2 并阻断合并。

---

## 17. 分阶段实施路线（Step A-F）

### Step A（第 1 周）：V3 基线与契约

1. 扩展 `RenderConfig`、`GPUData`、ABI 工具链。
2. 锁定 Pass 顺序与预算表。
3. 验收：仅合同与框架落地，不改变视觉输出。

### Step B（第 2-4 周）：阴影系统

1. 实现 `ShadowPrepare/Build/Resolve`。
2. High SDF 与 Ultra Hybrid 分档落地。
3. 完成失败回退与日志计数。

### Step C（第 3-5 周）：Clustered Lighting

1. 实现 `LightCullingPass`。
2. Lighting 读取 cluster list。
3. 完成溢出裁剪统计与回退路径。

### Step D（第 4-6 周）：Material 2.0

1. schema v2 与 `GPUMaterialDataV2` 落地。
2. BRDF-lite 接入实体与粒子 shader。
3. 完成 v1->v2 兼容映射。

### Step E（第 6-8 周）：VFX 联动

1. schema v3 与 3 类新事件落地。
2. 加入 tierPolicy 与失败日志策略。
3. 交付模板序列与预算估计工具。

### Step F（第 8-10 周）：全链路验收与发布门禁

1. 功能/性能/稳定性/契约全门禁闭环。
2. 发布开关灰度与异常回退演练。
3. 完成 V3 发布判定。

---

## 18. Conductor Track 拆分与交付要求

### 18.1 Track 列表

1. `v3_baseline_contracts_YYYYMMDD`（Step A：V3 基线与契约）
2. `v3_shadow_pipeline_YYYYMMDD`（Step B）
3. `v3_clustered_lighting_YYYYMMDD`（Step C）
4. `v3_material_lighting_depth_YYYYMMDD`（Step D）
5. `v3_vfx_lighting_integration_YYYYMMDD`（Step E）
6. `v3_validation_and_release_gate_YYYYMMDD`（Step F）

### 18.2 每个 Track 必备产物

1. `spec.md`
2. `plan.md`
3. `validation.md`
4. `metadata.json`
5. `index.md`

---

## 19. 风险清单与缓解策略

| 风险 | 影响 | 缓解 |
|---|---|---|
| Shadow Atlas 溢出抖动 | 阴影闪烁 | 确定性淘汰 + 滞回策略 + 日志计数 |
| Cluster 溢出导致漏光 | 视觉错误 | 固定裁剪优先级 + 溢出统计 + 回归用例 |
| ABI 偏移错位 | 难排查渲染异常 | 生成链路唯一化 + CI layout 快照 |
| Tier 降级抖动 | 帧时间波动 | 降级冷却时间 + 恢复阈值滞回 |
| 热重载中断 | 黑屏/资源错乱 | 双缓冲句柄 + 验证后替换 |

---

## 20. 验收标准（DoD）

1. 代码与资产契约已落地并通过构建。
2. `build.bat`、`build.bat analyze`、`build.bat perf` 通过。
3. Low/Medium/High/Ultra 全矩阵验证通过。
4. 三档性能阈值与新增 Pass 预算达标。
5. 关键场景视觉回归通过（截图差异阈值受控）。
6. V3 异常可自动回退 V2，且有可追踪日志证据。

---

## 21. 附录：关键数据结构（建议形态）

### 21.1 RenderConfig（新增字段）

```cpp
enum class ShadowMode : uint8_t { Off = 0, SDF = 1, Hybrid = 2 };

struct RenderConfig {
    bool shadowEnabled;
    ShadowMode shadowMode;
    uint32_t maxShadowedLights;
    uint32_t shadowAtlasSize;
    float shadowSoftness;

    bool clusteredLightingEnabled;
    uint32_t clusterTileSize;
    uint32_t clusterZSliceCount;

    bool normalLightingEnabled;
    bool specularEnabled;
    uint32_t materialQualityLevel;
};
```

### 21.2 阴影结构

```cpp
struct GPUShadowCaster {
    glm::vec2 position;
    float radius;
    float occluderHeight;
    uint32_t shapeIndex;
    uint32_t dynamicFlag;
    uint32_t reserved0;
    uint32_t reserved1;
};

struct GPUShadowLight {
    uint32_t lightId;
    uint32_t shadowType;
    glm::vec4 atlasRect;
    glm::vec4 penumbraParams;
};

struct GPUShadowAtlasMeta {
    uint32_t tileIndex;
    uint32_t lastUsedFrame;
    float priorityScore;
    float occupancy;
};
```

### 21.3 Cluster 结构

```cpp
struct GPUClusterHeader {
    uint32_t offset;
    uint32_t count;
    uint32_t overflowCount;
    uint32_t reserved;
};

struct GPUClusterLightIndex {
    uint32_t lightIndex;
};
```

### 21.4 材质结构（V2）

```cpp
struct alignas(16) GPUMaterialDataV2 {
    glm::vec4 baseColor;
    glm::vec4 emissiveAndIntensity;
    glm::vec4 pbrLite;      // roughness, specular, ao, heightBias
    glm::vec4 textureSlots; // albedo, normal, roughness, reserved
    glm::vec4 detailParams; // detailNormalScale + reserved
    glm::vec4 reserved[3];
};
```

---

## 22. 外部参考（一手资料）

1. NVIDIA GPU Gems 3: Volumetric Light Scattering as a Post-Process  
   <https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-13-volumetric-light-scattering-post-process>
2. Khronos/OpenGL: `glMemoryBarrier`  
   <https://docs.gl/gl4/glMemoryBarrier>
3. Khronos/OpenGL Wiki: SSBO / Memory Model  
   <https://wikis.khronos.org/opengl/Shader_Storage_Buffer_Object>  
   <https://wikis.khronos.org/opengl/Memory_Model>
4. SIGGRAPH 2012: Tiled and Clustered Forward Shading  
   <https://research.chalmers.se/publication/161726>
5. Blizzard 官方图形技术文章（Diablo IV）  
   <https://news.blizzard.com/en-us/article/23964183/peeling-back-the-varnish-the-graphics-of-diablo-iv>
6. GGG 官方论坛（PoE2 Rendering 入口）  
   <https://www.pathofexile.com/forum/view-thread/3447463>

> 说明：文中对 D4/PoE2 的工程策略提炼，基于公开资料推断，不涉及私有实现复刻。

---

> **修订说明（2026-02-15）**  
> 本文档为 V3 首版，定位为 V2 的增量升级规范。后续每个 V3 Track 完成后，需同步回填本文件中的“预算、门禁、兼容性与风险”章节，保持其作为发布前唯一准入基线。
