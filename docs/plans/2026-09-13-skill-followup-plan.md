# 技能专精收尾与设计修订 — 实施计划

- 日期：2026-09-13
- 设计：`docs/designs/2026-09-13-skill-followup-design.md`
- 基线：`0ef68d2d`（技能抽象化 + B2 清理，已提交）
- 清单对账：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（本计划执行期间同步勾选）
- 范围：技能1~12 收尾（非性能项全量）+ 4 项设计修订；A-01 抽象化另立 Track

## 1. 实施思路与原则

1. **先基础后行为**：契约/生成器/枚举等结构改动先做（Wave A），避免行为改动返工；设计修订（Wave B）先于依赖其数值的行为实现（Wave C）。
2. **单源优先**：每个数值/标签/角色只有一个权威来源；发现双源即收敛（技能10 系数、ArmorShred）。
3. **零兼容层**：无存档兼容需求时直接改数据/枚举，不保留旧字段别名。
4. **验证证据**：每 wave 附 `文件:行` 或测试名；出口 `build.bat` + `ctest` + 数据校验脚本。
5. **串行约束**：`SkillDefs.hpp`、`BladeFormation.cpp`、`SkillSystem.cpp`、`DamagePipeline.cpp`、`skill_mechanics.json`、`skills.json` 为高冲突文件，同 wave 内串行改动，不并行编译。

## 2. 对账后的状态表

> 下表为计划执行前的对账快照；执行完成后的终态见每行「终态」与 `docs/plans/2026-09-12-skill1-9-followup-backlog.md §9.4`。

### 2.1 B1（非性能）

| ID | 状态（对账时） | 剩余工作 | 终态 |
|---|---|---|---|
| B1-01 | OPEN | skill3 372 瞬移+静电场、375 引爆、331 溅射、355 剑阵元素附加 | CLOSED（C1.1-C1.4） |
| B1-02 | OPEN | skill4 476 曝光、472/474 数值、402/403/410/413/415/431/433/434/453/454/473/475 | CLOSED（C2.1-C2.3） |
| B1-03 | PARTIAL | 435 `crit_bonus`（`skill_mechanics.json:330`）无消费；455 已接线 | CLOSED（C2.4 + rd-rulings plan §4 B3） |
| B1-04 | PARTIAL | `counter_swords`（`:356` =5）无消费 → 5 道反击剑气实体未实现 | CLOSED（C2.5 + 470 键消费） |
| B1-05 | PARTIAL | 535/552/532/533 收窄外置；534/512 按 D8 | CLOSED（C3.1-C3.4） |
| B1-06 | OPEN | 并入设计修订 4.3 | CLOSED（RD-12：扇形洪流，无代码改动） |
| B1-07 | OPEN | `Buff.hpp` 增 `RemoveByKind(kind, source_skill_id)` 重载 | CLOSED（C4.1-C4.2） |
| B1-08~14,16~19 | CLOSED | — | CLOSED |
| B1-15/19 | CLOSED（backlog 过期） | 2026-09-13 计划 T6.4 已闭合，仅需勾选 | CLOSED（已勾选，见 backlog §9.4） |
| B1-20 | PARTIAL | 仅缺 `interception_chance=0.5` 骰子分布契约测试 | CLOSED（D1.1 + 模块化复审 §8，backlog 注记已订正） |
| B1-21/22/23 | BLOCKED→采证 | 现行测试环境或 headless | BLOCKED（无交互运行环境，见 backlog §2.3） |
| B1-24/26 | EXCLUDED | 性能 | EXCLUDED |
| B1-25 | CLOSED | CombatV2 已清空，仅确认 | CLOSED |

### 2.2 B2 与新增

| ID | 状态（对账时） | 剩余工作 | 终态 |
|---|---|---|---|
| B2-15 | PARTIAL | ArmorShred → `BuffId` 枚举；4~5 处字符串站点；统一 stacks 语义 | CLOSED（枚举/多层断言完成 + 351 数据消费统一，见 backlog §9.4） |
| B2-16 | OPEN(可选) | `SkillSpecializationBaker.cpp:1069-1074` 精确到 node 711 | CLOSED（C5.1 评估后拒绝，证据 `:1070-1079`） |
| B2-18 | PARTIAL | `ApplyFrostSlowDebuff`（`FlowingThrust.cpp:67-83`）走 ailment 契约 | CLOSED（方案 B：补注册 `Slow` + 单源构建口，3 护栏测试零改动，见 backlog §9.4） |
| B2-24 | CLOSED | — | CLOSED |
| 新-1 | OPEN | 技能10 系数双源（`getModifier` vs `GetMech`）+ `slash_count` 死键 | CLOSED（A2：`GetMech` 单源 + 死键删除） |
| 新-2 | OPEN | `SkillMechanicsRegistry` 键名拼写加载期校验 | CLOSED（R3：生成器 + 加载期校验） |
| 新-3 | OPEN | 测试隔离：`TestSetupScope` 未重置技能注册表；handler 非 RAII 泄漏 | CLOSED（D1.2） |
| 新-4 | OPEN | `SkillSystem.cpp` 973 on-hit 同步豁免理由过期（P2-P4 已完成） | CLOSED（D1.3） |
| 新-5 | OPEN | 文档口径：crit 单位旧措辞、backlog 勾选对账 | CLOSED（D1.4；backlog B1-01~07 漏勾于 2026-09-13 文档债批次补齐，见 backlog §9.4） |
| B2-23 | CLOSED(保留) | — | CLOSED |
| RD-18 | 裁决延后 | 不启动 | 延后 |

## 3. Wave A — 契约 / 数据 / 生成器基础

### A1. Keystone 默认推导修正 + skill1/2/3/10 契约重生成

- **原理**：`scripts/gen_skill_contracts.py:454-455` 存在兜底分支：技能**未配置** `keystone_node_ids` 时，所有 `max_points==1` 节点默认推导为 Keystone，导致 skill1/3/10 的普通单点节点（如 skill1 153 饮血刃）被误标。生成器没有「节点显式 role」输入源，修正方式为**删除该兜底分支**，并给需要 Keystone 的技能补显式 `keystone_node_ids` 配置（skill2/4/5/6/7/8/9/11/12 已配置，不受影响；skill2 251 现状 Passive 正确，无角色变更）。
- **伪代码**：
  ```
  # 删除 gen_skill_contracts.py:454-455 的兜底分支：
  #   elif not explicit_keystone_ids and max_points == 1:
  #       role = ROLE_KEYSTONE
  # 修正后角色仅来自显式集合：
  #   transmuter_ids / synergy_ids / explicit_keystone_ids /
  #   trigger_nodes / explicit_passive_ids → 对应角色，否则 Passive
  ```
- **任务**：
  - [x] A1.1 删除 `gen_skill_contracts.py:454-455` 兜底分支；`--check` 仍须通过（自洽）。
  - [x] A1.2 为 skill1/3/10 在 `skill_contracts_compact.json` 补显式 `keystone_node_ids`（按 GDD `<Keystone>` 节点），重生成契约；skill2 251 无角色变更。
  - [x] A1.3 出口校验 `gen_skill_contracts.py --check --check-idempotency --check-determinism`、`sync_skill_node_icon_ids.py --check`。
- **DoD**：skill1/3/10 契约无「单点即 Keystone」误标（skill2 因有显式列表本就正确）；`SkillContractRegistryTests` 的 keystone 精确集合断言通过。

### A2. 技能10 系数单源 + `slash_count`

- **原理**：`SevenStarSlash.cpp` 混用 `GetMech`（`skill_mechanics.json`）与 `getModifier`（`SkillRegistry` 的 `trigger.effectiveness/.range_mult`，源自 `skills.json`）。同一行为的系数应单源于机制表。
- **任务**：
  - [x] A2.1 将 `:438/447/547/557/562/566/574/581/614/645` 的 `getModifier` 系数迁移为 `GetMech(10, node, key)`；机制表补键，值等于原 trigger 系数。
  - [x] A2.2 `slash_count`（`skills.json:6358`）处置：`SevenStarSlash.cpp:444` 现为硬编码 `specState.starfall ? 4 : 7`，`slash_count` 是死键；将 `7` 改为读 `GetMech(10, ...,"slash_count", 7)` 或删键，二选一取证。
  - [x] A2.3 `SpecStateMappingTests` 数值等价回归。
- **DoD**：`SevenStarSlash.cpp` 生产路径 0 处 `getModifier`；`slash_count` 有消费者或已删。

### A3. B2-15 ArmorShred 枚举化

- **原理**：`"ArmorShred"` 字符串在 4~5 处比对/创建；且 RendingWave 与 FlowingThrust 的 stacks 编码不一致（`-10*stacks` vs 直接 stacks）。统一为 `BuffId::ArmorShred` 单一语义。
- **站点**：`FlowingThrust.cpp:523-524`、`RendingWave.cpp:537-538`、`BladeFormation.cpp:445-446`、`BladeBoomerang.cpp:363-364`（比较）/`:370`（创建）。
- **任务**：
  - [x] A3.1 `BuffIds.hpp` 增 `ArmorShred`（与 `SwordArrayArmorShred` 区分）。
  - [x] A3.2 比较改枚举；创建点统一 `.id`/`.stacks` 语义；删 TODO 注释。
  - [x] A3.3 回归技能1/2/3 相关测试。
- **DoD**：`rg '"ArmorShred"' src/` 为 0；三技能 armor-shred 效果等价或按统一语义修正并留断言。

### A4. 机制表键名加载期校验（新-2）

- **原理**：`SkillMechanicsRegistry` 仅校验结构与 skill id，键拼写错误静默回退默认值。增加每键的显式登记校验。
- **任务**：
  - [x] A4.1 生成/维护节点键名集合（schema），加载期对未知键 `LOG_WARN`（或严格模式报错，按现有注册表口径）。
  - [x] A4.2 测试：构造拼写错误键，断言被诊断。
- **DoD**：键改名漏同步可被加载期或 CI 检出。

## 4. Wave B — 设计修订落地

### B1. GDD 原文修订（直接改 `设计文档/职业设计草案_剑修.md`）

- [x] B1.1 skill3 §3.3 L228 标签去 `[Toggle]`；L229 基底改「限时召唤（25 法力/5s CD/3 柄/8s）」。
- [x] B1.2 skill1 §3.1 L156 回血 `100%`→`30%`。
- [x] B1.3 skill5 §3.5 L340 基底改扇形剑气洪流（去「屏幕随机天降」）。
- [x] B1.4 skill2 §3.2 L210 补「满层（10 层）额外重置流云刺冷却」；确认 251 契约（role=Passive + `affects_sword_intent=true`）与 HeavyMomentum 绑定登记。

### B2. 数据与实现落地

- [x] B2.1 skill3 灵剑持续时长：新增机制键/字段，`BladeFormation` 不再 `lifetime=-1` 常驻；重施刷新不叠数量。
- [x] B2.2 skill1 `skill_mechanics.json:14-16` `lifesteal_ratio` 1.0→0.30；`AilmentEngine.cpp:839-846` 默认值同步。
- [x] B2.3 skill5 `InfiniteBlades.cpp:127` 保持 BarrageEmitter 为唯一基底；`BeamChannelMode` 无 `Skyfall` 值（仅 `ContinuousLaser`/`BarrageEmitter`，`DeliveryArchetypes.hpp:153-156`），天降语义归 `SkyfallImpactDelivery` 原型（技能11/534），确认无残留引用即可。
- [x] B2.4 skill2 251 联动保留；`RendingWave.cpp:121` 删 TODO 并引 GDD 出处；`HeavyMomentum_Node213` 语义写入映射/设计文档。
- [x] B2.5 §11.8 终结帧口径在 `ProcEngine.cpp:59-65` 注释登记为已确认。
- [x] B2.6 数值口径（RD-04/05/09/15）在机制表/审查报告注释登记。

### B3. 测试

- [x] B3.1 skill3 灵剑到期消散 + 重施刷新断言。
- [x] B3.2 skill1 153 回血比例断言（30%）。
- [x] B3.3 skill5 基底单形态断言。
- [x] B3.4 skill2 满层重置流云刺 CD 断言（补 TODO 处的正向用例）。
- **DoD**：四项设计修订在 GDD/数据/实现/测试四处一致。

## 5. Wave C — 行为实现缺口

### C1. skill3（B1-01）

- [x] C1.1 **372 紫电紫雷**：瞬移攻击 + 静电场。**伪代码**：
  ```
  if specState.lightningForm:
      DoHit at target (teleport to cast pos)
      spawn StaticField{ radius = GetMech(3,372,"static_field_radius",60),
                         duration = GetMech(3,372,"static_field_duration",2) }
  ```
  消费 `skill_mechanics.json:254-257`；`BladeFormation.cpp:481-489` 的 Shock 硬编码改读节点门控。
- [x] C1.2 **375 灵剑充能**：引爆阈值读 `hits_required_base`/`hits_reduction_per_point`/`detonate_radius`（`:268-273`）。
- [x] C1.3 **331 弱点锁定**：`splash_radius`（`:205-208`）接入溅射判定（现仅 `bonusCrit` `BladeFormation.cpp:269`）。
- [x] C1.4 **355 剑阵元素附加**：`array_haste_pct`（`:244-246`）替换 `SummonAISystem.cpp:172` 硬编码 `1.50f`。
- **DoD**：四键无「0 消费」；功能测试覆盖。

### C2. skill4（B1-02/03/04）

- [x] C2.1 **476 元素曝光**：`Exposure=476` 接入命中→施加减抗 debuff，读 `exposure_pct_per_point`/`exposure_duration`（`:377-380`）。
- [x] C2.2 **472/474**：静电光环/冰龙卷数值键接入（`static_interval`、`static_radius`、`shock_*`、`frost_radius`、`knockback`）。
- [x] C2.3 **402/403/410/413/415/431/433/434/453/454/473/475**：逐节点实现，消费 `skill_mechanics.json:275-382` 对应键。
- [x] C2.4 **435 crit_bonus**（`skill_mechanics.json:330`）：消费为暴击加成；补断言。
- [x] C2.5 **`counter_swords`**（`:356`）：Melee 反击生成 5 道剑气实体（复用 `ResolveSkill4Counter` 归因）。
- **DoD**：13 节点键无 0 消费；技能4 功能/契约测试通过。

### C3. skill5（B1-05）

- [x] C3.1 535 击晕收窄至普通怪（`BeamChannelDeliverySystem.cpp:953-967` 加 Boss 排除）。
- [x] C3.2 552 落点改圆环（`:1114-1116`）。
- [x] C3.3 532 `:1058` `1000.0f`、533 `:1157` `70.0f` + `SkillSpecializationBaker.cpp:589` 外置机制键。
- [x] C3.4 534/512 按 D8 处置并断言。
- **DoD**：无硬编码魔法数；N12 收窄有测试。

### C4. skill8（B1-07）

- **伪代码**：
  ```
  void RemoveByKind(BuffKind k)                       // 现有
  void RemoveByKind(BuffKind k, uint32_t src_skill)   // 新增：仅移除 source_skill_id==src_skill 或 0
  ```
- [x] C4.1 `Buff.hpp` 增重载；`SkillSystem.cpp:1982-1988` 改调用（传 8u）。
- [x] C4.2 回归 FreeCast 用例。
- **DoD**：inline 过滤消除，单源。

### C5. 其它

- [x] C5.1 B2-16 skill7 714 精确到 node 711（可选，若低成本）。
  - [x] C5.2 B2-18 `ApplyFrostSlowDebuff`（`FlowingThrust.cpp:65-78`）统一走单源构建口 `AilmentAdapter::BuildMoveSpeedDebuff`。——已闭合（方案 B）：补注册 `AilmentType::Slow`（`ailment_contracts.json:+75-86`、`AilmentEngine.cpp:449-455`）+ 单源构建口；`id="FrostSlow"`/`type=SpeedDown`/`kind=Slow`/`{-30.0,MoveSpeed}` 逐字不变，3 条护栏用例零 diff。**订正原引用**：`FlowingThrustNodes.cpp:471` → `tests/functional/FlowingThrustNodes.cpp:470`、`SkillSpecializationBakerTests.cpp:899-904/:1313-1315`（原 `:777` 为父 TEST_CASE）。详见设计 `docs/designs/2026-09-13-skill1-slow-chill-ailment-contract-design.md`。
- **DoD**：`ApplyFrostSlowDebuff` 不再手工 `AddOrRefresh` legacy buff。

## 6. Wave D — 结构 / 测试 / 文档债务与运行时采证

- [x] D1.1 B1-20 `interception_chance=0.5` 骰子分布契约测试。
- [x] D1.2 新-3 测试隔离：`tests/TestCommon.hpp` `TestSetupScope` 增加技能注册表重置；`SkillBehaviors.cpp:610-635` handler 改 RAII。
- [x] D1.3 新-4 `SkillSystem.cpp:1010-1064` 973 on-hit：按现行管线重新论证豁免（快照已具备）或迁移 deferred queue；更新 `phase-P1/p1-report.md:97/:177` 过期理由。
- [x] D1.4 新-5 文档口径：skill1 审查 crit 旧措辞、backlog 勾选对账、B1-25 确认销项。
- [~] D1.5 运行时采证（B1-21/22/23、772、§11.9）：优先现行测试环境，失败转 headless；记录结果或未验证风险。——阻塞：当前无交互式运行时环境，仅自动化证据。
- **DoD**：backlog 全部非性能项勾选并附证据；ci 套件无已知 flaky。

## 7. Wave E — A-01 技能1~12 抽象化（独立 Track）

- 目标：技能1~12 行为层「触发/效果/交付」三层参数化，追求尽力而为的设计目标（不设 ≤100 行硬指标）。
- 流程：先出 `*-design.md`（重定 DoD）→ `*-plan.md` → 实施；`--gen-specstate` 生成器一并纳入。
- [ ] E1 设计重定 DoD 与分层边界。
- [ ] E2 迁移 skill1~9 行为；skill10~12 已信封化。
- **DoD**：以 E 的独立设计为准。

## 8. 测试方法

- 受影响层级：unit（契约/枚举/键校验）、integration（技能接线/隔离）、functional（节点机制）、manual/headless（运行时采证）。
- 新增回归点：Wave B3、C1~C5、D1.1/D1.2。
- 命令：
  ```
  build.bat
  ctest --test-dir build -C RelWithDebInfo -L ci
  ctest --test-dir build -C RelWithDebInfo -L integration
  ctest --test-dir build -C RelWithDebInfo -L skill
  python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism
  python scripts/sync_skill_node_icon_ids.py --check
  ```

## 9. 批次出口与验证

- 每 wave：`build.bat`（RelWithDebInfo，0 警告）+ 相关 `ctest` 子集。
- 每批：`ctest -L ci` 全绿 + 数据校验脚本 PASS；按 `docs/workflows/review.md` 出复审报告，结论 `提交` 后进入下一 wave。
- 全程禁止 `debug` 构建；不提交未经用户确认的 commit。
