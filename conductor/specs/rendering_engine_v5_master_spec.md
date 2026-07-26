# 渲染引擎 V5 主控技术规格书

> **文档版本**: 1.1
> **创建日期**: 2026-02-19  
> **定位**: 统领所有 V5 阶段 Conductor Tracks 的**主控规格书**  
> **上游设计**: [`GPU_Rendering_System_V5.md`](../../设计文档/特效和UI/GPU_Rendering_System_V5.md)  
> **基线前提**: V4 全部 Tracks 已完成并通过验收门禁

---

## 1. 文档关系图

```
GPU_Rendering_System_V4.md (V4 设计 — 前置)
    ↓
GPU_Rendering_System_V5.md (V5 设计文档 — 设计基线)
    ↓
rendering_engine_v5_master_spec.md ← 本文档（主控规格书 — 实施基线）
    ├── v5_jfa_distance_field_20260219    (Track 6: JFA 距离场)
    ├── v5_radiance_cascades_gi_20260219  (Track 7: 辐射级联 GI)
    ├── v5_sph_fluid_exploration_20260219 (Track 8: SPH 流体探索)
    └── v5_validation_release_gate_20260219 (Track 9: V5 验收门禁)
```

---

## 2. V5 愿景与核心命题

> **渲染引擎的"次世代跃迁"**: V4 完成了 GPU 全驱动、2D PBR 材质和规模化光影，引擎具备了"商业级直接光照"能力。V5 要在此基础上实现**质的突变**：
>
> 1. **全局光照 (GI)**：光线在场景中多次反弹，无噪点、实时、全动态
> 2. **距离场加速**：JFA 以 O(log N) 复杂度提供高效空间跳跃
> 3. **流体模拟**（探索性质）：SPH 血液/水面与 GI 交互

### 预研声明

V5 包含前沿研究技术（辐射级联、Holographic RC），实施过程中如发现性能或质量无法达标：
1. 降低级联层数或分辨率
2. 限制 GI 为仅 Ultra 档专属
3. 回退到 V4 纯直接光照，作为**可接受的最终状态**

---

## 3. V5 前置依赖（来自 V4）

| V4 产出 | V5 依赖原因 | 对应 V4 Track |
|---------|------------|---|
| PBR 材质管线（Emission 通道） | GI 需要 Emission 标识发光体 | Track 3 |
| 全局高度场（R16F HeightMap） | 遮挡体提取依赖高度信息 | Track 4 |
| Clustered Forward+ V4 | GI 与直接光照叠加合成 | Track 4 |
| RenderGraph V4 Pass 序列 | GI Pass 插入 Lighting 之后 | Track 5 |
| ABI V4 生成链路 | V5 结构体继承 V4 治理 | Track 3+4 |

**提前启动条件**：若 V4-A/B 已完成（GPU Text/Loot + PBR Schema + Emission 通道），V5 Track 6（JFA）可提前开工。

---

## 4. Track 依赖关系图

```
V4 验收完成 (v4_validation_release_gate)
    │
    ├──→ Track 6: v5_jfa_distance_field     ─┐
    │                                         │
    │    ┌────────────────────────────────────┘
    │    ↓
    ├──→ Track 7: v5_radiance_cascades_gi   ─┐ (依赖 JFA SDF)
    │                                         │
    ├──→ Track 8: v5_sph_fluid_exploration  ─┤ (依赖 JFA + 可选 GI)
    │                                         │
    │    ┌────────────────────────────────────┘
    │    ↓
    └──→ Track 9: v5_validation_release_gate (依赖 ALL V5 feature tracks)
```

---

## 5. 各 Track 概要

### Track 6: JFA Distance Field Pipeline

**ID**: `v5_jfa_distance_field_20260219`  
**类型**: feature  
**优先级**: P0  
**预计周期**: 3-4 周  
**任务估算**: ~22 tasks

**核心子系统**:
- 遮挡体提取（`OccluderExtractPass`）：静态地形 + 大型障碍物 + 动态实体 → OccluderMask (R8)
- JFA 核心迭代（`JumpFloodCS`）：seed 初始化 → log₂(N) 轮跳跃传播 → JFA+1 补偿
- 距离计算（`DistanceCS`）：seed → 欧氏距离 + 符号标记 → R16F SDF
- Ping-Pong 双缓冲纹理管理
- 增量更新策略（chunk 局部重算）
- 帧间隔更新（静态场景 2-4 帧周期）

**ABI 新增**: 无新 GPU 结构（SDF 纹理通过 image load/store 直接操作）

**验收标准**:
- JFA SDF 在 1080p 下精度误差 < 2px（与精确距离场对比）
- JFA 单独耗时 ≤ 1.5ms @1080p (4070S)
- 增量更新路径在 chunk 级变化时不引入累积误差

---

### Track 7: Radiance Cascades Global Illumination

**ID**: `v5_radiance_cascades_gi_20260219`  
**类型**: feature  
**优先级**: P0  
**预计周期**: 5-6 周  
**任务估算**: ~35 tasks

**核心子系统**:
- Emissive Buffer 构建（场景灯光投影 + 材质 Emission + VFX 粒子写入）
- 级联计算（6 级 L0-L5，自顶向下合并）
  - 每级射线追踪（SDF 空间跳跃加速）
  - 每级上一级辐射度采样（双线性空间 + 角度插值）
  - 发光体贡献累加（Emissive Buffer 读取）
- GI Composite（RadianceMap → LitHDR 叠加）
- 时域稳定性（temporal blend，相机运动自适应）
- Holographic RC 可行性评估（V5-B 阶段）

**RenderGraph 位置**: `... → Lighting → [OccluderExtract → JFA → RadianceCascades → GI Composite] → Volumetric → ...`

**ABI 新增**: `RadianceCascadeConfig` (32B)

**验收标准**:
- 4 级联 half-res GI 在 ≥3 个场景类型下呈现可辨间接光照
- 6 级联 full-res 在洞穴场景呈现明显颜色溢出（color bleeding）
- 时域混合在相机移动时无可察觉拖影
- 6 级联单独耗时 ≤ 2.5ms @1080p (4070S)

---

### Track 8: SPH Fluid Simulation (Exploration)

**ID**: `v5_sph_fluid_exploration_20260219`  
**类型**: feature (exploration)  
**优先级**: P2（不阻断 V5 GI 核心交付）  
**预计周期**: 3-4 周  
**任务估算**: ~18 tasks

**核心子系统**:
- SPH 核心循环（NeighborSearchCS → DensityCS → ForceCS → IntegrateCS → RenderCS）
- 空间哈希网格（与 V4 战利品避让共用基础设施）
- 与 GI 交互（流体发光注入 Emissive Buffer、高密度液面更新 OccluderMask）
- 粒子上限（Ultra: 10K, High: 5K）

**ABI 新增**: `GPUFluidParticle` (48B), `GPUFluidConfig` (32B)

**预研性质声明**:
- 若效果/性能不满意，可不纳入正式发布
- 超预算时自动切换为简化粒子特效（不做 SPH，仅视觉）

---

### Track 9: V5 Validation & Release Gate

**ID**: `v5_validation_release_gate_20260219`  
**类型**: quality  
**优先级**: P0  
**预计周期**: 2-3 周  
**任务估算**: ~25 tasks

**核心验证维度**:
1. **功能门禁**: JFA 精度、GI 间接光照可辨、颜色溢出呈现、时域无拖影
2. **性能门禁**: Ultra 常规 ≥180FPS, High half-res ≥270FPS, 极限场景 GI 自动关闭后恢复 V4 水平
3. **契约门禁**: ABI V5 layout 测试、新 binding 不冲突、RenderGraph 合同验证
4. **稳定性门禁**: 30 分钟 GI 压力运行、GI 开关切换无黑帧、SDF 增量无累积误差
5. **回退门禁**: `render.gi.enabled=false` 完整回退 V4, `render.fluid.enabled=false` 资源完全释放

---

## 6. ABI 迁移路径

```
V4 (GPU_ABI_VERSION = 4)
    ↓ Track 6: 无新 GPU 结构（SDF 通过 image 操作）
    ↓ Track 7: 新增 RadianceCascadeConfig (32B)
    ↓ Track 8: 新增 GPUFluidParticle (48B) + GPUFluidConfig (32B)
V5 (GPU_ABI_VERSION = 5)
```

**关键原则**:
1. V5 ABI 变更核心在 Track 7，锁定版本号 5
2. Track 8 (SPH) 结构随 Track 7 一起纳入 ABI V5
3. Track 6 无新结构，不触发 ABI 版本变更
4. 复用 V4 结构（`GPUMaterialDataV3`, `GPULightV2`）不做修改

---

## 7. 性能预算汇总

### V5 新增 Pass 预算

| Pass | 常规 (270FPS) | 高压 (180FPS) | 极限 (144FPS) | 来源 Track |
|------|:---:|:---:|:---:|---|
| OccluderExtractPass | 0.10ms | 0.15ms | 0.20ms | Track 6 |
| JFA Pass | 0.40ms | 0.60ms | 0.80ms | Track 6 |
| RadianceCascadesPass | 1.20ms | 1.80ms | 2.50ms | Track 7 |
| GI CompositePass | 0.05ms | 0.08ms | 0.10ms | Track 7 |
| SPH FluidPass | 0.30ms | 0.60ms | 0.80ms | Track 8 |
| **V5 新增总计** | **2.05ms** | **3.23ms** | **4.40ms** | — |

### 帧预算腾挪策略（GI 开启时）

| 被降级的 V4 Pass | 节省 | 触发条件 |
|-----------------|:---:|----------|
| Clustered 光源: 4096→1024 | ~0.15ms | GI 开启 |
| HeightShadow: 64→16 steps | ~0.30ms | GI 开启 |
| VFX 粒子上限降 30% | ~0.20ms | GI + 极限场景 |
| POM 关闭 | ~0.20ms | GI + 极限场景 |
| SDF 每 2 帧更新 | ~0.20ms/帧均摊 | 静态/低动态场景 |
| **可腾挪总计** | **~1.05ms** | — |

---

## 8. Quality Tier 矩阵（V5）

| 能力 | Low | Medium | High | Ultra |
|------|:---:|:------:|:----:|:-----:|
| GI | Off | Off | 4-cascade half-res | 6-cascade full-res |
| JFA SDF | Off | Off | Half-res, 每 2 帧 | Full-res, 每帧 |
| SDF 动态遮挡体 | Off | Off | 关键实体 only | 全部 |
| GI temporal blend | — | — | 0.92 | 0.88 |
| SPH Fluid | Off | Off | Off | Off（历史探索结论为 NO-GO；仅开发构建显式 opt-in） |
| Emissive Buffer | Off | Off | Half-res | Full-res |

---

## 9. 风险矩阵

| ID | 风险 | 影响 | 概率 | 缓解 | 监控 Track |
|:---:|------|------|:---:|------|---|
| V5-R01 | JFA 精度不足致 GI 漏光 | 视觉错误 | 中 | JFA+1 补偿 + 降级到 JFA+2 | Track 6 |
| V5-R02 | RC 复杂场景仍有噪点 | 画面品质 | 低 | L0 射线增至 8 + temporal 0.95 | Track 7 |
| V5-R03 | 帧预算不足（极限场景） | 性能 | **高** | half-res + 减少级联 + 帧间隔 + 极限关闭 | Track 7+9 |
| V5-R04 | 时域混合运动拖影 | 视觉瑕疵 | 中 | 运动降低 temporal + reprojection | Track 7 |
| V5-R05 | Emissive Buffer VFX 写入竞争 | 性能/正确性 | 中 | 独立 sub-buffer + 原子操作 | Track 7 |
| V5-R06 | SPH 粒子不稳定 | 视觉错误 | 中 | Leapfrog + CFL + 回收机制 | Track 8 |
| V5-R07 | OGL 4.3 计算天花板 | 架构限制 | 低 | 预研 Vulkan 迁移路径（V6 范畴） | Track 9 |
| V5-R08 | Holographic RC 无法高效实现 | 探索失败 | 中 | 标准 RC 为主方案 | Track 7 |

---

## 10. 兜底策略

**如果 V5 GI 最终无法达到可接受的性能/质量水平**：
1. GI 降级为仅 Ultra 档、仅洞穴/室内场景启用
2. 用 V4 多光源 + 体积光近似间接光照效果
3. 回退到 V4 作为正式发布的渲染基线
4. GI 技术储备保留，等待 Vulkan 迁移（V6）后重新启用

---

## 11. 里程碑时间线

```
V4 验收完成
    ↓
Week 0-3:  Track 6 (JFA Distance Field) ────────────────────┐
Week 3-8:  Track 7 (Radiance Cascades GI) ──────────────────┤
Week 5-8:  Track 8 (SPH Fluid — 可与 Track 7 后半段并行) ──┤
Week 8-10: Track 9 (V5 Validation Gate) ────────────────────┘
                                              ↓
                              V5 里程碑完成
                                              ↓
                               历史 V5 Core GO（生产整改见第 12 节）
```

---

## 12. 2026-07 生产整改路线

2026-07-26 架构审查确认，历史 V5 Core GO 只证明当时的功能实现和部分门禁，不能证明 Gameplay 离屏生产路径完整执行。不得优先增加视觉特性、重启 SPH 或开始 Vulkan 迁移；当前生产整改顺序如下：

```
M0-A: gpu_production_hdr_gi_closure_20260726 (P0)
  -> M0-B: gpu_rendergraph_resource_foundation_20260726 (P0)
    -> M0-C: gpu_hardware_validation_gate_20260726 (P0)
      -> M1-D: gpu_jfa_incremental_update_20260726 (P1)
      -> M2-E: gpu_adaptive_quality_control_20260726 (P2)
```

| Track | 目标 | 依赖 | 生产门禁关系 |
| --- | --- | --- | --- |
| [M0-A](../tracks/gpu_production_hdr_gi_closure_20260726/index.md) | 离屏 HDR/GI、GI 正确性、SPH NO-GO | 历史 V5 Core | 首要阻断项 |
| [M0-B](../tracks/gpu_rendergraph_resource_foundation_20260726/index.md) | typed resource、compiled plan、同步/资源/ABI/能力基础 | M0-A | M0-C 前置 |
| [M0-C](../tracks/gpu_hardware_validation_gate_20260726/index.md) | 完整 Gameplay 硬件 nightly/release gate | M0-A、M0-B | production GO 唯一证据 |
| [M1-D](../tracks/gpu_jfa_incremental_update_20260726/index.md) | 正确的 JFA dirty-region 更新 | M0-C | 非 M0 production 阻断项 |
| [M2-E](../tracks/gpu_adaptive_quality_control_20260726/index.md) | DRS 与自动曝光 | M0-C | 非 M0 production 阻断项 |

SPH 继续为 NO-GO，所有 shipped quality tier 默认关闭。OpenGL 4.3 仍以单队列为前提；只有 M0-C 用当前硬件数据证明架构边界后，才可讨论 V6 Vulkan 决策。

---

## 13. 修订说明

> **修订说明（2026-02-19）**  
> 本文档为 V5 主控规格书首版，从 `GPU_Rendering_System_V5.md` 设计中提炼实施层面的统一基线。  
> V5 含预研性质，SPH 流体为可选交付。  
> 后续每个 V5 Track 完成后，需同步回填本文件中的里程碑、风险与性能基线章节。
>
> **修订说明（2026-07-26）**
> 根据 `docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md`，新增第 12 节生产整改路线；历史 V5 Core GO 不再等同于 Gameplay production GO，SPH Ultra 路由修订为 Off。

---
_Generated by Feature Planner Engine._
