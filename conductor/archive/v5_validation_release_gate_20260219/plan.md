# V5 Validation & Release Gate 实施计划

> **Track ID**: `v5_validation_release_gate_20260219`  
> **依赖**: `v5_jfa_distance_field_20260219`, `v5_radiance_cascades_gi_20260219`（`v5_sph_fluid_exploration_20260219` 仅作为 GO/NO-GO 决策输入）  
> **状态**: [x] Completed (2026-02-21 verify + closeout sync)

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|------|------|----------|------|
| **Phase 1** | GI 功能回归 | JFA 精度 + GI 视觉 + 时域 | [x] |
| **Phase 2** | 性能验证 | 三档帧率 + Pass 预算 | [x] |
| **Phase 3** | 契约 & 稳定性 | ABI V5 + RenderGraph + 压力测试 | [x] |
| **Phase 4** | SPH 决策 | GO/NO-GO 判定 + 资源验证 | [x] |
| **Phase 5** | 回退 & 发布 | V5→V4 回退 + 架构评估 + 发布判定 | [x] |

---

## Phase 1: GI 功能回归

### Tasks
- [x] Task 1.1: JFA SDF 精度验证（< 2px @1080p）
- [x] Task 1.2: 4-cascade half-res GI 3+ 场景视觉验证
- [x] Task 1.3: 6-cascade full-res GI 颜色溢出验证（洞穴场景）
- [x] Task 1.4: 时域混合拖影检查（相机停止后 5 帧内，残影亮度差 ≤ 5%）
- [x] Task 1.5: GI + V4 直接光照叠加平衡验证（ROI 亮度比 [0.70, 1.30]）

---

## Phase 2: 性能验证

### Tasks
- [x] Task 2.1: Ultra 常规场景 ≥ **180 FPS** 验证（4070S@1080p, GI 开启）
- [x] Task 2.2: High half-res 常规 ≥ **270 FPS** 验证
- [x] Task 2.3: GI 自动关闭后恢复到 V4 基线 97% 以上帧率验证
- [x] Task 2.4: **JFA 单独耗时 ≤ 1.5ms** @1080p 测量（RenderDoc GPU Timer）
- [x] Task 2.5: **6-cascade 单独耗时 ≤ 2.5ms** @1080p 测量
- [x] Task 2.6: OccluderExtractPass, GI CompositePass 各不超预算 +10% 验证
- [x] Task 2.7: V4 Pass 帧预算腾挪效果验证（GI 开启时 V4 Cluster/HeightShadow 正确降级）

---

## Phase 3: 契约 & 稳定性

### Tasks
- [x] Task 3.1: ABI V5 layout 全结构验证
- [x] Task 3.2: 新 binding 冲突检查
- [x] Task 3.3: RenderGraph 合同验证（含 GI 4 Pass）
- [x] Task 3.4: 30 分钟 GI Ultra 压力运行（GPU 显存漂移 ≤ 64MB，5 分钟滑窗无单调上升）
- [x] Task 3.5: GI 开关切换测试（10 次）
- [x] Task 3.6: SDF 增量累积误差检查（100 次增量更新后 max error < 2px）

---

## Phase 4: SPH 决策

### Tasks
- [x] Task 4.1: SPH GO/NO-GO 决策审查
- [x] Task 4.2: 若 GO: SPH 性能 + 资源释放验证（N/A，本轮为 NO-GO 路径）
- [x] Task 4.3: 若 NO-GO: 回退确认 + 残留资源检查
- [x] Task 4.4: SPH 决策文档归档

---

## Phase 5: 回退 & 发布

### Tasks
- [x] Task 5.1: `render.gi.enabled=false` 完整回退到 V4 验证
- [x] Task 5.2: `render.fluid.enabled=false` 资源完全释放验证
- [x] Task 5.3: 回退后 V4 全功能确认
- [x] Task 5.4: OpenGL 4.3 性能天花板评估记录
- [x] Task 5.5: Vulkan 迁移前期笔记（V6 范畴）
- [x] Task 5.6: V5 release posture 文档编写
- [x] Task 5.7: `rendering_system_progress.md` V5 章节回填
- [x] Task 5.8: V5 发布判定（GO / NO-GO）
- [x] Task 5.9: 归档全部 V5 tracks

### Verification
- [x] 核心门禁全部通过
- [x] SPH 决策已记录
- [x] V5 RELEASED 标记

---
_Updated by Feature Developer._
