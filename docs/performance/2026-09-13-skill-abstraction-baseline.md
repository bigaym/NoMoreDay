# 技能抽象重构性能基线（A1-0 T0.2）

- 日期：2026-09-13
- 计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md` §A1-0
- 状态：**未验证（延后）**

## 处置说明

当前无可用交互运行环境，无法进入技能 1~9 施放场景并在 `%NMD_TRACY%` 下采集帧级数据。
按 `AGENTS.md`「无交互运行环境时的运行时观察项标注未验证」及 `docs/workflows/performance.md`，
本项登记为**未验证待办**，不阻断 A1 出口；A1-3/A1-4/A1-5 的性能对比同此处理，待运行环境可用后补采。

待办（后续补采时必须记录）：构建版本/哈希（`RelWithDebInfo`）、场景与输入、样本帧数、
捕获文件路径、帧时间 p95、`SkillSystem::Update` → 各 `behaviors/*::DoCast/DoTick/DoHit` 及
`DamageConditions`/`BladeFormation`/反击 helper/`BeamChannelDeliverySystem` 关键 zone 耗时占比。
原始 `.tracy` 不入库、不入记忆。

## 结构指标观测值（设计 §4.7 DoD#9 初始登记，仅观测不设阈值）

以下为 A1 启动前（2026-09-13）在 `main@dbda488d` 工作区的静态观测值，作为 A1-2/A1-3/A1-4/A1-5 拆除后的对比依据：

| 指标 | 命令 | 初始值 |
|---|---|---|
| 直接读点 `allocated_points.(find\|contains\|count)` | `rg -c "allocated_points\.(find\|contains\|count)" src/` | 75 |
| `ChannelingComponent` 引用 | `rg -c "ChannelingComponent" src/` | 33 |
| `primary_archetype` 引用（含 tests） | `rg -c "primary_archetype" src/ tests/` | 18 |
| `ReactiveWardComponent` 引用（含 tests） | `rg -c "ReactiveWardComponent" src/ tests/` | 17 |

上述统计口径为「文件命中计数」在计划 §A1-0 完成标准中的等价记录；精确到行的清单见各 wave 的任务分解。

## 可复现命令（待运行环境可用）

```powershell
build.bat
$env:NMD_TRACY  # 按 docs/workflows/performance.md 启动 Tracy 采集
# 进入技能 1~9 施放场景，采集 cast/tick 路径，导出帧时间与 zone 占比后回填本文件
```
