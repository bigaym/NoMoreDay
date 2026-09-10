# 技能 6 专精树（剑阵·诛仙）节点实现完全性审查 —— 深度复核与增补版（第 2 轮）

- **日期**: 2026-09-10
- **审查人**: C++ 代码审查（code-reviewer）
- **审查对象**: 技能 6「剑阵·诛仙」28 个专精节点 vs `设计文档/职业设计草案_剑修.md` §3.6 及全工程交互链路
- **结论**: **修改**（Blocker 级问题 3 项、High 级 7 项、Medium 级 10 项、Low 级 8 项）
- **轮次**: 第 2 轮（在第 1 轮基础上完成全量代码/数据/测试复核、纠偏与增补）

---

## 1. 审查目标

对照设计 `设计文档/职业设计草案_剑修.md` §3.6，逐节点核对技能 6 专精树在**数据层（skills.json / skill_contracts_compact.json / skill_6_tree.json / skill_mechanics.json）、烘焙层（SkillSpecializationBaker）、行为与交付层（SwordArray.cpp / AreaFieldDeliverySystem）、跨技能联动层（BeamChannelDeliverySystem / SummonAISystem / BladeResourceService）、消费层（DamagePipeline / StatsSystem / ProcEngine）、视觉层（VisualFXSystem）以及测试层**的实现完全性、拓扑正确性与一致性。

## 2. 结论

| 级别 | 第 1 轮数量 | 第 2 轮调整后数量 | 变动说明 |
|------|------------|-------------------|----------|
| Blocker | 3 | 3 | 维持 C1、C2、C3 |
| High | 5 | 7 | 增补 H5（视觉淡入硬编码致初段隐形）、H6（跨技能 5 联动暂停因双寿命失效） |
| Medium | 6 | 10 | 增补 M7（技能 3 联动阵眼判定锚点错误）、M8（675 前置错误指向 674）、M9（BuffIds 死代码）、M10（缺失独立功能测试套件） |
| Low | 5 | 8 | 增补 L6（653 契约遗漏）、L7（ResolveDamage 返回值丢弃）、L8（转质未联动特效色彩）；纠正 L5 文件路径 |

**总体判断**: 
实现完全性极低且存在多处严重跨系统级缺陷。28 个节点中**仅 4 个（630/631/633/652）有任何代码路径**，且其中 2 个（631 破甲、633 处决）核心机制字段**全工程无消费点**；**9 个节点因前置链锁死永久不可达**（含已实现但因此废弃的 652）；基础数值（法力/冷却/持续/脉动间隔）与设计全面偏离；契约数据角色错乱导致真转质节点 672 无法激活、633 绝命法场误触发裂空斩；**外部技能联动严重受损**（技能 5 的 515 暂停剑阵机制因双寿命权威在运行时静默失效，技能 3 的 355 阵内攻速加成因锚点错误导致判定反常）；**视觉层错误硬编码 5s 寿命导致剑阵在前 1~3 秒完全隐形**。测试矩阵将错误关键节点集固化为预期，无法暴露上述缺失。

### 第 1 轮事实性纠偏说明
1. **L5 测试文件路径纠正**：第 1 轮报告将 Skyfall 残留域夹具复用测试记为 `DeliveryArchetypesTests.cpp:190-210`，经查证其实际路径为 `tests/integration/DeliveryArchetypesTests.cpp`。
2. **675 前置需求设计核对纠正**：第 1 轮表 8 将 675 的「设计 prereq」误记为 `674`，经对照设计 §3.6 行 446 原文（`{需求: 任意 Transmuter}`），675 的设计前置应为**任意转质节点**（`[670×1, 672×1] OR`），现有数据层直接配置为 `674×1` 属于数据层拓扑错误，现予以纠偏并归入 M8。
3. **653 契约状态纠偏**：第 1 轮表 8 标记 634、635 为「契约缺」，但 653 随身剑垒在 `skills.json` 与 compact 契约中**同样完全缺失**，已补齐标注。
4. **610 / 612 角色语义核实**：设计 §3.6 关键节点清单（行 399-404）明确列出的 Keystone 仅为 4 个（`千丝万缕 613`、`绝命法场 633`、`剑阵牢笼 634`、`随身剑垒 653`）。数据层将 601、610、612、672、674 均标为 Keystone，除 672 应为 Transmuter、674 应为 Passive 外，610 在设计中为 `(0/1)` 机制节点、612 为 `(0/3)` 普通被动，均不属于设计关键 Keystone 范畴。

## 3. 输入

- `设计文档/职业设计草案_剑修.md` §3.6（行 394-446）＋交叉引用（行 234/270/363/666-669）
- `assets/data/skills.json` 技能 6（基础数据 + 内联 talent_tree 28 节点 + 内联 skill_contract 12 节点）
- `assets/data/skill_contracts_compact.json` 技能 6
- `assets/data/skill_6_tree.json`（28 节点坐标/前置）
- `assets/data/skill_mechanics.json`（顶层键配置）
- `src/game/systems/skill/behaviors/SwordArray.cpp`（85 行全文）、`SwordArray.hpp`
- `src/game/systems/skill/SkillSpecializationBaker.cpp`（case 6：85-90、644-655；通用 Trigger：127-143；装备修饰：161-198）
- `src/game/systems/skill/AreaFieldDeliverySystem.cpp`（193 行全文）
- `src/game/foundation/components/SkillDefs.hpp`（SwordArrayComponent 1079-1098、AreaFieldComponent 1103-1121）
- `src/game/systems/skill/SkillSystem.cpp`（Update 派发 1219-1224、转质运行时 1969-1978、前置 OR 2055-2074）
- `src/game/contracts/impl/StatsSystem.cpp`（通用 stat_modifiers 管道 445-491）
- `src/game/systems/combat/DamagePipeline.cpp`（归因 245/435，伤害结算 1320-1360）
- `src/game/systems/combat/VisualFXSystem.cpp`（剑阵视觉表现 189-236）
- `src/game/systems/skill/BeamChannelDeliverySystem.cpp`（技能 5 联动 222-236）
- `src/game/systems/skill/SummonAISystem.cpp`（技能 3 灵剑联动 162-176）
- `src/game/foundation/data/BuffIds.hpp`（BuffId 枚举 28-29、49-50）
- 测试：`tests/SkillKeyNodeMatrixTestHelpers.hpp`、`tests/fixtures/skill_specialization_keynodes.json`、`tests/integration/SkillSystemTests.cpp`（1351-1399）、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`（337-349）、`tests/unit/SkillBehaviorGuardTests.cpp`（424-450）、`tests/functional/SkillBehaviors.cpp`（959-981）、`tests/integration/GameplaySystems.cpp`（250-282）、`tests/integration/DeliveryArchetypesTests.cpp`（190-210）、`tests/functional/InfiniteBladesNodes.cpp`（528-555）、`tests/functional/BladeFormationNodes.cpp`（723-736）
- 前序审查：`docs/reviews/2026-09-08-skill3-specialization-nodes-review.md`、`docs/reviews/2026-09-09-skill5-specialization-nodes-review.md`

## 4. 变更文件边界

`git status --short`：**空**（工作区干净，本次为纯审查，无代码变更）。

## 5. 范围对齐

| 层 | 设计要求 | 现状 | 判定 |
|----|---------|------|------|
| 基础技能 | 持续 5s / 0.5s 脉动 / 法力 30 / 冷却 4s / 最大 1 阵 / Tags[Spell,Area,Physical,Duration] | 持续 6.0s / 0.3s / 法力 50 / 冷却 1.0s / 无数量上限校验 / Tags 缺 Duration 多 Hit | ❌ |
| 数据层 | 28 节点齐全、max/prereq/角色符合设计 | 22/28 max 错误、多处 prereq 数值错、675 前置错、角色大面积错乱、634/635/653 契约缺失 | ❌ |
| 烘焙层 | 全部 28 节点可烘焙出形态/数值变化 | 仅 4 节点有 case 分支，670/672 转质零烘焙 | ❌ |
| 行为与交付层 | 多阵/连线/处决/牢笼/阵眼/随身光环/转质等 24 项机制 | 仅缓速载荷、剑意回复方向性实现，核心处决/破甲无消费 | ❌ |
| 外部联动层 | 技能 5（万剑归阵暂停）、技能 3（灵剑攻速加成） | 技能 5 暂停因双寿命失效；技能 3 距离计算锚点错误 | ❌ |
| 视觉表现层 | 元素色彩联动、正确淡入淡出、牢笼剑墙 | 硬编码 5s 导致 6s/加成下初段完全隐形；元素色彩恒定紫色 | ❌ |
| 测试层 | 覆盖关键节点端到端行为 | 缺失独立 `SwordArrayNodes.cpp`；矩阵断言错误节点集与未消费默认值 | ❌ |

## 6. 质量与风险评估

1. **功能失效与阻断**：
   - 玩家若点出 633，因契约误标为 Trigger，导致领域伤害脉冲每跳命中即自动触发裂空斩（每 3 秒施放技能 2），造成超模且荒谬的玩法缺陷。
   - 玩家分配 652 意念合一（已写代码），但因为前置 601 max=1，真实加点流程**永久不可达**。
   - 技能 5（万剑归宗）点出 515（万剑归阵）后，引导轰击剑阵时试图通过累加 `array.duration` 暂停剑阵，但在 `AreaFieldDeliverySystem` 中 `field.remaining_duration` 照常递减，到期即被销毁，使该联动机制成为**摆设**。
2. **视觉渲染体验缺陷**：
   - `VisualFXSystem` 硬编码 5.0f 寿命淡入，在现行 6.0f 基础持续时间下，剑阵刚施放的前 1.0 秒 Alpha 恒为 0，完全隐形；若点出 600（持续时间提升），隐形时间最长可达 3.0 秒。
3. **架构与单一权威违背**：
   - 剑阵实体由 `SwordArrayComponent` 和 `AreaFieldComponent` 同时管理寿命与计时，出现状态双权威与节拍漂移，是导致跨技能联动失效的核心根因。

## 7. 发现项

### Blocker

**C1 — 24/28 节点零实现，已实现 4 节点中 2 个核心机制无消费点**
- 全工程仅 `SkillSpecializationBaker.cpp:644-655`（630/631/633/652 → feature_flags 位 1/2/4/8）与 `SwordArray.cpp:19-22,46-49`（flags → 组件字段）涉及技能 6 节点 ID；`600/601/602/603/610/611/612/613/614/615/632/634/635/650/651/653/654/655/670/671/672/673/674/675` 无任何烘焙/行为分支（`SkillNodeAssetRegistry.hpp` 仅为图标注册）。
- `SwordArrayComponent.has_armor_shred`（631）写入于 `SwordArray.cpp:47`，**无任何读取点**；`has_execute`（633）与 `execute_health_threshold_ratio`/`execute_damage_max_health_ratio`（`SkillDefs.hpp:1096-1097`）写入于 `SwordArray.cpp:48`，**无任何读取点**（`DamagePipeline.cpp:245,435` 仅为 cast_id/attacker 归因管道）。即：**破甲剑意（Armor Shred 50..200% 几率）与绝命法场处决（<12% 直接斩杀）完全未实现**。
- 整改：按设计逐节点补 Baker case 6 分支 + 行为层机制（建议先补 633 处决/602 极刑/610 多阵等骨架）。

**C2 — 633 绝命法场误标 Trigger：分配后每次领域脉冲命中自动触发裂空斩**
- 内联契约（skills.json 技能 6）与 compact 契约均将 `633` 标为 `Trigger`，携带 `{trigger_skill_id:2, effectiveness:0.5, internal_cooldown:3.0, 不消耗法力}`——这本是 **635 阵斩回响** 的参数（设计行 428），且事件应为"绝命法场**处决敌人时**"，不是 OnSkillHit。
- `SkillSpecializationBaker.cpp:127-143` 通用 Trigger 分支对 role==Trigger 的节点写入 `TriggerRule`（`trigger_skill_id!=0` → `listen_event=OnSkillHit`、`target=Victim`）。结果：玩家分配 633 后，**每次领域脉冲伤害命中（OnSkillHit）都会以 50% 效力施放技能 2（裂空斩），3 秒 ICD**。而 635 本体在契约与代码中完全缺失。
- 整改：633 角色 → Keystone；新增 635 Trigger 契约（事件应为处决/OnExecute 类事件，需与处决实现同步补齐）；compact 契约 `trigger_nodes` 同步改 `[{node_id:635,...}]`；`ExpectedKeyNodesBySkill()` 技能 6 集合同步修正。

**C3 — 9 节点因 max_points 错误永久不可达（含已实现的 652）**
- 前置校验为 OR 语义（`SkillSystem.cpp:2055-2074`），但数值型需求 "2/4" 需要目标节点 max≥2：
  - `601 虚空法网 max=1`（设计 0/4）→ 需 601×3 的 `614`、需 601×2 的 `615`/`650` 及其下游 `651`/`652`/`653`/`654`/`655` 全部不可达；
  - `612 剑气共鸣 max=1`（设计 0/3）→ 需 612×2 的 `613` 不可达。
  - 不可达集：`{613, 614, 615, 650, 651, 652, 653, 654, 655}`，共 9 节点。
- 特别注意：**652 意念合一（剑意回复）虽已实现，但在真实分配流程中永远点不出来**，仅测试用 `exec.active_nodes` 兜底路径能激活。
- 整改：按设计修正 max_points（601→4、612→3），并修正 `611/612` 的数值互换（见 H1）。

### High

**H1 — 契约角色大面积错乱（inline 与 compact 同错）**
| 节点 | 现角色 | 应角色（设计 §3.6 关键节点清单 行 399-404） |
|------|--------|------------------------------------------|
| 601 虚空法网 | Keystone | Passive |
| 610 双生剑阵 | Keystone | Passive（设计非 Keystone 关键节点） |
| 611 三才阵 | Passive+TypeB_Shred | Passive（**无削抗**；compact `resist_models{611:TypeB_Shred}` 应删除） |
| 612 剑气共鸣 | Keystone | Passive |
| 630 迟缓剑压 | Synergy（compact `synergy_node_ids=[630]`） | Passive（设计 Synergy=**614 流云穿阵**，行 401） |
| 633 绝命法场 | Trigger | Keystone（见 C2） |
| 634 剑阵牢笼 | 缺失 | Keystone |
| 635 阵斩回响 | 缺失 | Trigger |
| 653 随身剑垒 | 缺失 | Keystone |
| 671 炼狱余火 | Transmuter | Passive |
| 672 九幽雷池 | Keystone | **Transmuter** |
| 674 法阵侵蚀 | Keystone | Passive + TypeB_Shred（compact `resist_models`/`scope_policies` 挂在 672，应移到 674） |
- 后果：运行时转质机制（`SkillSystem.cpp:1969-1978` 施法时取 `s_allocated_transmuters.front()` → `StatsSystem.cpp:469-479` 按契约 role==Transmuter 门控）**永远无法把 672 识别为转质节点**；设计「火/雷互斥、max_transmuters=1」被 `max_transmuters=2 + transmuter_node_ids=[670,671]` 双双破坏（671 根本不该是转质）。

**H2 — 元素转质与设计脱钩（技能 3 审查 H3 同族缺陷）**
- `SwordArray.cpp:51-57`：阵实体元素转换来自 `BladeResourceService::GetHeavenlyAttunementElementTag`（`BladeResourceService.cpp:359`，技能 3 天剑认证），物理 50% 转认证元素——与 670/672 毫无关系。
- 670/672 无 Baker 分支 → `effective_tags` 恒为 Physical；671/672 的转质/落雷机制全部缺失。
- 测试 `SkillBehaviors.cpp:959-981`（"Sword Array field carries 50 percent fire conversion"）实际验证的是技能 3 认证耦合——**测试固化了错误设计**。
- 整改：补 670/672 Baker 分支（tag 转换 + 672 改落雷节律 1s 单体 2-3 目标）；阵实体转质从运行时转质选择读取；转换测试改用技能 6 自身转质配置。

**H3 — 基础数值三源不一致且偏离设计**
- 设计（行 396）：持续 5s、脉动 0.5s、法力 30、冷却 4s、最大 1 阵。
- skills.json：mana_cost=50、cooldown=1.0、tags 缺 Duration、多 Hit。
- `SkillSpecializationBaker.cpp:85-90`：duration=6.0f、sub_interval=0.3f、area_radius=120。
- `SwordArray.cpp:38-40`：同样的 6.0/0.3/120 兜底。
- `SkillDefs.hpp:1080-1082` 组件默认却是 5.0f/150.0f/0.5f（与设计一致）——**同一字段三个真相源**，实际取 Baker/DoCast 的偏离值。
- 另：DoCast 无任何"已存在剑阵"上限校验（`SwordArray.cpp:26` 直接 create），连设计基础值"最大 1 阵"都不满足——连按可堆出任意数量领域（610/611 未实现加剧此问题）。
- 整改：统一为 skills.json 数据为唯一来源（Baker 仅做增量），恢复 5s/0.5s/30/4s，DoCast 施加存在数量上限。

**H4 — 已实现节点的数值型制偏离设计且不随点数缩放**
- 630 迟缓剑压：硬编码缓速 30%/2s（`SwordArray.cpp:66`），设计为 10%..40% 四级（行 423）。
- 633 绝命法场：Baker `more_damage_mult ×1.25`（`SkillSpecializationBaker.cpp:650`）**无差别全局增伤**；设计为"仅对 Boss More+20%"＋"<12% 处决"（行 426）。`execute_health_threshold_ratio=0.15` 默认值与设计 0.12 不符，`execute_damage_max_health_ratio=0.10` 在设计中**不存在对应机制**（臆造字段）。
- 652 意念合一：每 0.3s 无条件 +1 剑意（`SwordArray.cpp:74-78`）；设计为"阵内每秒 33%..100% 几率生成 1 层"（行 434）——节律（脉动 vs 秒）、条件（无条件 vs 限阵内）、型制（必然 vs 概率）三重偏离；且剑意计时刻（`array.damage_timer`）与实际伤害脉动计时（`field.timer`）独立漂移，不同步。
- 653 随身剑垒：唯一走通用管道的节点（`StatsSystem.cpp:484-485` 通用 stat_modifiers），但内容为 t8/Armor +28/点——设计是"贴身光环+伤害 -50%"（行 435），护甲数值为臆造。653 且不可达（C3）。

**H5 [新增] — VisualFXSystem 寿命硬编码 5.0s 导致剑阵初段完全隐形**
- `src/game/systems/combat/VisualFXSystem.cpp:196-200`:
  ```cpp
  const float remaining = arrayInfo.duration;
  const float fadeIn = std::clamp((5.0f - remaining) / 0.3f, 0.0f, 1.0f);
  const float fadeOut = std::clamp(remaining / 0.4f, 0.0f, 1.0f);
  const float visibility = std::min(fadeIn, fadeOut);
  ```
- 视觉渲染系统错误假定剑阵最大寿命固定为 5.0s。而当前 Baker 与 `SwordArray.cpp` 将基础持续时间设为 6.0f。
- 当 `remaining > 5.0f` 时，`5.0f - remaining` 为负数，`fadeIn` 被 clamp 为 0.0f，导致 `visibility = 0.0f`（Alpha=0）。剑阵在施放后的**前 1.0 秒内完全隐形**！
- 若玩家点出 600 节点（持续时间最高 +2.0s 达 7~8s），隐形时间将放大至 2.0~3.0 秒。
- 根因：`SwordArrayComponent` 缺少 `initial_duration` 字段记录初始总持续时间供视觉归一化，且渲染系统硬编码常数 5.0f。

**H6 [新增] — 跨技能 5 联动断裂：515 万剑归阵暂停机制因双寿命失效**
- `src/game/systems/skill/BeamChannelDeliverySystem.cpp:222-236` 实现了技能 5 节点 515（万剑归阵）：
  ```cpp
  targetPos = {arrPos.x, arrPos.y};
  array.duration += beam.tick_interval; // 暂停剑阵持续时间判定
  ```
- 该逻辑仅累加了 `SwordArrayComponent.duration`，**完全未累加 `AreaFieldComponent.remaining_duration`**！
- 而 `AreaFieldDeliverySystem.cpp:43-46` 独立扣减 `field.remaining_duration`，且在到期时将实体推入 `s_to_destroy` 销毁。
- 结果：引导万剑归宗虽然在数值上维持了 `array.duration`，但经过 6 秒后，剑阵实体依然被 `AreaFieldDeliverySystem` 强制销毁，技能 5 的核心联动机制**在运行时静默失效**。

### Medium

**M1 — skill_mechanics.json 无技能 6 条目**：顶层仅 `version/comment/"1".."5"`，技能 6 机制参数零外置（同技能 3 审查 M4 缺陷型）。逐点数值（缓速 10..40%、破甲 50..200%、处决 12%、燃烧 3s、侵蚀 2/层×10 等）将无处可查。
**M2 — max_points 22/28 与设计不符**：`600:5(4)、601:1(4)、603:5(4)、611:3(1)、612:1(3)、614:5(1)、615:5(3)、630:5(4)、631:5(4)、632:5(3)、633:5(1)、634:5(1)、635:5(1)、650:5(4)、651:5(4)、653:5(1)、654:5(3)、670:3(1)、671:5(3)、673:5(3)、674:1(4)、675:5(1)`；仅 602/610/613/652/655/672 六个符合。**611/612 的 max 疑似互换**（611 三才阵 3、612 剑气共鸣 1；设计为 1/3）。
**M3 — prereq 数值与设计不符（非锁死类）**：631 需 630×1（设计 3/4，行 424）、633 需 632×1（设计 2/3，行 426）；634 需 630×4 与设计 4/4 一致但 630 max=5 使其偏离原意（应在 4 点树上刚好满点）。
**M4 — 测试矩阵固化错误关键集**：`tests/SkillKeyNodeMatrixTestHelpers.hpp:112` 与 `tests/fixtures/skill_specialization_keynodes.json:31` 均为 `{630,633,652,670,671}`——**遗漏 672、误把 671 当关键转质**，与 compact 契约同错。技能 6 的全部矩阵测试基于该集合，无法发现转质缺失。
**M5 — 测试断言过浅且断言臆造默认值**：冒烟矩阵仅断言存在 SwordArrayComponent 实体（`SkillSystemTests.cpp:1383-1385`）；集成场景仅断言 `HasArrayFlags`（`SkillKeyNodeMatrixIntegrationTests.cpp:342-347`）；守卫测试断言 `execute_*` 默认值 0.15/0.10（`SkillBehaviorGuardTests.cpp:448-449`）——对无消费字段断言默认值，会误导后续开发以为处决已实现。
**M6 — 双计时器/双销毁路径冗余与节拍漂移**：`array.duration`（`SwordArray.cpp:71-72`）与 `field.remaining_duration`（`AreaFieldDeliverySystem.cpp:43-46`）并行递减、两系统均可销毁实体。当前无 UAF（entt 组件即时移除），但属冗余状态且是 H6、M4/H4 中节拍漂移的根因。建议剑阵实体只保留一个寿命权威。
**M7 [新增] — 技能 3 联动阵眼判定锚点错误**：
- `src/game/systems/skill/SummonAISystem.cpp:169`:
  ```cpp
  const float d2 = Vector2DistanceSqr({ownerPos->x, ownerPos->y}, {arrPos.x, arrPos.y});
  if (d2 <= arr.radius * arr.radius) { effectiveDt *= 1.50f; break; }
  ```
- 技能 3 节点 355（剑阵共鸣）设计为“当处于 [剑阵·诛仙] 范围内时，灵剑获得 50% 攻速加成”。但代码计算的是**玩家本体坐标 `ownerPos`** 到剑阵的距离，而非**灵剑召唤物坐标 `pos`**！导致玩家在阵外但灵剑在阵内时加成失效，玩家在阵内而灵剑在千里之外时却错误获得加成。
**M8 [新增] — 675 移形换阵数据层前置链错误配置为 674**：
- `assets/data/skills.json` 与 `skill_6_tree.json` 将 675 的前置配为 `674×1`。
- 设计文档 §3.6 行 446 规定需求为 `{需求: 任意 Transmuter}`（即 `[670×1, 672×1] OR`，与 674 平行）。现配置强制玩家必须先投入 674 才能解锁 675，错误增加了前置层级并扭曲了天赋树拓扑。
**M9 [新增] — BuffIds.hpp 中剑阵枚举为死代码**：
- `src/game/foundation/data/BuffIds.hpp` 中定义了 `SwordArraySlow` 与 `SwordArrayArmorShred`，但在整个工程中**无任何读写点**。
- `SwordArray.cpp:66` 直接绕过 BuffId 使用 `AilmentType::Slow`，破甲 flag 未消费，属于未接通的死枚举。
**M10 [新增] — 缺失独立功能测试套件 SwordArrayNodes.cpp**：
- 对照技能 1（`FlowingThrustNodes.cpp`）、技能 2（`RendingWaveNodes.cpp`）、技能 3（`BladeFormationNodes.cpp`）、技能 5（`InfiniteBladesNodes.cpp`），技能 6 **完全缺失独立的节点功能测试文件**。现有测试仅散落在通用冒烟与守卫测试中，覆盖率形同虚设。

### Low

**L1 — skill_6_tree.json 与 skills.json 内联树双份数据**：前者节点仅 `{id,name_key,x,y,prerequisites}`（无 max_points/stat_modifiers），漂移风险；建议明确单一来源或生成器化。
**L2 — 行为层双轨取参**：profile 缺失时走 `exec.active_nodes.test(id%100)` 兜底（`SwordArray.cpp:46-49`），单测仅覆盖兜底轨（`SkillBehaviorGuardTests.cpp:425-439` 无 ActiveSkillsComponent），烘焙轨无行为级单测。
**L3 — Damage 载荷未写 damage_tags**：`SwordArray.cpp:64` 构造 PayloadDefinition 未设 `damage_tags = profile->effective_tags`（对照 `AreaFieldDeliverySystem.cpp:174-176` Skyfall 有设），元素标签依赖阵实体 SkillModifierComponent 间接生效，链路不直观。
**L4 — codebase-memory 图索引过期**：`ResolveSwordArrayHeavenlyAttunementConversion` 指向 SwordArray.cpp:52-66，现行文件（85 行）无此函数；重建索引。
**L5 [纠正] — tests/integration/DeliveryArchetypesTests.cpp:190-210 以技能 6 充当 Skyfall 残留域载体**：属测试夹具复用（skill_id=6 非天降技能），建议换独立 ID 以免误导。
**L6 [新增] — 契约源头 skills.json 遗漏 653 随身剑垒**：653 作为设计明确的核心 Keystone，在内联契约和 compact 契约中均未注册，处于未纳管状态。
**L7 [新增] — AreaFieldDeliverySystem 丢弃 ResolveDamage 返回值**：`AreaFieldDeliverySystem.cpp:97` 为 `(void)ResolveDamage(registry, req, target);`，丢弃了 `DamageExecutionResult`，后续实现 633 斩杀判断及 635 处决触发时必须捕获目标是否致死。
**L8 [新增] — 元素转质未联动视觉色彩**：`SwordArrayComponent` 预留了 `core_color` 与 `glow_color`，但 `SwordArray.cpp` 发生元素转换（天剑认证或未来 670 火/672 雷）时从未修改这两个颜色，视觉上恒为紫色魔法网。

## 8. 逐节点核对矩阵

| 节点 | 设计 max | 实际 max | 设计 prereq | 实际 prereq | 角色 | 可达 | 实现 | 偏差要点 |
|------|---------|---------|-------------|-------------|------|------|------|---------|
| 600 灵气流转 | 4 | 5 | — | — | Passive | ✓ | ❌ 无分支 | 持续 +0.5s/点 未实现 |
| 601 虚空法网 | 4 | 1 | — | — | Keystone(应 Passive) | ✓ | ❌ 无分支 | 半径 +15..60% 未实现；**锁死下游** |
| 602 极刑 | 5 | 5 | — | — | Passive | ✓ | ❌ 无分支 | 阵内每秒伤害 More +10..50% 未实现 |
| 603 阵基稳固 | 4 | 5 | — | — | Passive | ✓ | ❌ 无分支 | 法力 -5..20%/施法范围 +10..40% 未实现 |
| 610 双生剑阵 | 1 | 1 | 600×3 | 600×3 | Passive(契约误 Keystone) | ✓ | ❌ 无分支 | 数量+1/伤害-15% 未实现；DoCast 无上限 |
| 611 三才阵 | 1 | 3 | 610 | 610×1 | Passive(契约误 Shred) | ✓ | ❌ 无分支 | max 疑似与 612 互换；法力 +30% 未实现 |
| 612 剑气共鸣 | 3 | 1 | 610 | 610×1 | Keystone(应 Passive) | ✓ | ❌ 无分支 | 重叠 More +20..60% 未实现；**锁死 613** |
| 613 千丝万缕 | 1 | 1 | 612×2 | 612×2 | Keystone ✓ | **✗** | ❌ | 能量连线未实现；被 612 锁死 |
| 614 流云穿阵 | 1 | 5 | 601×3 | 601×3 | Synergy ✓(契约缺) | **✗** | ❌ | 位移引爆 150% 未实现；被 601 锁死 |
| 615 御剑阵威 | 3 | 5 | 601×2 | 601×2 | Passive | **✗** | ❌ | 御剑步频率 +20..60% 未实现；被 601 锁死 |
| 630 迟缓剑压 | 4 | 5 | 602×2 | 602×2 | Synergy(应 Passive) | ✓ | ⚠️ 部分 | 缓速硬编码 30%/2s，设计 10..40% 四级 |
| 631 破甲剑意 | 4 | 5 | 630×3 | 630×1 | Passive(契约缺) | ✓ | ❌ 无效 | flag 写入无消费；Armor Shred 未实现；prereq 数值错 |
| 632 虚弱领域 | 3 | 5 | 631×2 | 631×2 | Passive | ✓ | ❌ 无分支 | 敌方 Less -6..18% 未实现 |
| 633 绝命法场 | 1 | 5 | 632×2 | 632×1 | **Trigger(应 Keystone)** | ✓ | ⚠️ 部分 | 处决无消费；×1.25 全局增伤；**误触发裂空斩(C2)**；prereq 错 |
| 634 剑阵牢笼 | 1 | 5 | 630×4 | 630×4 | Keystone(契约缺) | ✓ | ❌ | 实体剑墙/半径-30% 未实现 |
| 635 阵斩回响 | 1 | 5 | 633 | 633×1 | **Trigger(契约缺)** | ✓ | ❌ | 触发参数错挂在 633；处决事件无源头 |
| 650 阵眼 | 4 | 5 | 601×2 | 601×2 | Passive | **✗** | ❌ | 阵内全局 More +15..60% 未实现；被 601 锁死 |
| 651 灵力泉涌 | 4 | 5 | 650×2 | 650×2 | Passive | **✗** | ❌ | 每秒回蓝 2..8 未实现；被 601 锁死 |
| 652 意念合一 | 3 | 3 | 651×2 | 651×2 | Passive+affects_intent ✓ | **✗** | ⚠️ 部分 | 已实现但**不可达**；型制偏离(H4) |
| 653 随身剑垒 | 1 | 5 | 650×4 | 650×4 | Keystone(契约缺) | **✗** | ⚠️ 错误 | 通用 +28 护甲/点为臆造；贴身光环未实现；被 601 锁死 |
| 654 剑神领域 | 3 | 5 | 653 | 653×1 | Passive | **✗** | ❌ | CDR +10..30% 未实现；被 601 锁死 |
| 655 法阵回护 | 3 | 3 | 650×2 | 650×2 | Passive | **✗** | ❌ | 智力 50..150% Ward/秒 未实现；被 601 锁死 |
| 670 焚天烈焰阵 | 1 | 3 | 603×2 | 603×2 | Transmuter ✓ | ✓ | ❌ 无分支 | 完全转火/熔岩/必附烧灼 未实现；max 错 |
| 671 炼狱余火 | 3 | 5 | 670 | 670×1 | **Transmuter(应 Passive)** | ✓ | ❌ | 点燃 More +20..60%/离阵燃烧 3s 未实现 |
| 672 九幽雷池 | 1 | 1 | 603×2 | 603×2 | **Keystone(应 Transmuter)** | ✓ | ❌ 无分支 | 完全转雷/1s 随机 2-3 单体落雷 未实现；**转质链失效** |
| 673 连珠落雷 | 3 | 5 | 672 | 672×1 | Passive | ✓ | ❌ | 落雷 +1..3 目标/电弧 未实现 |
| 674 法阵侵蚀 | 4 | 1 | 任意转质 | [670×1,672×1]OR | Keystone(应 Passive+TypeB) | ✓ | ❌ | TypeB 降抗 compact 挂在 672，应挂 674；max 错 |
| 675 移形换阵 | 1 | 5 | 任意转质 | 674×1(错误) | Passive | ✓ | ❌ | 移阵/重置一半持续 未实现；前置错挂 674；max 错 |

**统计汇总**:
- 实现状态：4/28 实现（部分 3、无效 1），24/28 零实现
- 可达性：9/28 不可达（前置死锁）
- 点数容量：22/28 max_points 错误
- 契约角色：10+ 节点角色错乱或遗漏（634/635/653 缺失）
- 拓扑前置：675 前置错误指向 674，631/633 点数需求偏离

## 9. JSON 数据矛盾清单（skills.json ↔ compact ↔ skill_6_tree ↔ 设计）

1. **compact 契约转质池错误**: `transmuter_node_ids=[670, 671]` vs 设计 `[670, 672]`（671 为普通被动，672 为真转质）。
2. **转质最大容量违背互斥**: compact/内联 `max_transmuters=2` vs 设计火/雷互斥 `max_transmuters=1`。
3. **Synergy 节点错位**: compact `synergy_node_ids=[630]` vs 设计 `[614]`（630 为迟缓被动，614 为位移穿阵引爆）。
4. **Trigger 节点与参数错挂**: compact/内联 `trigger_nodes=[{633, skill2, 0.5, ICD3.0}]` vs 设计 `635`（633 为处决 Keystone，635 为处决触发裂空斩）。
5. **抗性击碎挂载错乱**: compact/内联 `resist_models={611:TypeB_Shred, 672:TypeB_Shred}` vs 设计 `674:TypeB_Shred`（611/672 均无 Type B 削抗）。
6. **契约关键节点遗漏**: `634`（剑阵牢笼）、`635`（阵斩回响）、`653`（随身剑垒）在契约节点清单中**完全缺失**。
7. **max_points 全面偏离**: 22 个节点数值错误（611↔612 互换，601/674 仅为 1 锁死拓扑，其余被动节点多为 5）。
8. **前置条件偏离与错误**: 
   - 631 配置 630×1 vs 设计 3/4；
   - 633 配置 632×1 vs 设计 2/3；
   - 675 配置 674×1 vs 设计 `[670×1, 672×1] OR`（任意转质）。
9. **基础标签与基础数值偏离**: 
   - skills.json tags 为 `[Physical, Spell, Area, Hit, sword_skill]` vs 设计 `[Spell, Area, Physical, Duration]`（缺 Duration、多 Hit）；
   - mana_cost=50（设计 30）、cooldown=1.0（设计 4.0）。
10. **机制表空白**: `skill_mechanics.json` 缺少技能 6 配置根键（M1）。

## 10. 架构与跨系统偏差

1. **通用 Trigger 机制与数据错误叠加放大**：
   `SkillSpecializationBaker.cpp:127-143` 的通用 Trigger 写入本身无逻辑缺陷，但因契约将 633 标为 Trigger，导致领域伤害脉冲每跳命中即触发裂空斩——数据层错误经通用系统放大为严重的玩法灾难。
2. **转质机制所有权断裂**：
   技能 6 元素转换由外部技能 3（天剑诀认证）硬编码抢占，而引擎内部转质管道（`SkillSystem` → `StatsSystem`）因 672 未标为 Transmuter 对其完全失效，两条转质通道双双断裂。
3. **寿命双权威与跨系统联动失效**：
   `SwordArrayComponent` 与 `AreaFieldComponent` 并行维护寿命与计时。技能 5 联动（`BeamChannelDeliverySystem.cpp:232`）仅给 `array.duration` 补时，未能同步 `AreaFieldComponent.remaining_duration`，导致剑阵依然按原定时间被销毁，跨技能联动失效（H6）。
4. **视觉淡入对绝对时长的硬编码耦合**：
   `VisualFXSystem.cpp:198` 假定最大时长为 5.0f 计算 `fadeIn`，在实际持续时间大于 5.0f 时产生负数 clamp 为 0，导致剑阵在施放初始阶段完全隐形（H5）。
5. **空间几何与范围判定锚点错误**：
   `SummonAISystem.cpp:169` 判定灵剑是否在剑阵中时，使用玩家本体与剑阵中心的距离，而非灵剑实体坐标，违背空间局部性物理直觉（M7）。
6. **测试固化错误数据与假象**：
   关键节点矩阵在 helper 与 fixture 中硬编码，错误契约直接传染测试用例；测试断言 `execute_*` 默认值给出了“处决已就绪”的虚假安全感（M4/M5）。

## 11. 符合项

1. 行为层存在且注册（`REGISTER_SKILL_BEHAVIOR(SwordArray)`，`SkillSystem.cpp:1219-1224` 派发；`GameplaySystems.cpp:250-282` ID 合法性守卫通过）。
2. AreaField 通用交付管道完整可用（`AreaFieldDeliverySystem.cpp:26-111`：阵营过滤、KilledTag 过滤、定长 POD 载荷、VFX 提交、Damage+Ailment 两类载荷）。
3. feature_flags 烘焙→行为传递链（Baker case 6 → `profile->delivery.feature_flags` → SwordArray.cpp:46-49）完整，652 剑意回复方向正确（`SwordArray.cpp:76-78` 调 `SkillSystem::GainSwordIntent`）。
4. 前置 OR 语义、装备修饰器烘焙（Baker:161-198）、契约运行时组件存取/存档（SaveManager.cpp:128,253）等基础设施与技能 3 共用且此前已验证。
5. 670/672 prereq 数值（603×2）与设计 2/4 一致；602/610/613/652/655/672 max 正确；613 Keystone 角色、614 Synergy 类型、674"需任意转质"OR 前置形态与设计一致。

## 12. 整改路线（按优先级）

### Phase 1: 阻断级数据与契约修复（即刻修复事故）
1. **修复 C2 / H1**:
   - `assets/data/skills.json` 与 `assets/data/skill_contracts_compact.json`:
     - 633 角色从 `Trigger` 恢复为 `Keystone`，移除 trigger 字段；
     - 新增 635 契约条目（角色 `Trigger`，trigger_skill_id: 2, effectiveness: 0.5, internal_cooldown: 3.0, consumes_mana: false）；
     - 672 角色设为 `Transmuter`，从 `resist_models` 移除；
     - 671 角色恢复为 `Passive`，从 `transmuter_node_ids` 移除；
     - 674 角色设为 `Passive`，配置 `resist_models: {674: TypeB_Shred}`；
     - 614 注册为 `Synergy`（compact `synergy_node_ids=[614]`，移除 630）；
     - 补充 634、653 契约至 `skills.json` 与 compact 契约；
     - 设置 `max_transmuters: 1`。
2. **解开 C3 前置拓扑死锁**:
   - 修正 `601 max_points=4`，`612 max_points=3`，解开 9 节点死锁；
   - 互换 `611/612` 的 max_points 偏差（611→1，612→3）；
   - 修正 631（需 630×3）、633（需 632×2）的前置点数要求；
   - 修正 M8：将 675 的前置从 `674×1` 改为 `[670×1, 672×1] OR`。
3. **纠正基础技能数值 (H3)**:
   - `skills.json`: duration 5.0, pulse_interval 0.5, mana_cost 30.0, cooldown 4.0, tags 去除 Hit 添加 Duration；
   - 同步修改 Baker:85-90 与 `SwordArray.cpp:38-40` 兜底数值为 5.0/0.5/150。

### Phase 2: 核心行为与外部联动修复（闭环核心机制）
4. **修复寿命单一权威与技能 5 联动 (H6 / M6)**:
   - 统一剑阵实体寿命权威：若由 `AreaFieldComponent` 管理寿命，则 `SwordArrayComponent` 不再重复管理 entity 销毁；
   - 或在 `BeamChannelDeliverySystem.cpp:232` 中同时累加 `field.remaining_duration += beam.tick_interval`，确保万剑归阵能够正确暂停剑阵。
5. **修复视觉初段隐形缺陷 (H5)**:
   - 在 `SwordArrayComponent` 中添加 `float total_duration = 5.0f;`；
   - `VisualFXSystem.cpp:198` 改为基于 `std::clamp((arrayInfo.total_duration - remaining) / 0.3f, 0.0f, 1.0f)` 计算淡入，消除 5.0f 硬编码。
6. **修复技能 3 联动锚点错误 (M7)**:
   - `SummonAISystem.cpp:169` 将 `ownerPos` 替换为灵剑实体本身的 `pos`，修正攻速判定。
7. **实现 633 处决与 631 破甲机制 (C1)**:
   - 在 `AreaFieldDeliverySystem.cpp` 脉冲结算循环中，引入血量比率判断（生命 < 12% 且非 Boss 目标直接处决），处决成功时派发 CombatEvent；
   - 捕获 `ResolveDamage` 结果（L7）；
   - 接通 `has_armor_shred`，向目标施加 Armor Shred Ailment 或接通 `BuffId::SwordArrayArmorShred`。

### Phase 3: 专精树全量机制与外置参数落地
8. **机制表外置 (M1)**:
   - `assets/data/skill_mechanics.json` 建立 `"6"` 顶层键，录入全部 28 节点数值。
9. **补齐分支机制**:
   - 分支 A: 610/611 阵数量限制与施法上限检查，612 重叠 More，613 能量连线，614 位移穿阵引爆；
   - 分支 B: 632 敌方 Less，634 实体剑墙，635 处决触发裂空斩；
   - 分支 C: 650 阵内玩家全局 More，651 回蓝，652 意念合一（阵内每秒概率生成剑意），653 随身光环，654 CDR，655 护盾；
   - 分支 D: 670 火焰转质/熔岩地面，671 点燃增伤，672 雷池转质（改单体随机落雷），673 连珠电弧，674 抗性击碎 debuff，675 移形换阵重按挪移。
10. **色彩与视觉联动 (L8)**:
    - 转火设置火红 core/glow，转雷设置蓝白 core/glow。

### Phase 4: 测试护栏重构与回归
11. **测试矩阵纠偏 (M4 / M5)**:
    - `ExpectedKeyNodesBySkill()` 与 fixture 更新关键集为 `{613, 614, 633, 634, 635, 650, 652, 653, 670, 672, 674}`；
    - 移除对 0.15/0.10 废弃默认值的断言。
12. **新增独立功能测试文件 (M10)**:
    - 建立 `tests/functional/SwordArrayNodes.cpp`，仿照技能 1/2/3/5 建立覆盖 28 节点行为的单测套件。

## 13. 剩余风险

1. **处决事件在 CombatEventType 中的支持**:
   `CombatEventType` 当前包含 `OnKill`、`OnOverkill`，缺少独立的 `OnExecute`。若 635 仅由处决触发，需评估是在 `OnKill` 的 event payload 中附带 `is_execute` 标记，还是扩展新的 `CombatEventType::OnExecute`（波及 `ProcEngine`）。
2. **672 雷池单体落雷与 AreaField 架构冲突**:
   AreaField 是均匀全域 AOE 模型，而 672 要求“1 秒一次随机抽取 2-3 名敌人劈下单体高额落雷”。需要评估是在 `AreaFieldDeliverySystem` 中扩展随机目标模式，还是学习 `HeavenlySwordFieldComponent` 将雷池转质抽离为专用 Field 组件。
3. **653 随身剑垒的光环空间定位**:
   地表领域当前基于固定 `Position` 注册在 `SpatialHashGrid`。随身剑垒随玩家移动，每帧需动态更新阵实体的 `Position` 并重新插入 SpatialGrid，需关注多线程安全与查询一致性。
4. **多阵重叠（610/611/612）与能量连线（613）计算开销**:
   多个剑阵重叠区域的判定及中心连线碰撞线段检测，在大量敌人与同屏多阵时可能带来 CPU 峰值，需控制空间查询开销。

## 14. 证据命令摘要

```powershell
# 验证工作区与消费点
git status --short
rg -n "SwordArrayComponent" src
rg -n "execute_health_threshold_ratio|execute_damage_max_health_ratio" src

# 验证外部联动与视觉硬编码
rg -n "array\.duration \+= beam\.tick_interval" src/game/systems/skill/BeamChannelDeliverySystem.cpp
rg -n "5\.0f - remaining" src/game/systems/combat/VisualFXSystem.cpp
rg -n "Vector2DistanceSqr\(\{ownerPos->x" src/game/systems/skill/SummonAISystem.cpp

# 验证节点数据与死代码
python -c "import json; d=json.load(open('assets/data/skills.json',encoding='utf-8')); print([s for s in d['skills'] if s['id']==6][0]['talent_tree'])"
rg -n "SwordArraySlow|SwordArrayArmorShred" src/game/foundation/data/BuffIds.hpp

# 设计基线：设计文档/职业设计草案_剑修.md 394-446 行
```

## 15. 第三轮审查（2026-09-10，整改复核）

### 15.1 复核范围与证据

- 变更边界：19 个修改文件（4 数据 JSON、SkillDefs.hpp、SkillRegistry.cpp、VisualFXSystem.cpp、AreaFieldDeliverySystem.cpp、BeamChannelDeliverySystem.cpp、SkillSpecializationBaker.cpp、SkillSystem.cpp、SummonAISystem.cpp、SwordArray.cpp、6 个测试文件）+ 新增 `tests/functional/SwordArrayNodes.cpp`（1035 行，9 用例 205 断言）。
- 构建：`build.bat`（RelWithDebInfo, j=7）exit 0。
- 测试：nmd.tests.combat.unit / skill.unit / combat.integration / skill.integration 4/4 通过；`bin/NoMoreDayTests.exe "[Functional]*Skill 6*"` 9 用例全过；全量 21 项 CTest 19 通过，2 项失败与本变更无关（见 15.5）。
- 管线一致性：`scripts/gen_skill_contracts.py --check` 通过（曾因手工编辑丢末尾换行 FAIL，已由生成器重写修复，手工数据变更保留）。
- 设计对照：设计文档/职业设计草案_剑修.md §3.6（394-446 行）全部数值抽查吻合（630 四级 10..40%、631 50..200%、652 33..100%、612 More 20..60%、634 半径 70%、675 半法力+重置一半、674 1..4 层/2 点/10 上限/3s 等）。

### 15.2 整改确认矩阵

| 编号 | 整改要求 | 结论 | 证据 |
|---|---|---|---|
| C1 | 24 节点全量实现 | **✓** | SkillSpecializationBaker.cpp:648-728（28 节点分支）；AreaFieldDeliverySystem.cpp/SwordArray.cpp 运行时；SkillSystem.cpp:859-861（635 门控）；新增测试 9 用例全过 |
| C2 | 633 误标 Trigger | **✓** | 内联/compact 契约 633→Keystone、635→Trigger（trigger_skill_id 2）；ExecutedTag 门控确保仅处决触发（SkillSystem.cpp:859-861）；测试含普通击杀不误触发反例 |
| C3 | 9 节点锁死 | **✓** | 601/612 max 修正；613/614/615/650-655/670-675 全部可点且行为生效（测试验证） |
| H1 | 契约角色错乱 | **✓** | Keystone 633/634/653/613、Trigger 635、Synergy 614、Passive 674/671；keystone_exclusion_groups/keystone_node_ids/passive_node_ids 为生成器合法 schema（passive_node_ids 语义与技能 4 先例一致），运行时互斥消费链存在（SkillSystem.cpp:378/2129/2145/2342/2447 等） |
| H2 | 670/672 无烘焙 | **✓** | Baker case 6 新增 670/672 分支（flag 262144/1048576）；装备修饰级互斥守卫；技能 3 认证仅保留为"无转质时"回退（偏离已收敛到回退路径，可接受） |
| H3 | 基础数值三源不一致 | **✓** | 法力 30/冷却 4.0s/持续 5s/间隔 0.5s/半径 150/单阵上限，三源对齐，测试断言验证 |
| H4 | 臆造行为 | **✓** | 630 参数化 slow_magnitude；633 处决+Boss More 移至运行时按 Boss 判定；652 概率生成+限阵内+每秒节拍；653 护甲臆造删除 |
| H5 | 视觉 5.0f 硬编码 | **✓** | VisualFXSystem.cpp:193-199 基于 total_duration 计算淡入，测试验证 0.6667 数学 |
| H6 | 515 双寿命 | **✓** | BeamChannelDeliverySystem.cpp:224-242 三字段同步（duration/total_duration/field.remaining_duration），GetSkill5Point 兜底 |
| M1 | 机制表 | **部分 ✓** | skill_mechanics.json 新增 "6" 全 28 节点表且被 SkillMechanicsRegistry 加载；**残留**：672 落雷 ×2.5f 硬编码于 AreaFieldDeliverySystem.cpp、`base_targets` 键声明未消费（见 L-C） |
| M2 | max_points | **✓** | 22 处全部修正 |
| M3 | prereq | **✓** | 631←630×3、633←632×2、675←[670×1,672×1] OR |
| M4 | 测试矩阵 | **部分 ✓** | 关键集更新为 {613,614,633,634,635,652,653,670,672,674}，比第 2 轮建议少了 650——650 为常驻 Buff 型节点（无 trigger/guard 层语义），已在功能测试用例覆盖，可接受 |
| M5 | 断言加深 | **大部分 ✓** | 0.12 阈值、shred 堆叠数值、虚弱 6 属性、腐蚀堆叠/封顶、处决正反例、675 资源、多阵去重均已断言；**残留**：630 缓速仍为宽松匹配（测试仅验 flag 生效未验 40% 数值） |
| M6 | 双寿命权威 | **✓** | AreaField 为单一权威：AreaFieldDeliverySystem.cpp:44-45 每帧同步 array.duration；SwordArray.cpp Update 有 field 时只读不递减 |
| M7 | SummonAI 锚点 | **✓** | SummonAISystem.cpp:170 改用灵剑自身坐标；测试验证阵内攻速生效 |
| M8 | 675 前置 | **✓** | [670×1, 672×1] OR，674 同步 |
| M9 | BuffId 死代码 | **部分 ✓** | BuffIds.hpp:28-29 SwordArraySlow/SwordArrayArmorShred 已接通（AreaFieldDeliverySystem.cpp:169/230 等消费）；**残留**：其余 5 个新 buff 绕过枚举体系用字符串 id（见 M-B） |
| M10 | 新测试文件 | **✓** | tests/functional/SwordArrayNodes.cpp 覆盖 28 节点行为 + 资源验证 + 去重验证，超额完成 |
| L1 | 双份数据 | **✓** | 生成器管线一致（--check 通过） |
| L2 | 行为层双轨 | **部分 ✓** | 数值计算统一在行为层 DoCast 从机制表读取（Baker 只置 flag），轨道统一；分工明确但与"全部烘焙"形态不同，可接受 |
| L3 | damage_tags | **✓** | SwordArray.cpp:338 payload 写入 effective_tag |
| L4 | 图索引过期 | 未整改 | 索引操作属工具链维护，非代码变更范围，维持现状 |
| L5 | 夹具借用 | 未整改 | DeliveryArchetypesTests 夹具未调整，维持现状 |
| L6 | 653 契约遗漏 | **✓** | Keystone 条目已补 |
| L7 | 返回值 | **✓** | AreaFieldDeliverySystem.cpp:212 `auto result = ResolveDamage(...)` 并用 result.target_killed |
| L8 | 色彩联动 | **✓** | SwordArray.cpp:323-340 火红/雷蓝 core/glow 联动，测试断言颜色值 |

### 15.3 新发现问题（本轮变更引入）

**High**

- **H-A 处决路径绕过正常死亡链**：`AreaFieldDeliverySystem.cpp:193-222` 手工处决（emplace KilledTag + OnKill + execHit）跳过了正常死亡链中的 `MonsterAffixSystem::OnEnemyDeath`（词缀死亡效果，如 SoulEater/Avenger 全局监听）与 `killCount++`。对照正常链 `CombatSystem.cpp:728-750`（736 行 OnEnemyDeath、749 行 killCount++）。带词缀普通怪被绝命法场处决时词缀死亡效果丢失、击杀统计缺失。**建议**：提取公共击杀助手（或至少补调 OnEnemyDeath），复用于两处执行。

**Medium**

- **M-A 热路径堆分配与字符串查找**：`AreaFieldDeliverySystem.cpp:108/127/130` 每次脉冲重建 candidates/struckTargets/pool 三个 `std::vector`；632/674/613/650/654 的 buff id 用字符串字面量构造 `std::string`（"SwordArrayWeaken"/"SwordArrayCorrosion" 等均 >15 字符，超出 MSVC SSO）并在每次命中时 `effects.Get(字符串)` 比较。违反 code_standard §2.1/§7.2。**建议**：静态 thread_local vector + BuffIds 枚举化。
- **M-B Buff id 体系不一致**：M9 仅接通 630/631 两个枚举（BuffIds.hpp:28-29），其余 5 个新 buff（"SwordArrayWeaken"/"SwordArrayCorrosion"/"SwordArrayConnectionSlow"/"SwordArrayCore"/"SwordArrayCDR"）直接用字符串字面量 id，绕过 BuffIds 体系。**建议**：统一补枚举。

**Low**

- **L-A ExecutedTag 无清除点**：全仓库仅 AreaFieldDeliverySystem.cpp:200/216 emplace 与 SkillSystem.cpp:860 门控，无显式清除，依赖死亡实体销毁自然消除；复生/长存场景有残留风险（当前无复生机制，风险低）。
- **L-B 处决 execHit 参数位语义错位**：`AreaFieldDeliverySystem.cpp:206/222` 调用 `CreateSkillHit(..., effective_tag, 0.0f, false)`——签名第 5 参为 `bool is_crit`、第 6 参为 `uint64_t cast_id`（CombatEvents.hpp:255-259），传 `0.0f`/`false` 凑巧行为正确（is_crit=false、cast_id=0），但参数位语义错误，签名演化时会静默错位。
- **L-C 672 数值机制表外**：落雷 ×2.5f 硬编码（AreaFieldDeliverySystem.cpp:118）；`lightning_targets = 2 + pts_673` 硬编码 base 2，机制表 `base_targets` 键声明未消费；设计"随机抽取 2-3 名"实现为固定 2 名。
- **L-D legacy 死字段**：`execute_damage_max_health_ratio` 标注 "legacy compat" 保留且全仓库无消费（SkillDefs.hpp:1099）。
- **L-E 文件尾换行**：skill_mechanics.json 尾部无换行（生成器不管此文件）。
- **L-F 线程防御退化**：AreaFieldDeliverySystem.cpp:38 `s_to_destroy` 由 `static thread_local` 改为 `static`；当前 Update 在 SkillSystem::Update 内单线程顺序调用（GameplayState.cpp:368 唯一调用点）无实际竞态，但与周围 thread_local 风格不一致。
- **L-G 增伤联动缺口**：613 连线（0.80*dt）与 614 引爆（1.50）效果力硬编码，不随 profile.more_damage_mult（610 -15%/602 +50%/653 -50%）缩放，而 650 主伤害通过载荷乘数承载——同一节点体系两种扩伤路径。
- **L-H 671 型制偏差**：设计"敌人离开火阵后继续燃烧 3 秒"，实现为命中时点燃 duration+3s（AreaFieldDeliverySystem.cpp:237），效果近似但型制不同（阵内每跳都被延长）。

### 15.4 验证证据与限定

```powershell
# 构建与测试（全部执行）
build.bat                     # RelWithDebInfo, exit 0
ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(combat|skill).unit|nmd.tests.combat.integration|nmd.tests.skill.integration"   # 4/4 Passed
bin\NoMoreDayTests.exe "[Functional]*Skill 6*"   # 9 用例 205 断言全过
python scripts\gen_skill_contracts.py --check   # [OK]
ctest --test-dir build -C RelWithDebInfo        # 19/21 过
```

限定：全量 CTest 2 项失败与本变更无关——`nmd.tests.performance`（ParticleTrailBenchmark.cpp:205，SubEmitter 1k/frame dispatch 开销 0.237ms > 0.2ms，两次重跑稳定，粒子渲染路径与本变更无交集，属环境敏感基准）；`nmd.tests.gpu.hardware`（GPU 内核环境 NOT_RUN 类）。建议后续单独排查。

### 15.5 结论

第 2 轮 3 Blocker / 7 High / 10 Medium / 8 Low 除 L4（图索引，工具链）、L5（夹具）两项非代码项外全部实质整改完成；Phase 1-4 整改策略执行到位，M10 新增测试体系质量优秀（含处决正反例、互斥 Baker 级、多阵去重、资源验证等此前完全缺失的护栏）。

但本轮变更引入 1 个 High（H-A 处决绕过正常死亡链，词缀死亡效果与击杀统计丢失）、2 个 Medium（M-A 热路径堆分配、M-B buff id 体系）、8 个 Low。

**`修改`** —— 阻塞项为 H-A：修复后（提取公共击杀路径补调 OnEnemyDeath 与 killCount++，同步补一条处决词缀效果测试）即达 `提交` 状态。建议提交前顺手修复 M-B（BuffIds 补 5 个枚举，M-A 中字符串查找问题随之消解大半）；M-A 的 vector 复用与 L 级问题可作为后续跟踪，不阻塞。

## 16. 第四轮复核（2026-09-10，修复落地确认）

第 3 轮全部发现项已由三个并行子代理修复完毕（文件边界互斥：AreaFieldDeliverySystem+击杀链 / SwordArray+SkillDefs+机制表 / 测试护栏），主代理统一构建验证。

### 16.1 修复清单

| 项 | 修复内容 | 证据 |
|---|---|---|
| H-A | 提取公共击杀助手 `CombatSystem::KillEnemy(registry, target, attacker, overkill=0, rawDamage=0)`（CombatSystem.cpp:501-529，完整承接原死亡链：KilledTag→OnKill→OnEnemyDeath→OnOverkill 条件→PlayerStats.killCount 条件累加）；正常链 CombatSystem.cpp:758 改为一行调用；处决路径（AreaFieldDeliverySystem.cpp:203-234，两段重复块合并为 `tryExecute` lambda）改调助手。killCount 语义与正常链逐字一致（仅 PlayerStats 攻击者累加），`emplace_or_replace<KilledTag>` 使助手对两类调用方幂等 | tests/functional/SwordArrayNodes.cpp:1052-1097 新用例：Toxic 词缀敌人被处决 → KilledTag + killCount==1 + 3 个 VolatileOrbTag 毒球且 owner 正确 |
| M-A | candidates/struckTargets 改函数级 `static thread_local` + 使用前 clear；删除 pool 整池拷贝，改为在 candidates 上随机下标 swap/pop 原地抽取；632/674 的 `effects.Get` 改用 BuffId 枚举重载（Buff.hpp:207） | AreaFieldDeliverySystem.cpp:40/110/130-145/327/349/368 |
| M-B | BuffIds.hpp 新增 5 枚举（SwordArrayWeaken/ConnectionSlow/Core/CDR/Corrosion，映射字符串与原字面量逐字符一致，存档与断言零破坏）；AreaField 与 SwordArray 内全部字面量 id/查找改为枚举 | BuffIds.hpp:29-33/53-57；SwordArray.cpp:445/482/516 |
| L-A | 确认 `CombatEventDispatcher::Dispatch` 完全同步（CombatEventDispatcher.cpp:63-88，调用栈内联 handler）后，处决 dispatch execHit 返回后立即 remove<ExecutedTag>（AreaFieldDeliverySystem.cpp:223-224），标记生命周期闭合在同帧调用栈 | 联动：tests/functional/SwordArrayNodes.cpp 原 line 558 `any_of<ExecutedTag>` 断言同步删除（Update 返回后标记必已清除） |
| L-B | 处决 execHit 调用改为 `CreateSkillHit(owner, target, skill_id, effective_tag)`（去掉错位的 0.0f/false 实参） | AreaFieldDeliverySystem.cpp:218/231 |
| L-C | 落雷 ×2.5f → 机制表键 `strike_damage_mult`（skill_mechanics.json 672 节点）+ 脉冲级单次查表（AreaFieldDeliverySystem.cpp:147-151）；`lightning_targets = base_targets(2, 机制表) + GetInt(0,1) + pts_673`，体现设计"随机抽取 2-3 名" | skill_mechanics.json:590；SwordArray.cpp:231-233 |
| L-D | `execute_damage_max_health_ratio` legacy 死字段删除（rg 全仓零消费确认） | SkillDefs.hpp |
| L-E | skill_mechanics.json 补文件尾换行 | 文件末字节 125→10 |
| L-F | `s_to_destroy` 恢复 `static thread_local` | AreaFieldDeliverySystem.cpp:40 |
| L-G | SwordArrayComponent 新增 `damage_more_mult`（SkillDefs.hpp:1105），DoCast 从 `profile->more_damage_mult` 填充（SwordArray.cpp:244-245）；613 连线（`0.80f*dt*mult`）与 614 引爆（`1.50f*mult`）效果力接入，与 650 主伤害路径扩伤口径一致 | SwordArray.cpp:396-397/438-439 |
| L-H | 671 余燃 `3.0f` 硬编码 → 机制表键 `linger_burn_duration`（默认 3.0f 兜底）+ 中文注释说明「命中时延长点燃」与「离阵后余燃 3 秒」的型制映射取舍及未来迁移方向 | AreaFieldDeliverySystem.cpp:239-246 |
| 测试 T2 | 630 缓速数值断言补强（4 点 → -40.0 PercentAdd、SpeedDown、MoveSpeed、单 modifier） | tests/functional/SwordArrayNodes.cpp:415-423 |
| 测试 T3 | lightning_targets 3 处固定断言放宽为区间（>=2&&<=3 / >=4&&<=5，适配随机基线）；buff id 字符串断言核对零冲突（映射逐字符一致） | tests/functional/SwordArrayNodes.cpp:806-807/886-887/898-899 |

### 16.2 验证证据

```powershell
build.bat                                  # exit 0（修复两处编译错误后全绿）
ctest -R "nmd.tests.(combat.unit|skill.unit|combat.integration|skill.integration)"   # 4/4 Passed
bin\NoMoreDayTests.exe "[Functional]*Skill 6*"   # 全量 1457 用例 / 128383 断言 0 失败
ctest --test-dir build -C RelWithDebInfo   # 19/21 过
```

限定：全量 CTest 的 2 项失败均为与本变更无关的非确定性/环境项——`nmd.tests.gpu.hardware`（GPU 内核环境，两轮均失败）；`nmd.tests.ci.nonperf`（全量并行下偶发，单独重跑 exit 0 通过；上轮同类的 `nmd.tests.performance` ParticleTrail 0.237ms 边缘波动本轮已自行通过）。三项均非技能/战斗路径，建议独立工单跟踪 CI 稳定性。

### 16.3 结论

第 3 轮全部 1 High + 2 Medium + 8 Low 修复完毕并有测试护栏锁定（处决词缀链、630 数值、672 随机目标区间）。数据层 L4（图索引）与 L5（夹具）维持第 3 轮裁定不变（非代码项）。

**`提交`**

