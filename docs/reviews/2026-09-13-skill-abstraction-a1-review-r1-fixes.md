# A1 审查第一轮修复记录（2026-09-13）

- 输入审查：
  - `docs/reviews/2026-09-13-skill-abstraction-a1-review-r1-correctness.md`（结论：修改）
  - `docs/reviews/2026-09-13-skill-abstraction-a1-review-r1-completeness.md`（结论：修改）
- 修复范围：A1 工作区未提交变更（baseline `HEAD`）。

## 已修复

| 来源 | 发现 | 修复 |
|---|---|---|
| 完成性 M1 / DoD#8 | 全仓仍有 61 处 `allocated_points.(find\|contains\|count)` 直查（含 UI 层） | 全部迁移至 `skills::ReadPoints/HasNode`；含 UI/应用层（`GameUiSnapshotBuilder`、`UISkillSpecRenderer`、`UISkillTalentTree`、`InputSystem`、`GameplayState`）。现状：仅 helper 自身 `SkillPointAccess.hpp:16` 一处 `find`，DoD#8 达成。 |
| 正确性 L3 | `DamageInterceptors.hpp` 反向 include `SkillBehaviorBase.hpp`（战斗层依赖技能行为层） | helper 提炼至 foundation 头 `game/foundation/components/SkillPointAccess.hpp`；`SkillBehaviorBase.hpp` include 复用；`DamageInterceptors.hpp` 及其余消费点改引 foundation 头。 |
| 正确性 (SkillSystem `.at()`) / R1b-L1 | `SkillSystem.cpp` 用会抛的 `.at()` 读 `allocated_points`/`tree->nodes` | 改为 `ReadPoints` / `find` 迭代器安全取值。 |
| 完成性 M3 | `SkillBakerFlagConsumerTests` 名义称「每 flag 有消费点」但未断言 | 更名为 `SkillBakerFlagRegistry`，显式 `kKnownDeadFlagBits`（bit24/26/27/28）并断言死位/非死位一致性；文件头写明局限（消费点存在性由映射表 + rg 保证）。 |
| 完成性 M5 | 设计与实现的 missing-key 策略表述冲突 | 在 design §4.4 项 4 记录实施订正：加载期 schema 校验 + 运行期返回调用点语义默认值（非裸 0）；逐键必填列为 A2 增强。 |
| 完成性 M6 | 计划称 `--gen-specstate` 生成器挂 A1-1，但无实现 | 计划 §7 记录订正：生成器延后 A2-1，A1-6 T6.5 以手写镜像基线交付。 |
| 完成性（A1-3 DoD） | A1-3 DoD 复选框未勾 | 勾选 DoD#5 / 编译 0 警告 + rg 0 / 基线复跑（未验证延后）。 |
| 完成性 L4 | backlog 已完成项未勾 | A-04 / B1-10 已勾并附证据。 |

## 推迟至第二轮（死代码 / 双源 / 兼容回退）

| 来源 | 发现 | 计划 |
|---|---|---|
| 正确性 M2 | 技能5 写入 `BeamChannelComponent::channel_timer` 但从不读取（仅技能7 消费），注释失真 | 第二轮：删除技能5 死写点并修正注释（保留技能7 语义）。 |
| 正确性 M1 | `HeavenlySwordDescent` / `BladeMasteryService` 旧 `tick_interval` 写入迁移后由「死写」变为可能生效 | 第二轮：核验是否被 profile 重算覆盖；若仍死则删除，若生效则记录为有意行为修复。 |
| 完成性 M2 / DoD#3 | `BeamChannelDeliverySystem.cpp:164-167` Has7 双通道、`BladeFormation.cpp` flags/points 双源、`SkillSystem.cpp` bakedProfile/points 双源 | 第二轮：收敛为单源（B2-20 映射表断言）。 |
| 正确性 L1 | `BladeFormation.cpp` `fx.Get(BuffId)` 经 `Buff.hpp` 仍构造临时 `std::string` | 第二轮：为 `Buff.hpp` 增 `Get(std::string_view)` 重载。 |
| 正确性 L2 | `ResolveSkill4Counter` 保留 `35.0f` 兜底 | 记录：兜底等于设计值，非硬编码业务数值（DoD#1 名义达成）。 |
| 正确性 L4/L5 | cast_id 归一/ForEachChainCandidate 性能边界 | 记录为低危，暂不改动。 |

## 复验结果

- `build.bat` 全量重编译：**0 warning / 0 error**，EXIT=0。
- `NoMoreDayTests --test-case-exclude=*Performance*,*GPU-Diagnostic*`：连续 3 次 **1473 / 1473 通过**。
- `gen_skill_contracts.py --check --check-idempotency --check-determinism`：`[OK]`。
- `sync_skill_node_icon_ids.py --check`：0 updated / 0 missing。
- `rg allocated_points\.(find|contains|count) src/`：仅 helper 自身 1 处。

## 已知未验证 / 风险

- Tracy 性能基线、A1-3「观察一拍」：无运行环境，标注未验证待补。
- `SkillSystemTests.cpp` BladeWard 470 反击用例存在**既有**跨用例全局 Hook/单例污染偶发失败（该用例注释 `:1212` 自述）；本批复验 3 次未复现，独立运行通过；非本次改动引入。
- 第二轮将执行死代码/兼容回退/双源清理，随后仍需运行期环境补采基线与稳定性验证。
