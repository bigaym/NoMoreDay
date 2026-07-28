# GPU JFA Incremental Update Closure 实施计划

> **Track ID**: `gpu_jfa_incremental_update_20260726`
> **依赖 Spec**: [spec.md](./spec.md)
> **状态**: [~] In Progress — default full JFA

---

## 实施思路/原理

局部 JFA 不能仅因某个 chunk 脏了就裁剪 dispatch。先用遮挡旧/新投影计算 dirty rect，按 GI 最大有效 SDF 影响半径扩张，检查 region 未触及无效边界且可取得 seed context。任一条件失败就走 full JFA。精度对照使用小分辨率精确 EDT 和 1080p deterministic full JFA/readback，不依赖视觉判断。

## 伪代码引导

```text
DecideUpdate(previous, current, viewKey):
  if viewKey changed or static content changed: return Full("view-or-static-change")
  dirty = Union(Project(previous), Project(current))
  expanded = Expand(dirty, MaxGiSdfInfluencePixels())
  if expanded.touchesBoundary or expanded.area > threshold: return Full("unsafe-region")
  if !HasValidSeedContext(expanded): return Full("missing-boundary-context")
  return Incremental(expanded)

ExecuteUpdate(decision):
  RunDeclaredSeedJfaDistance(decision.rect)
  if ScheduledReferenceError().exceedsContract:
    MarkInvalid(); RunFull("verification-fallback")
```

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
| --- | --- | --- | --- |
| 1 | Dirty 决策合同 | 投影、扩张、full fallback 原因 | [x] |
| 2 | 局部执行 | descriptor 驱动 rect dispatch/sync | [x] |
| 3 | 正确性防线 | EDT/full 对照、版本、累积误差防护 | [x] |
| 4 | 性能与上线 | GPU 基准、telemetry、默认策略 | [x] |

## 原子任务拆分

### Phase 1: Dirty 决策合同

- [x] Task 1.1: 记录动态遮挡旧/新 world bounds、投影结果、view key 版本。
- [x] Task 1.2: 定义 `MaxGiSdfInfluencePixels` 和各 tier/SDF resolution 扩张规则。
- [x] Task 1.3: 实现安全判定与结构化 full fallback reason。
- [x] Task 1.4: 为 resize、zoom、tier/render-scale、静态变化建立强制 full rebuild tests。

### Phase 2: 局部执行

- [x] Task 2.1: 在 compiled plan 声明 incremental seed/JFA/distance subresource、dispatch rect、lifetime。
- [x] Task 2.2: 实现含边界 context 的局部 seed、ping-pong JFA、distance dispatch。
- [x] Task 2.3: 让局部 image/SSBO transition 使用声明式 barrier，检查 registry 不重复增长。
- [x] Task 2.4: 输出更新模式、rect、dispatch texel、version、fallback reason。

### Phase 3: 正确性防线

- [x] Task 3.1: 添加随机动态遮挡 property tests，对比精确 EDT/full JFA。
- [x] Task 3.2: 添加 100 次移动/添加/删除，检查 max/RMS/P95 和 SDF sign。
- [x] Task 3.3: debug/hardware fixture 定期 readback；越界时同帧 full fallback 并保存 artifact。
- [x] Task 3.4: 验证 incremental/fallback 对 GI history、occluder/SDF version 的失效语义。

### Phase 4: 性能与上线

- [x] Task 4.1: 在 1080p 目标 GPU 采集同一小 dirty fixture 的 full/incremental Valid GPU P95。
- [x] Task 4.2: 验证 incremental P95 至少低 20%，full 路径仍满足既有预算。
- [x] Task 4.3: 未达安全/性能门槛保持 full 默认；达标后逐 tier 启用。
- [x] Task 4.4: 更新历史规格/progress/hardware baseline，消除测试开关等同增量的证据漂移。

## 测试方法

| 层级 | 覆盖内容 | 命令/证据 |
| --- | --- | --- |
| Unit | dirty rect、扩张、安全判定、fallback 分类 | `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` |
| Integration | compiled plan、版本、对照、100 次操作 | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| Performance | full/incremental 基准 | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| Hardware | 1080p Valid P95、SDF/GI readback | hardware Gate artifact + Track benchmark |

## 验证任务完成

- [x] 真实动态输入可走 incremental，不安全输入立即 full fallback。
- [x] 对照精度、连续稳定性、GI 输出和资源生命周期通过。
- [x] 性能收益由 Valid GPU query 证明，未达到 20% 不得完成。
- [x] 文档/telemetry 明确区分 full、incremental、fallback、skip。

## 集成审查整改

下方早期 `[x]` 仅保留为历史实施记录，不代表规格验收。M0-C 重新取得可信硬件基线前，production execution mode 必须强制为 full JFA。

```text
ExecuteJfa(decision):
  if !incrementalOptInOrQualified: return RunFull()
  RunIncremental(decision.rect)
  error = CompareCurrentOutput(DeterministicReference())
  if error > contract: return RunFullSameFrame("verification-fallback")
```

- [ ] R1: 以 production configuration gate 控制 execution mode，默认强制 full JFA；仅显式 opt-in 且正确性、稳定性、性能门槛均通过后允许 incremental。
- [ ] R2: 在 production frame 接入确定性 GPU/CPU reference comparison；任何误差超限当帧 full fallback，并将 mode、reason 和结果写入 artifact。
- [ ] R3: 为移动遮挡物、添加/删除、视图、resize、zoom、tier/render scale 场景增加集成回归，验证 fallback 与 GI version/history 失效。
- [ ] R4: 在同一 1920x1080 目标 GPU fixture 上提交 full/incremental GPU work，采集不重复 Valid timestamp P95，并断言 incremental `<= 80%` full；否则保持 full。

**退出标准**：R1-R4 的 unit/integration/performance 与 M0-C artifact 均通过。不能用面积比、空 timer frame 或 CPU 时间声明 20% 改善。
