# A1 审查第二轮报告：死代码与兼容回退清理（2026-09-13）

- 审查目标：A1 阶段（技能抽象与 B2 清理）死代码、双源、兼容回退、陈旧引用与热路径临时对象清理。
- 结论：**提交**（发现项已修复并复验；剩余项为已登记的低危/延后项）。
- 审查轮次：第二轮（第一轮：完成性与潜在问题）。
- 输入：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（DoD#1/2/3/6/8）、`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md` A1、`docs/reviews/2026-09-13-skill-abstraction-a1-review-r1-*.md`、`docs/reviews/2026-09-13-skill-abstraction-a1-review-r1-fixes.md`。
- 变更边界：A1 工作区未提交变更（`git status` 约 65 文件 + 新增 `src/game/foundation/components/SkillPointAccess.hpp`）。

## 发现项与处置

| 级别 | 发现 | 证据 | 处置 |
|---|---|---|---|
| 高 H1 | `BakedDeliveryParams::secondary_archetype` 生产侧只写不读（A1-2 删 `primary_archetype` 时漏删） | `rg secondary_archetype` 仅字段+3 处 Baker 写+1 测试断言 | **已删**字段 + 3 写点 + 镜像断言；`DeliveryArchetype` 枚举保留并注释为 taxonomy（`ReactiveWard=11` 按 RD-16 保留） |
| 中 M1 | `skill-baker-consumer-map.md` 与 `SkillBakerFlagConsumerTests.cpp` 仍描述已收敛的 `Has7` 双通道、行号漂移 | `BeamChannelDeliverySystem.cpp:166 HasNode7` vs 文档 `:268/:270/:272` 旧描述 | **已更新**文档 §3.7/表 B/表 C/§7.4 与技能7 相关 consumer 串；无技能7 死位产生 |
| 中 M2 | DoD#3 残留：多技能仍 `feature_flags` 优先、点数回退（category ① 节点语义） | `SwordArray.cpp:99-253`、`RendingWave.cpp`、`BladeWard.cpp:126-133`、`BladeFormation.cpp`、`SkillSystem.cpp:1949-1955`、`BeamChannelDeliverySystem.cpp:1091` | **登记为已接受的兼容回退**（经分析生产可达，非死代码）：`SkillSystem.cpp:1944-1948` 注释说明 UI 设槽不置 `StatsDirty`、未重烘路径可达；测试 `SwordArrayNodes.cpp:117` 覆盖未烘焙路径。技能7 双通道已收敛。 |
| 中 M3 | 技能5/7 `del.duration` 死写；技能5 引导上限硬编码 `5.0f` | `SkillSpecializationBaker.cpp` case5/case7 写 `duration`，无消费者 | **已修**：技能5 改 `skills::GetMech(5,0,"max_channel_time",5.0f)`，`skill_mechanics.json` 技能5 新增 node `"0".max_channel_time=5.0`；删除 case5/case7 死写。行为不变（5.0→5.0） |
| 中 M4 | `getPoints` 由「存在即返回」改 `ReadPoints>0`，而 `SpecializedSkill::from_json` 可加载 0/负点数条目 → 语义差异 | `SkillDefs.hpp:519-525` 无 clamp；`SaveManager` 走 `from_json` | **已修（根因）**：`from_json` 加载期剔除 `points<=0` 条目，使「0 点即不存」成为不变量、`ReadPoints>0` 与存在性等价；新增回归单测 `[Unit] SpecializedSkill - from_json prunes non-positive allocated points` |
| 低 L1 | `Buff::Remove(BuffId)`/`Has(BuffId)` 与 `BloodSea.cpp:127/161`、`SevenStarSlashShared.hpp:56` 仍构造临时 `std::string` | 热路径堆分配 | **已修**：`Remove` 改 `std::string_view`，去包装；`rg "Get\(std::string\(|Remove\(std::string\("` = 0 |
| 低 L2 | 无引用枚举常量 `BladeFormationNodes::ColossusRend` | `rg ColossusRend` 仅 1 处 | **已删** |
| 低 L3 | 其它硬编码数值 | `InfiniteBlades.cpp:130` `5.0f`（经 M3 外置）；`BeamChannelDeliverySystem.cpp:36 kMindBladeBaseDamage=30.0f`；Baker 字面量 | 技能5 已外置；其余为带契约/映射说明的既有现状，登记不阻塞 |
| 低 L4 | 技能6 夹具改独立 id 106 后端到端「技能6→AreaField」覆盖转移 | `DeliveryArchetypesTests.cpp:191/194/208` | 属有意解耦（验字段透传）；已确认映射另有覆盖，登记 |

## 新发现（登记，未在本轮改动）

- **`SkillRegistry::SanitizeLoadedSkillSlots` 无生产调用者**（`SkillRegistry.cpp:776`；仅测试直接调用）。其语义（复位未知技能槽）本应在存档加载后执行。该函数为既有孤儿；接入加载流程属行为变更、无运行环境验证，故本轮仅登记，建议后续单列任务并补运行时验证。
- `SkillBakerFlagConsumerTests` 的 consumer 串采用行号，随代码位移易漂移；建议改为「文件 + 位/符号」稳定锚点（本轮已校正技能7 相关条目）。

## 复验结果（串行）

- `build.bat`（RelWithDebInfo）：**0 warning / 0 error**，EXIT=0。
- `ctest -C RelWithDebInfo -L "ci|skill|unit|combat"`：**11/11**。
- `ctest -C RelWithDebInfo -LE "performance|gpu-hardware"`：**17/17**。
- `gen_skill_contracts.py --check --check-idempotency --check-determinism`：`[OK]`。
- 死符号清扫：`secondary_archetype`/`ColossusRend`/`ReactiveWardComponent`/`primary_archetype`/`ChannelingComponent` = 0；`allocated_points.(find|contains|count)` 仅 helper 自身 1 处；`channel_timer` 写点仅技能7（`SkillSystem.cpp:2114`、`MindBlade.cpp:30`）。

## 剩余风险 / 未验证

- Tracy 性能基线、A1-3「观察一拍」：无运行环境，标注未验证待补。
- 既有 order-dependent flaky：`SkillSystemTests` BladeWard 470 反击用例、`SkillBehaviors.cpp:635`（技能10 damageEvents）——单独运行通过、与 A1 改动无关；建议后续修复测试隔离。
- M2 的 category ① 双源为设计允许的兼容回退，若后续要彻底单源化（统一 `HasNode`、flags 仅留交付参数位），需单列任务并补回归。
- `SanitizeLoadedSkillSlots` 孤儿函数（见上）。

## 最佳实践建议

- B2-20 守护表 consumer 引用改用稳定符号锚点，避免行号漂移。
- 继续以「加载期校验 + 运行期不静默返 0」口径补齐 `skill_mechanics.json` 逐键清单（A2 增强）。

## 下一步

A1 两轮审查完成，进入 **A2 阶段**（技能 10/11/12 迁移），随后执行 A2 的第一轮（完成性/潜在问题）与第二轮（死代码/兼容回退）审查与修复。
