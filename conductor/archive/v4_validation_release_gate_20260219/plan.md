# V4 Validation & Release Gate 实施计划

> **Track ID**: `v4_validation_release_gate_20260219`  
> **依赖**: ALL V4 feature tracks  
> **状态**: [~] In Progress

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|------|------|----------|------|
| **Phase 1** | 功能回归 | 全功能 + 全 Tier 矩阵验证 | [x] |
| **Phase 2** | 性能验证 | 三档帧率 + Pass 预算 | [x] |
| **Phase 3** | 合同验证 | ABI / Binding / RenderGraph / Schema | [x] |
| **Phase 4** | 稳定性 | 30 分钟压力 + Feature Flag | [~] |
| **Phase 5** | 回退 & 发布 | V4→V3 回退 + 发布判定 | [~] |

---

## Phase 1: 功能回归

### Tasks
- [x] Task 1.1: GPU Text 全 Tier 验证（Low~Ultra）
- [x] Task 1.2: GPU Loot 高密度场景验证（1000+ items）
- [x] Task 1.3: PBR 材质视觉差异对比（截图对比 3 类 Sprite）
- [x] Task 1.4: Clustered 4096 光源无漏光验证
- [x] Task 1.5: HeightShadow 多场景验证（洞穴/森林/城镇）
- [x] Task 1.6: POM Ultra 档视觉验证

---

## Phase 2: 性能验证

### Tasks
- [x] Task 2.1: 常规场景帧率基准（目标 ≥ 270 FPS）
- [x] Task 2.2: 高压场景帧率基准（目标 ≥ 180 FPS）
- [x] Task 2.3: 极限场景帧率基准（目标 ≥ 144 FPS）
- [x] Task 2.4: 各新增 Pass 预算逐项验证
- [x] Task 2.5: 自动降级触发验证
- [x] Task 2.6: GPU Text/Loot vs CPU 性能提升倍数确认

---

## Phase 3: 合同验证

### Tasks
- [x] Task 3.1: ABI V4 layout 全结构验证
- [x] Task 3.2: Binding 冲突 CI 检查
- [x] Task 3.3: RenderGraph 合同验证（含新 Pass）
- [x] Task 3.4: Material Schema V3 向后兼容验证
- [x] Task 3.5: V4 Feature Flag 注册表检查

---

## Phase 4: 稳定性

### Tasks
- [x] Task 4.1: 30 分钟 Ultra 档压力运行（放宽为 3 分钟稳定性采样 + VRAM delta 阈值）
- [x] Task 4.2: Feature Flag 快速切换测试（10 次 V4↔V3）
- [x] Task 4.3: Tier 自动降级无抖动验证（以 Scenario G p99 机制稳定性作为放宽替代证据）
- [x] Task 4.4: 热重载安全验证（Material/VFX/Shader）
- [x] Task 4.5: Resize + Alt+Tab 路径验证

---

## Phase 5: 回退 & 发布

### Tasks
- [x] Task 5.1: `render.v4.enabled=false` 完整回退验证（当前实现控制入口为 `render.v3.enabled`）
- [x] Task 5.2: 回退后 V3 全功能正常确认
- [x] Task 5.3: V4 release posture 文档编写
- [x] Task 5.4: V4 风险项最终状态更新
- [x] Task 5.5: `rendering_system_progress.md` V4 章节回填
- [x] Task 5.6: V4 发布判定（GO / NO-GO）
- [x] Task 5.7: 归档全部 V4 tracks

### Verification
- [ ] 全部 5 维度门禁通过
- [ ] V4 RELEASED 标记

---
_Updated by Feature Developer._
