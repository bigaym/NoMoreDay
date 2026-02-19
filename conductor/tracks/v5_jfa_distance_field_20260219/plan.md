# JFA Distance Field Pipeline 实施计划

> **Track ID**: `v5_jfa_distance_field_20260219`  
> **依赖**: `v4_validation_release_gate_20260219`（或满足 V5 启动前置条件）  
> **状态**: [ ] Not Started  
> **策略基线**: JFA 实时主路径 + 精确 EDT 对照验证 + JFA+2 异常兜底

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|------|------|----------|------|
| **Phase 1** | 遮挡体输入与资源契约 | OccluderExtract 输入闭环 + V5 资源规格落地 | [ ] |
| **Phase 2** | JFA 全量重算正确性 | SeedInit/JFA/JFA+1/Distance 全链路可验证 | [ ] |
| **Phase 3** | 运行时优化与兜底 | Half-res/间隔更新/增量更新/JFA+2 fallback | [ ] |
| **Phase 4** | 集成验收与门禁证据 | RenderGraph 集成 + 性能/精度门禁证据 | [ ] |

---

## Phase 1: 遮挡体输入与资源契约

### Tasks
- [ ] Task 1.1: 固化 JFA 资源格式与命名（`OccluderMask=R8`, `SeedPing/Pong=RG16UI`, `DistanceField=R16F`），同步 `RenderConstants/RenderGraph` 资源标签。
- [ ] Task 1.2: 实现静态遮挡体层缓存（chunk 加载/卸载时更新，不逐帧重建）。
- [ ] Task 1.3: 实现动态遮挡体层写入（关键实体投影），并支持与静态层 OR 合成。
- [ ] Task 1.4: 落地 `OccluderExtractPass` 输入输出契约（禁止写 FBO0，输出到离屏纹理）。
- [ ] Task 1.5: 增加 OccluderMask debug 可视化路径（便于误差定位和回归截图）。
- [ ] Task 1.6: 覆盖 resize 重建路径（窗口尺寸变化后资源安全重建且句柄有效）。

### Verification
- [ ] OccluderMask 轮廓与碰撞数据一致，静态层在 120 帧内无无效重建。
- [ ] Resize 后 `OccluderMask/Seed/SDF` 资源有效且无崩溃或黑帧。

---

## Phase 2: JFA 全量重算正确性

### Tasks
- [ ] Task 2.1: 实现 `SeedInitCS`（遮挡体写自身坐标，空白写 `0xFFFF` 哨兵）。
- [ ] Task 2.2: 实现 `JumpFloodCS`（8 邻域传播 + 指数步长调度 + ping-pong 双缓冲）。
- [ ] Task 2.3: 实现 `JFA+1` 补偿（step=2,1）并保留可开关控制。
- [ ] Task 2.4: 实现 `DistanceCS`（欧氏距离 + 有符号距离输出到 `R16F`）。
- [ ] Task 2.5: 实现精确 EDT 对照链路（离线/测试路径，非实时默认路径）。
- [ ] Task 2.6: 实现误差统计工具（RMS/P95/Max）并纳入单测或集成测试报告。

### Verification
- [ ] Full-res JFA 对照精确 EDT：`P95 <= 2px` 且 `Max <= 4px`（1080p 基准场景）。
- [ ] 遮挡体内 SDF 为负，外部为正，边界连续无明显断裂。

---

## Phase 3: 运行时优化与兜底

### Tasks
- [ ] Task 3.1: Half-res 路径（默认 High 档），实现上采样并输出到统一 SDF 读口。
- [ ] Task 3.2: `sdfUpdateInterval` 帧间隔更新控制（静态/低动态场景节流）。
- [ ] Task 3.3: `JFA+2` 兜底路径（当误差或溢出计数越界时自动切换）。
- [ ] Task 3.4: 增量更新实验路径（dirty chunk/rect 局部重算，feature flag 控制）。
- [ ] Task 3.5: 完成 barrier 审计（compute 写后采样读前 `glMemoryBarrier` 点位清单化）。

### Verification
- [ ] Half-res 对照 Full-res：`RMS <= 1px`, `P95 <= 2px`。
- [ ] 间隔更新 120 帧稳定性：边界抖动幅度 `<= 0.5px`。
- [ ] 触发兜底后误差恢复到门禁阈值内，无持续抖动或闪烁。

---

## Phase 4: 集成验收与门禁证据

### Tasks
- [ ] Task 4.1: 串联 RenderGraph Pass：`OccluderExtract -> JFA -> Distance`，并对外暴露 `DistanceField` 资源。
- [ ] Task 4.2: 接入 Tier 策略（Low/Med Off, High Half-res+Interval, Ultra Full-res 每帧）。
- [ ] Task 4.3: 接入 Profiling 指标并验证 JFA pass 预算（目标 `<= 1.5ms @1080p/4070S`）。
- [ ] Task 4.4: 增补测试覆盖（合同、精度、resize、fallback、barrier 关键断言）。
- [ ] Task 4.5: 产出 Track 验证证据并同步风险项（若未达标，挂接 `bug_registry` 并标注非阻塞/阻塞）。

### Verification
- [ ] `build.bat` 通过，`ctest -L ci/unit/integration` 通过。
- [ ] RenderGraph 合同校验通过，`DistanceField` 可被 Track 7 正常读取。
- [ ] 所有 AC 达成，或未达标项已具备可追踪风险与回退方案。

---

## 执行顺序约束

1. 先完成 Phase 2 全量正确性，再推进 Phase 3 优化项。  
2. `JFA+2` 与精确 EDT 仅作为兜底/验证，不得替代默认实时主路径。  
3. 任一阶段出现性能/稳定性回归，先修复再进入下一阶段。  

---
_Updated for V5 review alignment (2026-02-20)._ 
