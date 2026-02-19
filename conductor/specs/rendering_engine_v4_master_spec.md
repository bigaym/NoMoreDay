# 渲染引擎 V4 主控技术规格书

> **文档版本**: 1.0  
> **创建日期**: 2026-02-19  
> **定位**: 统领所有 V4 阶段 Conductor Tracks 的**主控规格书**  
> **上游设计**: [`GPU_Rendering_System_V4.md`](../../设计文档/特效和UI/GPU_Rendering_System_V4.md)  
> **基线前提**: V3 全部 Tracks 已完成（172/172 tasks），`v3_validation_and_release_gate_20260215` 已归档

---

## 1. 文档关系图

```
GPU_Rendering_System_3.md (V3 设计 - 已完成)
    ↓
GPU_Rendering_System_V4.md (V4 设计文档 — 设计基线)
    ↓
rendering_engine_v4_master_spec.md ← 本文档（主控规格书 — 实施基线）
    ├── v4_preflight_v3_closure_20260219 (Track 0: V3 债务闭环)
    ├── v4_gpu_text_rendering_20260219   (Track 1: GPU 文字渲染)
    ├── v4_gpu_loot_rendering_20260219   (Track 2: GPU 战利品渲染)
    ├── v4_pbr_material_pipeline_20260219(Track 3: 2D PBR 材质)
    ├── v4_advanced_lighting_20260219    (Track 4: 高级光影)
    └── v4_validation_release_gate_20260219 (Track 5: V4 验收门禁)
```

---

## 2. V4 愿景与核心命题

> **从"渲染系统"到"渲染引擎"的蜕变**: V3 完成了契约化 RenderGraph、Clustered Lighting 基础、Material 2.0 和 VFX 联动，奠定了"系统"级别的工程基线。V4 的目标是在此基础上完成三个维度的跃升：
>
> 1. **GPU 全驱动**：文字和战利品渲染从 CPU 瓶颈迁移到 GPU，消灭最后的 CPU 热路径
> 2. **材质质感革命**：2D PBR 材质标准让光影呈现"同光异材"的真实层次
> 3. **光影规模突破**：4096 动态光源 + 高度图阴影，每个火花都能照亮周围物体

---

## 3. V4 实施前置依赖（来自 V3 遗留）

| 依赖 ID | 来源 | 描述 | 解除条件 | 对应 Track |
|---|---|---|---|---|
| `DEP-V3-F4.6` | F4.6 perf_clustered_uplift | Clustered 128-light improvement < 5.0% | 移除豁免，连续 3 次 release perf 满足 ≥5.0% | `v4_preflight_v3_closure_20260219` |
| `DEP-V3-F6.2` | F6.2 screenshot_compare | 截图基线未就绪 | 补齐 6 个关键场景 baseline/candidate | `v4_preflight_v3_closure_20260219` |

---

## 4. Track 依赖关系图

```
Track 0: v4_preflight_v3_closure (V3 债务闭环)
    │
    ├──→ Track 1: v4_gpu_text_rendering    ─┐
    │                                        │
    ├──→ Track 2: v4_gpu_loot_rendering    ─┤ (V4-A 可并行)
    │                                        │
    │    ┌───────────────────────────────────┘
    │    ↓
    ├──→ Track 3: v4_pbr_material_pipeline  (V4-B, 依赖 V4-A 完成 ABI V4)
    │        │
    │        ↓
    ├──→ Track 4: v4_advanced_lighting      (V4-C, 依赖 V4-B PBR 材质)
    │        │
    │        ↓
    └──→ Track 5: v4_validation_release_gate (依赖 ALL V4 feature tracks)
```

---

## 5. 各 Track 概要

### Track 0: V4 Pre-flight — V3 Debt Closure

**ID**: `v4_preflight_v3_closure_20260219`  
**类型**: quality/chore  
**优先级**: P0  
**预计周期**: 0.5-1 周  
**任务估算**: ~8 tasks

**核心目标**:
1. 验证 `DEP-V3-F4.6` Clustered 性能豁免状态
2. 补齐 `DEP-V3-F6.2` 截图基线
3. 执行一次完整的 `build.bat gate` 产出 V3 最终 release posture
4. 确认 V3 全部风险项（R-V3-001~005）状态
5. 为 V4 编码开绿灯

---

### Track 1: GPU Text Rendering (MSDF)

**ID**: `v4_gpu_text_rendering_20260219`  
**类型**: feature  
**优先级**: P0  
**预计周期**: 2-3 周  
**任务估算**: ~20 tasks

**核心子系统**:
- MSDF 字体图集生成（4096² atlas, msdfgen 离线工具）
- 字形度量缓冲区（`GPUGlyphMetrics` SSBO）
- Compute Shader 排版管线（前缀和 + 原子计数）
- MDI Quad 绘制（`GPUTextQuad` → `glDrawArraysIndirect`）
- 动画系统（飘字/重力/淡出/缩放弹跳/暴击放大）
- Feature Flag `render.gpuText.enabled` 双路径

**ABI 新增**: `GPUTextCommand` (16B), `GPUGlyphMetrics` (40B), `GPUTextQuad` (40B)

**验收标准**:
- GPU 文字在 High/Ultra 下视觉与 CPU `PopupRenderer` 一致
- 暴击连锁（100+ 同屏数字）性能提升 ≥ 10x vs CPU 路径
- Low/Med 自动回退 CPU 路径无异常

---

### Track 2: GPU Loot Rendering

**ID**: `v4_gpu_loot_rendering_20260219`  
**类型**: feature  
**优先级**: P0  
**预计周期**: 2-3 周  
**任务估算**: ~18 tasks

**核心子系统**:
- MDI 自动合批管线（FrustumCullCS → IndirectArgsCS → MultiDrawIndirect）
- GPU 力导向标签避让（GridHashCS → RepulsionCS → PositionUpdateCS）
- 空间网格划分（32×32 px Cell, 原子写入）
- Feature Flag `render.gpuLoot.enabled` 双路径

**ABI 新增**: `GPULootInstance` (32B), `DrawElementsIndirectCommand`

**验收标准**:
- 同屏 1000+ 战利品标签无严重重叠
- GPU 路径 vs CPU 路径性能提升 ≥ 5x
- LootPass 帧预算 ≤ 0.20ms (极限场景)

---

### Track 3: 2D PBR Material Pipeline

**ID**: `v4_pbr_material_pipeline_20260219`  
**类型**: feature  
**优先级**: P1  
**预计周期**: 3-4 周  
**任务估算**: ~25 tasks

**核心子系统**:
- Material Schema V3（`GPUMaterialDataV3` 128B, 字段重组）
- 四层贴图规范（Albedo/Normal/Mask/Detail）
- BRDF-Lite Shader（GGX NDF, Schlick-GGX Geometry, Schlick Fresnel + 2D 边缘抑制）
- Texture2DArray 分层管理（Albedo层 + Normal层 + Mask层）
- 美术资产工具链（Height→Normal→AO 离线生成）
- V2→V3 Material JSON 兼容映射

**ABI 变更**: `GPUMaterialDataV2` → `GPUMaterialDataV3` (128B 字段重组)

**验收标准**:
- 至少 3 类 Sprite（玩家/怪物/场景物体）有 Normal+Mask 贴图
- "同光异材"层次感可辨（金属 vs 布料 vs 石材）
- V2 材质 JSON 自动映射默认值，不崩溃

---

### Track 4: Advanced Lighting (Clustered Forward+ V4 + HeightShadow)

**ID**: `v4_advanced_lighting_20260219`  
**类型**: feature  
**优先级**: P1  
**预计周期**: 3-4 周  
**任务估算**: ~28 tasks

**核心子系统**:
- Clustered Forward+ V4（4096 光源 + 多类型剔除 + AreaLight/LineLight）
- Cluster 布局（16×16 tile, 8 zSlices, ~65K clusters）
- HeightShadowPass（Raymarching 64 steps + Self-Shadow）
- 视差遮挡映射 POM（16 layers, Ultra 专属）
- 全局高度场（R16F HeightMap, chunk 增量更新）
- 超预算自动降级链

**ABI 变更**: `GPULightV2` (64B), `GPUClusterHeaderV2` (16B)

**验收标准**:
- 同屏 **4096** 动态光源场景无漏光（Ultra），1024 光源场景无漏光（High）；Cluster 溢出统计计数允许，但无视觉漏光
- 高度阴影在 **3** 个场景类型（洞穴/森林/城镇）呈现明显差异，高度差异 ≥ 0.5m 的物体阴影可辨
- HeightShadowPass ≤ **0.90ms** (极限场景, Ultra 64 steps)
- POM 在 Ultra 档地面 Tile 视觉错位感可辨（参照对比截图）

---

### Track 5: V4 Validation & Release Gate

**ID**: `v4_validation_release_gate_20260219`  
**类型**: quality  
**优先级**: P0  
**预计周期**: 2-3 周  
**任务估算**: ~30 tasks

**核心验证维度**:
1. **功能门禁**: GPU Text/Loot 全档位正确、PBR 层次差异、4096 光源无漏光、高度阴影可辨
2. **性能门禁**: 三档帧率达标、各新增 Pass 不超预算 +10%
3. **契约门禁**: ABI V4 layout 测试、Binding 冲突检查、RenderGraph 合同验证、Schema V3 兼容
4. **稳定性门禁**: 30 分钟压力运行、Feature Flag 切换无黑帧、Tier 降级无抖动
5. **回退门禁**: `render.v4.enabled=false` 可完整回退 V3

---

## 6. ABI 迁移路径

```
V3 (GPU_ABI_VERSION = 3)
    ↓ Track 0: preflight 确认 V3 ABI 健康
    ↓ Track 1/2: 新增 Text/Loot 结构（不破坏 V3 现有结构）
    ↓ Track 3: GPUMaterialDataV2 → GPUMaterialDataV3（破坏性变更，需 v2→v3 映射）
    ↓ Track 4: GPULightV1 → GPULightV2, ClusterHeaderV1 → V2
V4 (GPU_ABI_VERSION = 4)
```

**关键原则**:
1. ABI 升级在 Track 3（PBR Material）锁定版本号
2. Track 1/2 新增结构不递增 ABI 版本（纯新增，向后兼容）
3. Track 4 光源结构变更与 Track 3 同属 ABI V4，一次性递增
4. `static_assert(sizeof(...))` 覆盖所有新增/变更结构

---

## 7. 性能预算汇总

| Pass | 常规 (270FPS) | 高压 (180FPS) | 极限 (144FPS) | 来源 Track |
|------|:---:|:---:|:---:|---|
| ScenePass (PBR 升级) | 1.3ms | 1.8ms | 2.2ms | Track 3 |
| LightCullingPass (V4) | 0.20ms | 0.40ms | 0.60ms | Track 4 |
| ShadowPass | 0.40ms | 0.90ms | 1.30ms | 沿用 V3 |
| LightingPass (BRDF-Lite) | 0.70ms | 1.10ms | 1.40ms | Track 3+4 |
| HeightShadowPass | 0.30ms | 0.60ms | 0.90ms | Track 4 |
| VFXPass | 0.50ms | 0.80ms | 1.00ms | 沿用 V3 |
| GPUTextPass | 0.05ms | 0.10ms | 0.15ms | Track 1 |
| GPULootPass | 0.05ms | 0.10ms | 0.20ms | Track 2 |
| PostProcess | 0.30ms | 0.50ms | 0.80ms | 沿用 V3 |
| Composite | 0.10ms | 0.15ms | 0.20ms | 沿用 V3 |
| **渲染总计** | **3.90ms** | **6.45ms** | **8.75ms** | — |

> ⚠️ 高压/极限场景超出帧预算，需要自动降级（见 §9）。

---

## 8. Quality Tier 矩阵（V4 完整）

| 能力 | Low | Medium | High | Ultra |
|------|:---:|:------:|:----:|:-----:|
| PBR 材质 | Albedo only | Albedo + Normal | Full BRDF-Lite | Full + Detail Normal |
| Clustered Lighting | Off | 256 lights (V3) | 1024 lights | 4096 lights |
| Height Shadow | Off | Off | Basic (16 steps) | Full (64 + self-shadow) |
| POM | Off | Off | Off | 16 layers |
| GPU Text | CPU fallback | GPU basic | GPU full | GPU + 动画全开 |
| GPU Loot Layout | CPU simple | CPU simple | GPU force-directed | GPU + HDR glow |

---

## 9. 超预算自动降级顺序

1. HeightShadow: 64 steps → 16 steps → Off
2. Bloom Mip 层数下降
3. Distortion Off
4. 动态光源上限: 4096 → 1024 → 256
5. PBR 降级为 Albedo-only
6. Clustered 回退到全遍历
7. Self-Shadow Off
8. POM Off

---

## 10. 风险矩阵

| ID | 风险 | 影响 | 概率 | 缓解 | 监控 Track |
|:---:|------|------|:---:|------|---|
| V4-R01 | MSDF 中文字形超出单张图集 | 文字渲染不全 | 中 | Primary/Secondary 双图集 + LRU | Track 1 |
| V4-R02 | 力导向标签避让不收敛 | 标签闪烁 | 低 | 阻尼衰减 + 3 帧强制锁定 | Track 2 |
| V4-R03 | 2D PBR 法线方向过于统一 | 全屏高光/过暗 | 中 | Roughness bias + Fresnel 抑制 | Track 3 |
| V4-R04 | 4096 光源 Cluster 溢出 | 漏光 | 中 | 固定优先级裁剪 + 降级到 1024 | Track 4 |
| V4-R05 | 高度图精度不足致阴影锯齿 | 视觉瑕疵 | 中 | 软阴影 smoothstep + Mip 自适应 | Track 4 |
| V4-R06 | POM 采样不足致拉伸 | 视觉错误 | 低 | 自适应层数 + 视角阈值关闭 | Track 4 |
| V4-R07 | ABI V4 迁移导致 V3 回归 | 渲染异常 | 中 | V3→V4 兼容映射 + layout 快照 | Track 5 |

---

## 11. 里程碑时间线

```
Week 0-1:  Track 0 (V3 Debt Closure) ─────────────────────────┐
Week 1-3:  Track 1 (GPU Text)  ─────────────┐                 │
Week 1-3:  Track 2 (GPU Loot)  ─────────────┤ (并行)          │
Week 3-6:  Track 3 (PBR Material) ──────────┤                 │
Week 6-9:  Track 4 (Advanced Lighting) ─────┤                 │
Week 9-11: Track 5 (V4 Validation Gate) ────┘                 │
                                              ↓                │
                              V4 里程碑完成 ← ──────────────────┘
                                              ↓
                              V5 可启动（见 V5 Master Spec）
```

---

## 12. 与 V5 的衔接

V4 完成后，引擎将具备 V5 所需的全部前置条件：
- PBR 材质管线（Emission 通道 → GI 发光体标识）
- 全局高度场（遮挡体提取依赖）
- Clustered Forward+ V4（GI 与直接光照叠加合成）
- RenderGraph V4 Pass 序列（GI Pass 插入位置）
- ABI V4 生成链路（V5 结构体继承 V4 治理）

V5 可在 V4-B 完成后提前启动 JFA 预研（距离场不依赖高级光影）。

---

> **修订说明（2026-02-19）**  
> 本文档为 V4 主控规格书首版，从 `GPU_Rendering_System_V4.md` 设计中提炼实施层面的统一基线。  
> 后续每个 V4 Track 完成后，需同步回填本文件中的里程碑、风险与性能基线章节。

---
_Generated by Feature Planner Engine._
