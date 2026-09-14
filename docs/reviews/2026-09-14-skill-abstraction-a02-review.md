# 技能 10~12 专精抽象化复审报告（Track A-02）

- 审查目标：Track A-02 —— 技能 10（七星斩）/ 11（天剑降临）/ 12（血海）迁移到 `SpecStateTable` 统一信封，退役全部手写 `*PointBinding` / `*FlagBinding` / `*MechBinding` 表，达成全仓 D-A5 闭环与 D-A1 纯粹性，并关闭 L-2 遗留。
- 结论：`提交`
- 审查轮次：首次审查（最终通过）
- 输入：
  - 设计：`docs/designs/2026-09-14-skill-abstraction-track-a02-design.md`
  - 实施计划：`docs/plans/2026-09-14-skill-abstraction-track-a02-plan.md`
  - 审查标准：`docs/workflows/review.md`
  - 验证证据：见本文「验证证据」小节（构建、ctest、生成器门禁、grep）。

## 结论

`提交`。Track A-02 的 DoD 1~5 逐条达成；核心重构为行为等价迁移，60 个机制系数与 10 个兜底字面量经独立逐条比对与 HEAD 完全一致；硬否决项扫描无命中。

## 变更文件边界

`git status --short`（14 个已修改 + 2 个新增计划/设计文档）：

```
 M assets/data/skill_specstate/skill_10.json
 M src/game/systems/skill/SpecStateTable.hpp
 M src/game/systems/skill/behaviors/BloodSea.cpp
 M src/game/systems/skill/behaviors/BloodSea.hpp
 M src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp
 M src/game/systems/skill/behaviors/HeavenlySwordDescent.hpp
 M src/game/systems/skill/behaviors/SevenStarSlashShared.hpp
 M src/game/systems/skill/behaviors/generated/SevenStarSlashSpecState.gen.hpp
 M tests/functional/BloodSeaNodes.cpp
 M tests/functional/HeavenlySwordDescentNodes.cpp
 M tests/integration/GameplaySystems.cpp
 M tests/unit/GeneratedSpecStateTests.cpp
 M tests/unit/SpecStateMappingTests.cpp
 M tests/unit/SpecStateTableTests.cpp
?? docs/designs/2026-09-14-skill-abstraction-track-a02-design.md
?? docs/plans/2026-09-14-skill-abstraction-track-a02-plan.md
```

检视范围：上列全部 14 个变更文件；生产侧重点为 `SpecStateTable.hpp`、`SevenStarSlashShared.hpp`、`HeavenlySwordDescent.hpp/.cpp`、`BloodSea.hpp/.cpp`，测试侧重点为 `GeneratedSpecStateTests.cpp`、`SpecStateTableTests.cpp`、`SpecStateMappingTests.cpp`、`GameplaySystems.cpp` 与两个 functional 文件。变更规模 `523 insertions / 1014 deletions`（净减 491 行手写绑定样板）。

## 范围对齐

| DoD | 判定 | 证据 |
|---|---|---|
| 1. D-A5 闭环 | 达成 | `rg "struct (SevenStarSlash\|HeavenlySword\|BloodSea)(Point\|Flag\|Mech)?Binding" src/` = 0；`rg "kSevenStarSlashPointBindings\b\|kSevenStarSlashFlagBindings\b\|kHeavenlySwordMechBindings\|kBloodSeaMechBindings" src tests` = 0 |
| 2. D-A1 全仓统一 | 达成 | 全部 12 个 `*SpecState` 为仅含 `int` 点数字段 + `bool` 点亮标志的生成 POD；`SpecStateTable.hpp:53-55` `static_assert` 约束平凡复制 + 标准布局 |
| 3. L-2 关闭 | 达成 | `SpecStateTableTests.cpp` 新增 `CheckTableMatchesRuntime`，对 `kSevenStarSlashTableGen` / `kHeavenlySwordDescentTableGen` / `kBloodSeaTableGen` 逐绑定断言运行期 `ReadPoints` / `HasNode` 一致 |
| 4. 测试 100% 绿 | 达成（见剩余风险 R1） | `-L ci` 1/1、`-L skill` 2/2、`-L integration` 6/6、`-L unit` 8/8 全绿；全量 ctest 17/19，2 例失败为既有 GPU/性能抖动（非本 Track 代码路径） |
| 5. 文档闭环 | 达成 | 本报告结论「提交」；`docs/plans/2026-09-12-skill1-9-followup-backlog.md` 中 A-02 / A-03 标记销项 |

与计划的偏差（均已披露并复核）：

1. **计划外补漏——统一模板 clamp（设计 §2.5）**：计划 T1~T5 任务列表未列该项，但设计 §2.5 明文要求将 `ResolveSpecState` 模板的点读填表统一为 `std::max(0, ReadPoints(...))`，以结构等价技能 11/12 的既有 clamp。已在 `src/game/systems/skill/SpecStateTable.hpp:66` 落地并补充 `#include <algorithm>`；对技能 1~9 无可观测影响（写入侧恒非负），属设计授权的严格化。
2. **计划外补漏——`SpecStateMappingTests.cpp`**：计划 T5.1~T5.4 未列出该文件，但它引用了被删除的 `kSevenStarSlashPointBindings` / `kSevenStarSlashFlagBindings`。已将其重定向到 `*Gen` 表，保持「节点 id ↔ SpecState 成员」映射与 1021/1022 转质覆盖两处原意图，比较改为按 `binding.node` 顺序无关。

## 质量与风险评估

- **行为等价（首要风险）**：独立逐条比对确认 `HeavenlySwordDescent.cpp::DoCast` 的 20 个 `GetMech` 局部量与已删除 `kHeavenlySwordMechBindings`、`BloodSea.cpp::DoCast` 的 40 个局部量与已删除 `kBloodSeaMechBindings` 在 node/key/default 上完全一致（含技能 12 的 3 个 node=0 技能级键）；技能 11/12 各 5 个兜底常量（`90/140/5/0.18/14`、`4.8/120/0.25/0.12/0.12`）与 HEAD POD 缺省字面量逐字节一致。`spec` 仅余点数字段与点亮标志。
- **技能 10 转质语义**：`SevenStarSlashShared.hpp:266-276` 的 2 参包装先经 4 参模板填 17 点 + 8 flag，再无条件以 `SkillSystem::GetActiveTransmuterNode` 覆盖 `poleStarOrbit` / `starfall`，与 HEAD 语义一致；4 参调用不构成自递归。
- **硬否决扫描（`docs/workflows/review.md` §硬否决）**：无裸 `new`/`delete`（§5.2）、无 `reinterpret_cast`/C 式转换（§6.1）、无裸 `std::thread`/`detach`（§8.1）、无跨实体创建销毁持有 EnTT 组件指针（§5.3）、热路径未新增堆分配或字符串比较（§2.1/§7.2）。新增 `GetMech(..., "key", ...)` 字符串键在 `DoCast` 入口每次施法各读一次，非逐帧分支，且与迁移前手写绑定表的取数方式一致，设计显式授权。
- **测试真实性**：未出现 `CHECK(true)` 类空断言；被删除的断言限于计划 T5.4 授权范围（技能 11/12 functional 文件的机制字面量断言，机制回归由同文件 `GetMech` 数据用例与行为用例承担）。
- **重复轮子**：退役手写绑定结构后由生成头单源承接，未引入新 helper/组件/配置项，无等价实现重造。

## 发现项

1. **Low（已修复）** `src/game/systems/skill/behaviors/BloodSea.hpp:8-9`：别名化后 `#include <algorithm>` 与 `#include <array>` 已无使用（`std::array` 由生成头自带，`.cpp` 自行包含 `<algorithm>`），与 `HeavenlySwordDescent.hpp` 的处理不一致，违 `conductor/code_standard.md` 的包含卫生。已在本次复查中移除两行；复建 `build.bat RelWithDebInfo` EXIT 0，定向用例 43/43 通过。
2. **Low（残留）** `tests/functional/HeavenlySwordDescentNodes.cpp`、`tests/functional/BloodSeaNodes.cpp`：设计 §3.1 的原意是「断言改指 `GetMech`」，实现按计划 T5.4 做成了「删除机制/兜底断言」。已加载数据值的机制断言仍在（如 `BloodSeaNodes.cpp:211` 覆盖 `low_life_threshold`），但 `DoCast` 内 10 个 `constexpr` 兜底字面量（技能 11 `90/140/5/0.18/14`、技能 12 `4.8/120/0.25/0.12/0.12`）无直接断言；未来若误改兜底值，仅在缺数据/降级路径暴露。列入剩余风险 R2。

## 最佳实践建议

1. 针对上列 Low-2，补一个「键缺失 → `GetMech` 返回兜底字面量」的缺失键断言（每技能 5 个兜底常量各一条），或在计划/设计中显式登记覆盖转移；不阻塞本次提交。
2. 修复仓库 `.clang-format:78` 的 `Standard: Cpp20` 与本地 clang-format 22.1.3 不兼容问题（改为 `c++20` 或升级/固化工具版本），使格式化门禁可用（见 R3）。

## 剩余风险

1. **R1（接受）**：全量 `ctest` 有 2 例失败——`nmd.tests.performance`（`ParticleTrailBenchmark.cpp:205` 阈值 `dispatchOverheadMs < 0.2` 得 0.308；`RadianceCascadesBenchmark.cpp` holographic 中位数比阈值）与 `nmd.tests.gpu.hardware`（`GICompositePass exceeded GPU budget`）。二者均属粒子/GI 渲染基准与硬件预算门禁，与本 Track 代码路径无关；对同一过滤重复运行出现「一次全过、一次在**不同**断言行失败」，属负载相关抖动（与既有 O-01/O-03/O-07 同类）。`ci` / `skill` / `integration` / `unit` 标签全绿。
2. **R2（接受，建议跟进）**：见发现项 Low-2，10 个 `DoCast` 兜底字面量暂无直接断言。
3. **R3（接受）**：本次未执行 `clang-format`——仓库 `.clang-format:78` 的 `Standard: Cpp20` 被 clang-format 22.1.3 拒绝（`unknown enumerated scalar`）。属既有工具/配置不匹配，非本 Track 引入；变更按周边风格手工排版。
4. **R4（接受）**：既有告警基线未变（`PhantomTrance.cpp:275` C4834、`SpecStateMappingTests.cpp` / `SpecStateTableTests.cpp` 的 `CAPTURE` 多参 C4002，均与 HEAD 逐字节一致），非本 Track 引入，按 AGENTS.md「不修改无关内容」未处理。
5. **R5（范围声明）**：backlog 中 A-02 原条目括注「含 HeavenlySwordField / BloodSeaField 组件迁移（另立计划）」。本次销项仅覆盖本 Track 的 SpecState 抽象化范围；组件迁移仍按该「另立计划」独立跟踪，本报告不主张其完成。

## 验证证据

- 构建：`build.bat RelWithDebInfo` EXIT 0（首次全量；移除 `BloodSea.hpp` 冗余包含后增量复建亦 EXIT 0）。改动 TU 无新增告警。测试二进制 `bin/NoMoreDayTests.exe`。
- 定向用例（doctest）：`*BloodSea*` 3/20、`*SpecState*` 27/2391、`*Skill Node*` 1/360、`*Skill 10*` 6/197、`*Skill 11*` 5/213、`*Skill 12*` 7/292；复查合并过滤 `*BloodSea*,*SpecState*,*Skill 1*` 43 用例 / 2900 断言全过。
- 全量回归：`ctest --test-dir build -C RelWithDebInfo --output-on-failure` → 17/19；标签 `ci` 1/1、`skill` 2/2、`integration` 6/6、`unit` 8/8 全绿；失败 2 例为 R1。
- 生成器门禁（三条全 EXIT 0）：`gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism`（`[OK] skill_contract blocks are up to date.` / `[OK] SpecState artifacts unchanged.`）、`gen_skill_mechanics_schema.py --check`（472 entries, 80 unreferenced）、`sync_skill_node_icon_ids.py --check`（updated 0 / unchanged 255、76 / missing 0）。
- 结构闭包 grep：D-A5 与残留符号两条 `rg` 均为 0 命中（见「范围对齐」表）。
- 生成物确定性：仅 `SevenStarSlashSpecState.gen.hpp` 因 descriptor 变更重生成；其余生成头 `[OK] No changes required.`，`git status --short` 无计划外生成物改动。

## 下一步动作

提交。可选跟进项（不阻塞）：R2 补兜底字面量断言、R3 修复 `.clang-format` 配置。
