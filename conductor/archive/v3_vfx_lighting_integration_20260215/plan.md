# V3 VFX Lighting Integration Plan

> **Track ID**: `v3_vfx_lighting_integration_20260215`  
> **TDD Policy**: unit -> integration -> perf  
> **实施路线**: Step E（第 6-8 周）  
> **前置依赖**: `v3_baseline_contracts_20260216` + Shadow + Clustered + Material Tracks 已完成

## Phase 1: Foundation（Schema 与类型定义）

- [x] **E1.1**: 在 `VFXTypes.hpp` 中定义新事件枚举：`ShadowPulse`, `LightProfileBlend`, `MaterialPhaseShift`。
- [x] **E1.2**: 定义 3 个事件 payload 结构：`ShadowPulseParams`, `LightProfileBlendParams`, `MaterialPhaseShiftParams`。
- [x] **E1.3**: 定义 `tierPolicy` 枚举：`strict`, `degrade`, `skip`。
- [x] **E1.4**: 将 `vfx_schema_version` 升级为 `3`，定义 v2→v3 兼容策略。
- [x] **E1.5**: 添加 unit test：新类型枚举正确性、payload 默认值、tierPolicy 解析。

## Phase 2: 解析与校验

- [x] **E2.1**: 扩展 `VFXSequenceManager` JSON 解析器以识别 v3 事件类型和 payload。
- [x] **E2.2**: 实现 `tierPolicy` 字段校验（仅接受 `strict|degrade|skip`），非法值拒绝。
- [x] **E2.3**: 实现 payload 数值范围校验（如 `duration > 0`、`softnessScale >= 0`）。
- [x] **E2.4**: 实现未知事件类型安全处理：warning + skip（不崩溃）。
- [x] **E2.5**: 实现 v2 序列加载兼容层：无新字段时使用默认值。
- [x] **E2.6**: 添加 unit test：合法/非法 JSON 解析、兼容层、错误输出格式。

## Phase 3: Runtime Handler

- [x] **E3.1**: 实现 `ShadowPulse` handler：动态调整阴影 softness/intensity，带 duration 插值。
- [x] **E3.2**: 实现 `LightProfileBlend` handler：在两个光照配置间平滑过渡，带 blendTime 插值。
- [x] **E3.3**: 实现 `MaterialPhaseShift` handler：动态缩放材质 roughness/specular/emissive，带 duration 插值。
- [x] **E3.4**: 实现 `tierPolicy` 调度逻辑：
  - `strict`: Tier 不支持 → 输出 error 日志 + 中断事件链。
  - `degrade`: Tier 不支持 → 输出 warn 日志 + 使用降级近似效果。
  - `skip`: Tier 不支持 → 输出 info 日志 + 跳过本事件。
- [x] **E3.5**: 连接 handler 到 Shadow/Material/Light 系统接口（通过抽象接口，不直接依赖）。
- [x] **E3.6**: 添加 unit test：每个 handler 的插值行为、tierPolicy 各分支覆盖。

## Phase 4: 工具链交付

### 4A: 预算估计器

- [x] **E4A.1**: 创建 `VFXBudgetEstimator` 类，支持分析单个序列的成本构成。
- [x] **E4A.2**: 实现粒子成本估算（发射数 × 生命周期 × 计算复杂度因子）。
- [x] **E4A.3**: 实现灯光成本估算（动态光源数 × 光照范围因子）。
- [x] **E4A.4**: 实现阴影/材质事件成本估算。
- [x] **E4A.5**: 实现输出格式：JSON（机器可读）+ 控制台摘要（人类可读）。
- [x] **E4A.6**: 实现预算超限 warning 逻辑（可配置阈值）。

### 4B: 预览场景

- [x] **E4B.1**: 创建 VFX 预览场景框架：独立运行窗口 + 基础渲染循环。（CLI 版本框架）
- [x] **E4B.2**: 实现时间线可视化（事件在时间轴上的分布图）。
- [x] **E4B.3**: 实现热重载 diff：监控 VFX JSON 文件变更 → 差异检测 → 自动刷新预览。
- [x] **E4B.4**: 添加基础 UI 控件（播放/暂停/进度条/Tier 切换）。

## Phase 5: 模板序列创作

- [x] **E5.1**: 创作近战模板序列 ×3（斩击闪光、重击地震、连击节奏），含 tierPolicy 配置。
- [x] **E5.2**: 创作法术模板序列 ×3（火球爆炸、冰冻扩散、雷击链），含 tierPolicy 配置。
- [x] **E5.3**: 创作 AoE 模板序列 ×2（范围毒雾、神圣光柱），含 tierPolicy 配置。
- [x] **E5.4**: 创作召唤模板序列 ×2（暗影传送、元素凝聚），含 tierPolicy 配置。
- [x] **E5.5**: 创作环境交互模板序列 ×2（火把点燃、水面波纹扩散），含 tierPolicy 配置。
- [x] **E5.6**: 所有模板运行预算估计器，确认无预算超限。

## Phase 6: 测试矩阵与性能验证

- [x] **E6.1**: 添加 integration test：全 12 个模板序列加载 + 执行 + Tier 降级行为。
- [x] **E6.2**: 添加 integration test：v2 序列全量回归（现有 10 个 v2 序列不受影响）。
- [x] **E6.3**: 添加 integration test：预览工具热重载 diff 功能验证。
- [x] **E6.4**: 添加 performance test：10 个序列同时运行的帧时间波动（≤ 1ms）。
- [x] **E6.5**: 运行 `build.bat`、`build.bat analyze`、`build.bat perf`，修复回归。

## Acceptance Gates (DoD)

- [x] 3 种新事件确定性执行且版本安全。
- [x] tierPolicy 行为全覆盖且可复现。
- [x] 预算估计器输出 JSON + 人类摘要。
- [x] 预览工具支持时间线可视化 + 热重载 diff。
- [x] 12 个模板序列全部就位并通过降级测试。
- [x] v2 序列兼容性回归通过。
- [x] 10 并发序列帧时间波动 ≤ 1ms。
