# V3 Clustered 2D Lighting Plan

> **Track ID**: `v3_clustered_lighting_20260215`  
> **TDD Policy**: unit -> integration -> perf  
> **实施路线**: Step C（第 3-5 周）  
> **前置依赖**: `v3_baseline_contracts_20260216` 已完成

## Phase 1: Foundation（数据结构与常量）

- [ ] **C1.1**: 在 `GPUData.hpp` 中实例化 `GPUClusterHeader`、`GPUClusterLightIndex`、`GPULightBounds` 结构体（含 static_assert 对齐检查）。
- [ ] **C1.2**: 在 `RenderConstants.hpp` 中定义 Cluster 常量：`kDefaultClusterTileSize`、`kDefaultClusterZSliceCount`、`kMaxLightsPerCluster`、`kMaxTotalClusteredLights`。
- [ ] **C1.3**: 实现渲染层 ID -> z-slice 映射函数，文档化映射规则（基于渲染层/高度带，非真实 3D 深度）。
- [ ] **C1.4**: 创建 `ClusteredLightingState` 单例骨架（buffer pool、帧计数、索引表接口）。
- [ ] **C1.5**: 添加 unit test：index math 正确性、cluster-id 映射、z-slice 映射边界。

## Phase 2: LightCullingPass（Compute 路径）

- [ ] **C2.1**: 创建 `light_culling.comp` shader 骨架，定义 workgroup 和 cluster 布局。
- [ ] **C2.2**: 实现 AABB 粗筛阶段：`GPULightBounds` vs cluster bounds 检测。
- [ ] **C2.3**: 实现半径/锥体精筛阶段。
- [ ] **C2.4**: 实现 `maxLightsPerCluster` 上限裁剪与优先级排序（优先级高的光源保留）。
- [ ] **C2.5**: 实现溢出计数器写入 `GPUClusterHeader.overflowCount`。
- [ ] **C2.6**: 实现 `LightCullingPass` C++ 侧调度逻辑：buffer bind、dispatch、barrier。
- [ ] **C2.7**: 添加 unit test：单 cluster 内光源填充、溢出裁剪确定性、空场景处理。

## Phase 3: LightingPass 整合

- [ ] **C3.1**: 修改 `light_accumulation.frag`：添加 cluster 光源列表读取分支（从 SSBO 读取 `GPUClusterHeader` + `GPUClusterLightIndex`）。
- [ ] **C3.2**: 插入 `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)` 同步点：`LightCullingPass` compute 输出 -> `LightingPass` fragment 读取。
- [ ] **C3.3**: 实现回退路径：当 `clusteredLightingEnabled=false` 或资源失败时，切换到 V2 全光遍历路径。
- [ ] **C3.4**: 实现 warning 日志节流（once-per-window）。
- [ ] **C3.5**: 集成 Tier 默认值与自动降级序列（`QualityTierManager`）。
- [ ] **C3.6**: 验证 default/offscreen pipeline 兼容性。
- [ ] **C3.7**: 添加 integration test：Tier 切换、回退路径、cluster 读取正确性。

## Phase 4: 测试矩阵与性能验证

- [ ] **C4.1**: 添加 integration test：resize、Alt+Tab/context restore、离屏路径稳定性。
- [ ] **C4.2**: 添加 Quality Tier 全矩阵 integration test（Low/Medium/High/Ultra）。
- [ ] **C4.3**: 添加 performance benchmark：≥128 lights 场景 A/B 对比（V2 vs Clustered），验证 ≥25% 改善。
- [ ] **C4.4**: 添加 performance benchmark：低光场景（<16 lights），验证无性能回归。
- [ ] **C4.5**: 添加边界条件 test：0 lights、1 light、超满 cluster、不均匀分布。
- [ ] **C4.6**: 运行 `build.bat`、`build.bat analyze`、`build.bat perf`，修复回归。

## Acceptance Gates (DoD)

- [ ] ≥128 lights: Lighting pass 平均耗时 ≥25% 改善。
- [ ] 低光场景无性能回归（等于或快于 baseline）。
- [ ] 无跨 Tier 闪烁/漏光回归。
- [ ] Overflow 计数器可读取并有测试验证。
- [ ] `glMemoryBarrier` 同步点可审计。
- [ ] 回退到 V2 路径后视觉一致。
