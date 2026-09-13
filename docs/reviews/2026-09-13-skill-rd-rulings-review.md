# 技能 RD 裁决实施 —— 审查报告

## 审查目标

`docs/plans/2026-09-13-skill-rd-rulings-plan.md` 所定义的技能 RD 裁决实施包（RD-01/02/03/06/08/14 与驱动它们的 SkillRegistry/生成器改动）。

## 结论

`提交`

（首轮审查发现 2 项 High、1 项 Low 实现缺陷，已派子代理修复并独立复审通过；第二轮就 RD-06 的 455 AoE 范围补裁并实现 Wave D，复审又发现并修复 1 项边界缺陷。剩余风险见文末，均非阻塞项。）

## 审查轮次

首次审查 + 修复后复审 + 补裁 Wave D 复审（同一报告内记录）。

## 输入

- 设计：`docs/designs/2026-09-13-skill-rd-rulings-design.md`
- 计划：`docs/plans/2026-09-13-skill-rd-rulings-plan.md`
- 审查标准：`docs/workflows/review.md`、`conductor/code_standard.md`
- 基线：`31cc67e6`
- 验证证据：
  - `build.bat`（RelWithDebInfo，含 worktree mapping / legacy reintroduction / module boundaries / render ABI / validate_json 预检）→ exit 0。
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → exit 0，`100% tests passed, 0 tests failed out of 1`，15.66s。
  - `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` → exit 0，`[OK] skill_contract blocks are up to date.`
  - `python scripts/sync_skill_node_icon_ids.py --check` → exit 0，`Nodes updated: 0`、`Nodes missing icon files: 0`。
  - 定向回归：`NoMoreDayTests.exe -tc=*455 Offensive Guard Ignores*,*additive merge updates source_skill_id*` → `2 passed | 0 failed`，`21 assertions passed`。

## 变更文件边界

`git status --short`（23 个已修改文件 + 2 个未跟踪文档，887 insertions / 204 deletions）：

- 源码：`src/game/foundation/data/SkillRegistry.cpp`、`src/game/systems/combat/AilmentEngine.{hpp,cpp}`、`src/game/systems/combat/DamagePipeline.cpp`、`src/game/systems/skill/SkillSystem.cpp`、`src/game/systems/skill/behaviors/{FlowingThrust,PhantomTrance}.cpp`
- 数据/脚本：`assets/data/{mastery_skill_trees,skill_8_tree,skill_contracts_compact,skill_mechanics,skill_mechanics_schema,skills}.json`、`scripts/gen_skill_contracts.py`
- 测试：`tests/unit/{AilmentEngineTests,ProjectileTests}.cpp`、`tests/integration/{Skill1FollowupTests,SkillContractRegistryTests}.cpp`、`tests/functional/{FlowingThrustNodes,PhantomTranceNodes,Skill4FollowupTests}.cpp`
- 文档：`设计文档/职业设计草案_剑修.md`、`docs/plans/2026-09-12-skill1-9-followup-backlog.md`
- 新增（未跟踪）：`docs/designs/2026-09-13-skill-rd-rulings-design.md`、`docs/plans/2026-09-13-skill-rd-rulings-plan.md`

## 范围对齐

- 12 棵树的 keystone 分组与计划映射表逐项一致，权威源为 `skill_contracts_compact.json`，内嵌契约由 `gen_skill_contracts.py` 物化，符合“不得手改内嵌契约/树节点”的约束。
- 组内“≥2”校验在 `SkillRegistry.cpp` 与生成器中同步删除，`node_id: 0` 作为“无前置”既有惯例（数据中已有 37 处），`SkillSystem.cpp`、`UISkillTalentTree.cpp`、`UISkillSpecRenderer.cpp`、`SkillRegistry.cpp` 消费端均正确跳过/通过。
- RD-02/03/06/08/14 的行为与设计一致；455 的 20%/2s 取自 `skill_mechanics.json`，schema 已重生成。
- 未发现范围泄漏、存档迁移、确定性破坏或禁用消费者被触及。

## 质量与风险评估

- 代码检视基于实际上下文，非仅凭测试结论。455 倍率对 `total_final_damage` 与 `final_pool.values[i]` 各乘一次，无重复计入。
- 首轮审查在热路径（战斗结算）发现 2 项 High 正确性缺陷，均已修复并有非平凡回归测试锁定。
- 数据生成器幂等/确定性/图标同步三连校验通过。

## 发现项

### High — 已修复：RD-08 流血合并后来源技能归属陈旧

- 位置：`src/game/systems/combat/AilmentEngine.cpp:660`、`:701`（修复前）。
- 观测：Bleed 契约为 `max_stacks=2 / Independent / Additive`；Additive 合并分支仅同步 `slot.source`，通用合并路径仅同步 `effect.source`，二者都未更新 `source_skill_id`。
- 为何成问题：153 饮血刃回血门控读取 `effect.source_skill_id == 1`（`AilmentEngine.cpp:838-841`）。跨来源叠加后，非流云刺流血可能因陈旧 `==1` 误回血，流云刺流血也可能因陈旧 `0` 漏回血，违反 RD-08 的 DoD。
- 修复：两处合并路径在同步 `source` 的同时写入 `request.source_skill_id`；保强分支（保留原来源语义）未改动。
- 回归测试：`tests/unit/AilmentEngineTests.cpp:250` `[Unit] AilmentEngine - additive merge updates source_skill_id`（旧实现得到 `{1,0}`，修复后断言 `{1,999}`）。

### High — 已修复：RD-06 的 455 标记被非攻击伤害消耗

- 位置：`src/game/systems/combat/DamagePipeline.cpp:811-816`（修复前门控仅排除模拟与 DoT tag）。
- 观测：门控基于 `request.attacker` 的标记。荆棘反伤 `CombatSystem.cpp:250-257`（`origin=ThornsReflect`，`attacker=带荆棘者`）与地面危险区/持续场 `HazardSystem.cpp:421-428`、`BloodSea.cpp:254`、`BladeWard.cpp:511`（`origin=HazardEnvironment`）都会误消耗玩家的“下一次攻击”标记。
- 为何成问题：设计语义是“下一次攻击”，反伤/环境持续场并非玩家攻击；标记被静默吃掉会使 RD-06 随机失效且不可复现。
- 修复：门控追加排除 `ThornsReflect`、`HazardEnvironment`、`AilmentTick`（黑名单而非白名单，保留 `DirectSkillCast` 与 `SecondaryProc` 等投射物/飞剑攻击的消费能力）。
- 回归测试：`tests/functional/Skill4FollowupTests.cpp:486` `[Functional] Skill 4 - 455 Offensive Guard Ignores Non-Attack Damage`。

### Low — 已修复：节点 815 说明文案漂移

- 位置：`assets/data/skills.json:5098`。
- 观测：前置已改为 `node_id: 0`，但 `desc_key` 仍以 `*{需求: 滞空切割}* ` 开头。
- 修复：仅删除陈旧前缀，其余文案不动。

### 第二轮复审（补裁 Wave D：455 一次攻击行为范围）

- 触发：首轮剩余风险 1（455 对 AoE 仅首个实例加成）。设计 §3.4 已补裁「一次攻击行为=一次施法或一次普攻挥击，其全部伤害实例共享 +20%」，计划新增 §10 Wave D。
- 实现落点：`DamageRequest::attack_key`（`src/game/contracts/DamagePipelineTypes.hpp:50-53`）、`BuffEffect::offensive_guard_consumed`/`offensive_guard_attack_key`（`src/game/foundation/components/Buff.hpp:128-133`，运行期字段、不序列化）、消费逻辑改为按攻击标识聚合（`src/game/systems/combat/DamagePipeline.cpp:808-846`）、普攻挥击标识（`src/game/systems/combat/CombatSystem.cpp:151-154` / `:211`）。
- 复审发现并修复的缺陷（Medium）：`ActiveEffectsComponent::AddOrRefresh` 同 id 刷新分支未复位新增瞬态字段，导致 2s 窗口内再次闪避后残留 `consumed=true` 吞掉新加成；已在 `Buff.hpp:212-215` 补同步，并加回归用例 `[Functional] Skill 4 - 455 Refresh Resets Consumed State`（`tests/functional/Skill4FollowupTests.cpp:597`）。已用「回退修复 → 用例失败（170 vs Approx(204)）→ 恢复 → 通过」验证用例有效性。
- 验证证据：定向 `NoMoreDayTests.exe "-tc=[Functional] Skill 4 - 455*"` → `5 passed | 0 failed`、`48 assertions`；`ctest -C RelWithDebInfo -L ci` → `100% tests passed, 0 failed`（16.50s）；两条数据脚本 PASS。
- 残余限制：`source_entity` 不带 `cast_id` 的 AoE（如 skill5 万剑归宗场地经 `AreaFieldComponent`、owner-only 路径）仍退回逐实例语义（`attack_key=0`）；已在计划 §10 D4 登记，本次未扩大范围。

## 最佳实践建议

1. RD-08 的 DoT 归属建议后续用“来源技能 ID + 施加者”联合键表达归属，避免仅凭单一 `source_skill_id` 在极端叠加下的歧义；当前修复已覆盖已知合并路径。
2. 455 的消费门控后续若新增伤害来源（如装备词缀触发），应同步评估是否属于“攻击”，避免再次误消耗。
3. 剩余 2 条既有编译警告（`PhantomTrance.cpp:257`、`SwordArrayNodes.cpp:40`）可另行清理。

## 剩余风险

1. **已解决（第二轮补裁 Wave D）**：455 的 AoE/多目标语义已由设计 §3.4 补裁定为「一次攻击行为共享 +20%」并实现，`attack_key` 聚合 + 回归用例覆盖；`source_entity` 无 `cast_id` 的 AoE 路径仍走逐实例兜底（计划 §10 D4 登记）。
2. **Low**：GDD `设计文档/职业设计草案_剑修.md` 的血潮奔流/血环噬身前置文案 `污秽侵蚀 2/4` 与数据（node1220 1 点）漂移，属计划 §9 已列未决项。
3. **接受的行为变化**：以 `HazardEnvironment` 结算的玩家技能场（血海、剑阵持续场等）不再消耗也不再享受 455 标记，这是“仅真正攻击消耗”的必然结果。
4. **Low**：2 条既有编译警告（非本次改动行段）。

## 下一步动作

- 本次实现范围（含补裁 Wave D）可提交。
- GDD 前置文案（剩余风险 2）可在后续文档修订中处理。
