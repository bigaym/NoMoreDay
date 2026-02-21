# V5 Validation & Release Gate 规格说明书

> **Track ID**: `v5_validation_release_gate_20260219`  
> **设计参考**: [`GPU_Rendering_System_V5.md` §12](../../../设计文档/特效和UI/GPU_Rendering_System_V5.md)  
> **主控规格书**: [`rendering_engine_v5_master_spec.md`](../../specs/rendering_engine_v5_master_spec.md)  
> **状态**: ✅ Completed (Core GO, SPH NO-GO)

---

## 1. 概述

V5 核心功能（JFA + GI）完成后的全链路验收。鉴于 V5 含有预研性质（SPH），验收标准分为**核心交付**和**可选交付**。

## 2. 核心交付门禁（MUST PASS）

### 2.1 功能门禁
- [x] JFA SDF 精度 < 2px（1080p，与精确距离场对比）
- [x] 4-cascade half-res GI 在 **≥3 场景类型**（洞穴/城镇/森林）下呈现可辨间接光照
- [x] **6-cascade full-res GI 在洞穴场景呈现明显颜色溢出**（color bleeding，相邻面颜色溢出 ≥ 10% 亮度变化）
- [x] GI 时域混合拖影阈值满足：相机停止后 5 帧内，运动边缘残影亮度差 ≤ 5%
- [x] GI 与 V4 直接光照叠加亮度比满足：同一 ROI 下 `L(GI+Direct)/L(Direct)` 在 [0.70, 1.30]

### 2.2 性能门禁
- [x] Ultra 档常规场景 ≥ **180 FPS**（GI 开启，4070S@1080p）
- [x] High 档常规场景 ≥ **270 FPS**（4-cascade half-res GI）
- [x] 极限场景 GI 自动关闭后性能恢复到 V4 基线的 97% 以上
- [x] **JFA 单独耗时 ≤ 1.5ms** @1080p (4070S)
- [x] **6-cascade 单独耗时 ≤ 2.5ms** @1080p (4070S)
- [x] 各新增 Pass 不超过预算表 +10%

### 2.3 契约门禁
- [x] ABI V5 layout 测试通过
- [x] 新 binding 不冲突
- [x] RenderGraph 合同验证通过（含 GI 4 Pass）

### 2.4 稳定性门禁
- [x] 30 分钟 GI 压力运行无崩溃，GPU 显存占用漂移 ≤ 64MB（5 分钟滑窗无单调上升）
- [x] GI 开关切换无黑帧
- [x] SDF 增量更新 100 次后误差仍满足 max error < 2px（相对精确距离场）

### 2.5 回退门禁
- [x] `render.gi.enabled=false` 完整回退到 V4（视觉回归截图通过）
- [x] 回退后 V4 全功能正常，性能不低于 V4 基线 97%

## 3. 可选交付门禁（SPH）

- [x] SPH GO/NO-GO 决策已记录
- [x] 若 GO: 10K 粒子性能 ≤ 0.80ms, 资源正确释放（N/A，本轮 NO-GO 路径）
- [x] 若 NO-GO: 回退到纯视觉粒子，无残留资源

## 4. 架构前瞻评估

- [x] OpenGL 4.3 是否已到性能天花板？
- [x] Vulkan 迁移前期评估（V6 范畴，仅记录结论）

## 5. 验收标准

全部核心门禁通过 + SPH 决策已记录后，V5 标记为 RELEASED，引擎成熟体完成。

---
_Updated by Feature Developer._
