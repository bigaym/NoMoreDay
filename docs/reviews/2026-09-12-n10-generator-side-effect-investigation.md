# N10 调查：gen_skill_contracts 泛化规则的越界产出

- 日期：2026-09-12
- 范围：B1 第一波 WS2 / T2.6
- 结论：**数据与设计一致，越界产出为信息冗余噪声，无需修脚本或回滚数据**

## 1. 背景

N10 记录于 `docs/reviews/2026-09-09-skill5-specialization-nodes-review.md:424`：

> gen 脚本泛化规则使技能1 内嵌契约新增 130/153/211/230/232 五条目、`mastery_skill_trees.json` 技能1 新增 1115——范围外变更，需确认影响面。

需核对上述新增节点是否符合设计意图，并确认 `scripts/gen_skill_contracts.py` 泛化规则的边界。

## 2. 结论速览

1. 新增条目的**角色判定与设计一致**：130/153/211/230/232/1115 在设计中均**不是** Keystone，生成器将其标记为 `Passive` 正确。
2. 越界产出的**根因**是生成器在 23086211 引入了 `emits_non_default` 的新条件（`max_points == 1 and role == Passive`），对所有技能/专精树的单点被动节点无差别补条目。
3. 由于运行时对缺失节点的默认角色就是 `Passive`（`src/game/foundation/data/SkillContract.hpp:62`），这些条目与“不生成”在语义上等价，**行为无差异**。
4. 处理建议：本波**保留数据现状**，将“收窄生成器 :491 条件”记为后续可选项，不在本波修改（回滚会波及全部 12 个技能与专精树的契约重生成）。

## 3. 证据

### 3.1 生成器边界

`scripts/gen_skill_contracts.py`：

- 角色判定 `:445-455`：`elif not explicit_keystone_ids and max_points == 1: role = Keystone`。当技能存在显式 Keystone 配置时，该单点→Keystone 推断被整体跳过，节点回落为 `Passive`。
- 产出开关 `emits_non_default` `:488-499`，其新增条件 `:491`：

  ```python
  or (max_points == 1 and role == ROLE_PASSIVE)
  ```

  对于“已配置显式 Keystone + 存在单点被动节点”的技能，该条件会让所有单点被动节点都产出一条全默认条目。
- 条目字段（`role=Passive, resist_model=None, scope_policy=SkillOnly, affects_sword_intent=false, affects_sword_step=false, trigger=默认`）与运行时默认完全一致。

提交溯源：

```
git show 23086211        # skill5 提交，同时改动 skills.json / mastery_skill_trees.json / gen 脚本
git log -S '"node_id": 1115' -- assets/data/mastery_skill_trees.json   # eb390b43 → 23086211
git log -S '"node_id": 130'  -- assets/data/skills.json                # ca0b3390 → 23086211
git log -S '"node_id": 211'  -- assets/data/skills.json                # 137dc3c8 → 23086211
```

### 3.2 运行时语义等价

`src/game/foundation/data/SkillContract.hpp:12-18` 定义 `SpecNodeRole{Passive=0,...}`，`:62` 中 `NodeContractData::role` 默认 `SpecNodeRole::Passive`。契约 `nodes` 为 `unordered_map<node_id, NodeContractData>`，缺失节点在消费端按默认 `Passive` 处理，因此补出的全默认条目与缺失条目等价。

### 3.3 设计一致性逐节点核对

设计文档 `设计文档/职业设计草案_剑修.md`：

| 节点 | 设计出处 | 设计标记 | 生成角色 | 判定 |
| --- | --- | --- | --- | --- |
| 130 留影 | `:144` | 无 `<Keystone>` | Passive | 一致 |
| 153 饮血刃 | `:156` | 无 `<Keystone>` | Passive | 一致 |
| 211 碎裂之刃 | `:192` | 无 `<Keystone>` | Passive | 一致 |
| 230 回旋劲 | `:200` | 无 `<Keystone>` | Passive | 一致 |
| 232 引力陷阱 | `:202` | 无 `<Keystone>` | Passive | 一致 |
| 1115 天域锁界 | `:1079` | 无 `<Keystone>` | Passive | 一致 |

对应技能的设计 Keystone 清单：

- 技能1 `:120`：风行者、孤注一掷（113/154）
- 技能2 `:176`：万剑归宗-残篇、星环护体、时空停滞、湮灭波（213/214/234/253）
- 技能11（`heavenly_sword`，天剑降临）`:1047`：天穹贯星、万象轮转、天相极化（1107/1113/1120）

新增条目节点均不在此列，故 `Passive` 为正确角色。

> 注：N10 原文称“技能1 新增 1115”，实际 1115 属于 `mastery_skill_trees.json` 中的 `heavenly_sword`（技能11），为原文对归属的笔误。

### 3.4 当前契约节点清单（核对基线）

- skill1：`[110,113,130,132,133,134,153,154,155,170,172,174]`（talent_tree 共 28 节点，`min_nodes=max_nodes=28`）
- skill2：`[211,213,214,215,230,232,234,251,253,254,255,270,272,274]`
- heavenly_sword(11)：`[1101,1102,1107,1109,1111,1113,1115,1117,1120,1124]`

## 4. 处置

- **数据**：保持现状，不回滚 130/153/211/230/232/1115。
- **脚本**：本波不改。
- **后续建议（非本波范围）**：如需消除契约噪声，可将 `scripts/gen_skill_contracts.py:491` 的条件收窄为仅在“缺少显式 Keystone 配置的技能”中生效，或直接移除该条件并为需要显式落盘的被动节点在数据层补声明；改动需整体重生成并回归全部技能契约。

## 5. 验证

- `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism --verbose`：`[OK] Determinism check passed.` / `[OK] Idempotency check passed.` / `[OK] skill_contract blocks are up to date.`，退出码 0。
- 本调查未运行任何 C++ 构建或测试命令。

## 附录 A：T2.1 设计词汇对齐项（`Duration`）

技能6（剑阵·诛仙）标签含 `Duration`（`assets/data/skills.json:3552`）。核查：

- `SkillRegistry.cpp:639-668` 的 `StringToTag` 通过 legacy 别名表 `:656-660` 将 `"Duration"` 映射到 `Tag::DamageOverTime`，因此该标签生效、未被静默丢弃。
- `TagRegistry.hpp` 中**没有**名为 `Duration` 的规范标签（枚举、`kTagInfoTable` 均无）。
- 设计文档 `设计文档/职业设计草案_剑修.md:395` 明确为技能6 声明 `[Duration]` 标签，`:407` 亦出现该词汇，属设计词汇。
- 消费方 `SkillDisplayPreviewService.cpp:140` 依据 `Tag::DamageOverTime` 将技能6 预览显示为每秒伤害（PerSecond）。

**处置**：保留数据现状，不改脚本、不删标签。**设计词汇对齐项**：设计层缺少规范 `Duration` 标签，当前依赖 legacy 别名借用 `DamageOverTime` 语义。建议后续设计任务二选一：(a) 在 `TagRegistry` 注册语义独立的规范 `Duration` 标签并定义其消费语义；(b) 在设计文档中明确 `Duration` 即 `DamageOverTime` 的同义声明，使别名成为正式约定而非临时兼容。
