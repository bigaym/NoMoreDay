# V3 Validation and Release Gate Plan

> **Track ID**: `v3_validation_and_release_gate_20260215`  
> **TDD Policy**: validation-driven  
> **实施路线**: Step F（第 8-10 周）  
> **前置依赖**: 所有 V3 feature tracks 已完成

## Phase 1: Foundation（门禁框架与基础设施）

- [ ] **F1.1**: 定义门禁矩阵正式 schema（JSON 格式），包含 4 层门禁的检查项清单。
- [ ] **F1.2**: 定义门禁产物格式（JSON + CSV），含版本号和时间戳。
- [ ] **F1.3**: 确认 `render.v3.enabled` Feature Flag 已就绪（来自 baseline Track）。
- [ ] **F1.4**: 定义 Pass 预算常量与阈值配置（来自 baseline Track），确认可读取。
- [ ] **F1.5**: 创建基线 benchmark profiles 定义文件（`baseline_270`, `combat_180`, `stress_144`）。

## Phase 2: 功能门禁（Functional Gate）

- [ ] **F2.1**: 实现默认帧缓冲路径渲染正确性测试（无黑屏检测）。
- [ ] **F2.2**: 实现离屏帧缓冲路径渲染正确性测试。
- [ ] **F2.3**: 实现多分辨率路径测试（至少 3 种分辨率：720p/1080p/1440p）。
- [ ] **F2.4**: 实现 Quality Tier 全矩阵测试（Low/Medium/High/Ultra 自动遍历）。
- [ ] **F2.5**: 实现 Resize 连续操作稳定性测试（10 次连续 resize，无崩溃/泄漏）。
- [ ] **F2.6**: 实现 Alt+Tab / context restore 测试。
- [ ] **F2.7**: 实现 Shader/Material/VFX 热重载稳定性测试。

## Phase 3: 契约门禁（Contract Gate）

- [ ] **F3.1**: 整合 ABI layout 快照测试（确认 GPU_ABI_VERSION=3 所有结构快照匹配）。
- [ ] **F3.2**: 整合 Binding Registry 冲突检查（确认无 binding 冲突，无字面量 binding）。
- [ ] **F3.3**: 整合 RenderGraph 合同验证（Pass 顺序 + Frame Ownership）。
- [ ] **F3.4**: 整合 Schema 版本校验（material_v2 + vfx_v3 格式正确）。
- [ ] **F3.5**: 汇总契约门禁报告，输出 JSON + CSV。

## Phase 4: 性能门禁（Performance Gate）

- [ ] **F4.1**: 实现 baseline_270 性能 profile 测试场景（常规场景 ≥270 FPS）。
- [ ] **F4.2**: 实现 combat_180 性能 profile 测试场景（高压场景 ≥180 FPS）。
- [ ] **F4.3**: 实现 stress_144 性能 profile 测试场景（极限场景 ≥144 FPS）。
- [ ] **F4.4**: 实现 V3 新增 Pass 单独计时验证（LightCulling/Shadow/Lighting 各 Pass 预算达标）。
- [ ] **F4.5**: 实现回归比较器：当前结果 vs 存储基线，>10% 回归触发阻断。
- [ ] **F4.6**: 实现 ≥128 lights Clustered Lighting 专项对比（≥25% 改善验证）。

## Phase 5: 稳定性门禁（Stability Gate）

- [ ] **F5.1**: 创建 30 分钟压力运行脚本（自动化场景循环 + VRAM 采样）。
- [ ] **F5.2**: 实现 VRAM 持续增长检测（30 分钟内 VRAM delta < 配置阈值）。
- [ ] **F5.3**: 实现 context restore 黑屏回归检测（模拟 Alt+Tab → 恢复 → 截图验证）。
- [ ] **F5.4**: 实现 Tier 切换压力测试（快速连续切换 Tier，验证无崩溃/抖动）。

## Phase 6: 截图差异回归

- [ ] **F6.1**: 定义关键场景截图基线（至少 6 个场景：空闲/战斗/Boss/多光/室内/室外）。
- [ ] **F6.2**: 实现截图对比自动化（SSIM > 0.95 或像素差异 < 2%）。
- [ ] **F6.3**: 超阈值差异 → 自动标记 review 需求 + 生成 diff 图。

## Phase 7: 回退演练与风险验证

- [ ] **F7.1**: 实现故障注入测试：模拟 Shadow Pass 失败 → 验证自动回退到 V2。
- [ ] **F7.2**: 实现故障注入测试：模拟性能回归 >10% → 验证阻断报告生成。
- [ ] **F7.3**: 实现灰度开关验证：`render.v3.enabled` 运行时切换 → V3/V2 路径正确切换。
- [ ] **F7.4**: 验证风险 R-V3-001（Atlas 溢出淘汰确定性）— 注入溢出场景。
- [ ] **F7.5**: 验证风险 R-V3-002（Cluster 溢出漏光）— 128+ lights 回归。
- [ ] **F7.6**: 验证风险 R-V3-003（ABI 偏移错位）— 篡改 ABI 版本触发阻断。
- [ ] **F7.7**: 验证风险 R-V3-004（Tier 降级抖动）— 快速 Tier 切换无帧时间波动。
- [ ] **F7.8**: 验证风险 R-V3-005（热重载中断）— 注入热重载失败不黑屏。

## Phase 8: 闭环与发布

- [ ] **F8.1**: 整合门禁 runner 到 `build.bat` QA 流程或独立 CI target。
- [ ] **F8.2**: 汇总全量门禁报告（功能 + 契约 + 性能 + 稳定性 + 截图 + 风险）。
- [ ] **F8.3**: 编写发布检查清单 (release_checklist.md)。
- [ ] **F8.4**: 编写交接文档 (handoff.md)：V3 架构概述 + 运维要点。
- [ ] **F8.5**: 执行全量 `build.bat`、`build.bat analyze`、`build.bat perf` 最终验证。

## Acceptance Gates (DoD)

- [ ] 功能/性能/稳定性/契约 4 层门禁全部报告 green。
- [ ] 合并阻断在合成回归场景下正确触发。
- [ ] V2 回退路径在门禁失败时正确激活。
- [ ] 截图差异回归通过（SSIM > 0.95）。
- [ ] 5 个风险的缓解措施全部验证通过。
- [ ] 产物输出完整、可复现、已存档。
- [ ] 发布检查清单和交接文档就绪。
