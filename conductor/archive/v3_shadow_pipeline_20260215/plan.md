# V3 Shadow Pipeline Plan

> **Track ID**: `v3_shadow_pipeline_20260215`  
> **TDD Policy**: unit -> integration -> perf  
> **实施路线**: Step B（第 2-4 周）  
> **前置依赖**: `v3_baseline_contracts_20260216` 已完成

## Phase 1: Foundation（数据结构与骨架）

- [x] **B1.1**: 在 `GPUData.hpp` 中实例化 `GPUShadowCaster`、`GPUShadowLight`、`GPUShadowAtlasMeta` 结构体（含 static_assert 对齐检查）。
- [x] **B1.2**: 定义 `ShadowCasterComponent` ECS 组件（occluder shape type、height、dynamic flag）。
- [x] **B1.3**: 在 `RenderConstants.hpp` 中添加阴影相关常量：`kMaxShadowCasters`、`kShadowChunkSize`、`kCameraNeighborhoodRadius`、`kAtlasEvictionHysteresis`。
- [x] **B1.4**: 创建 `ShadowPreparePass`、`ShadowBuildPass`、`ShadowResolvePass` 骨架类（注册到 RenderGraph，初始为 no-op）。
- [x] **B1.5**: 创建 `ShadowAtlasAllocator` 骨架类（tile 分配接口 + 淘汰接口）。
- [x] **B1.6**: 添加 unit test：GPU 结构 layout/size 断言、Component 默认值验证。

## Phase 2: 遮挡体收集（OccluderCollector）

- [x] **B2.1**: 实现世界 chunk 划分逻辑，定义 chunk 坐标映射。
- [x] **B2.2**: 实现静态遮挡体按 chunk 缓存：首次入视野时上传 GPU，LRU 淘汰冷 chunk。
- [x] **B2.3**: 实现动态遮挡体 camera 邻域收集：仅收集 `kCameraNeighborhoodRadius` 范围内的动态遮挡体。
- [x] **B2.4**: 实现遮挡体 SSBO staging 与批量上传路径。
- [x] **B2.5**: 添加 unit test：chunk 缓存命中/淘汰、邻域过滤正确性、staging 数据一致性。

## Phase 3: SDF Shadow 路径（High 档）

- [x] **B3.1**: 实现 `ShadowBuildPass` 的 SDF 计算路径（compute shader）。
- [x] **B3.2**: 实现 `ShadowResolvePass`：从 SDF 场输出 `ShadowMask` 纹理。
- [x] **B3.3**: 实现 shadowSoftness 参数化控制（来自 RenderConfig）。
- [x] **B3.4**: 添加 unit test：SDF 采样边界条件、空场景/单遮挡体/多遮挡体场景。

## Phase 4: Atlas Shadow 路径（Ultra 档）

- [x] **B4.1**: 实现关键光选择算法：`priority + screen influence` 复合评分 top-N 排序。
- [x] **B4.2**: 实现 `ShadowAtlasAllocator` 的 tile 分配与确定性淘汰（LRU + 优先级），添加 **滞回策略**（连续 N 帧低优先级才释放）。
- [x] **B4.3**: 实现 Atlas tile 渲染路径（per-light shadow map 写入 atlas 子区域）。
- [x] **B4.4**: 实现溢出计数器与结构化日志输出。
- [x] **B4.5**: 添加 unit test：淘汰确定性、滞回策略行为、溢出计数准确性。

## Phase 5: 光照整合与 Tier 联动

- [x] **B5.1**: 在 `light_accumulation.frag` 中集成 `shadowFactor`：`attenuation * shadowFactor * lighting`。
- [x] **B5.2**: 实现 Quality Tier 策略联动：Medium=Off、High=SDF、Ultra=Hybrid（从 RenderConfig 读取）。
- [x] **B5.3**: 实现 GL 同步点：`ShadowBuildPass`(compute) -> `ShadowResolvePass/LightingPass`(fragment) 的 `glMemoryBarrier`。
- [x] **B5.4**: 实现失败回退：任一 Shadow Pass 失败时回退到 V2 Lighting 路径，发出 `spdlog::warn` 含失败原因和帧号。
- [x] **B5.5**: 验证 default framebuffer 和 offscreen framebuffer ownership 不变式。
- [x] **B5.6**: 添加 integration test：Tier 切换流畅性、回退路径正确性。

## Phase 6: 测试矩阵与性能验证

- [x] **B6.1**: 添加 integration test：resize、Alt+Tab/context restore、离屏路径稳定性。
- [x] **B6.2**: 添加 Quality Tier 全矩阵 integration test（Low/Medium/High/Ultra）。
- [x] **B6.3**: 添加 performance test：Shadow 各 Pass 单独计时（mean/p95/p99），验证 High≤0.8ms、Ultra≤1.3ms。
- [x] **B6.4**: 添加 Atlas 压力测试：注入大量光源触发溢出，验证确定性淘汰与无闪烁。
- [x] **B6.5**: 运行 `build.bat`、`build.bat analyze`、`build.bat perf`，修复回归。

## Acceptance Gates (DoD)

- [x] High 档阴影开销 ≤ `0.8ms`；Ultra 档 ≤ `1.3ms`。
- [x] Medium 档阴影零开销。
- [x] Default/offscreen 路径无黑屏回归。
- [x] Atlas 溢出淘汰行为确定性（多次运行结果一致）。
- [x] 回退到 V2 后视觉/性能与 V2 一致。
- [x] 视觉回归截图差异阈值受控（边缘稳定，无严重闪烁）。
