# 怪物与AI设计 V1.0（Combat vNext 对齐）

关联文档：

- `设计文档/战斗系统与属性设计.md`
- `设计文档/怪物词缀设计.md`

## 1. 设计哲学：标签驱动 + 合同驱动

怪物仍然是标签容器，但战斗判定以统一合同为中心：

1. 怪物标签决定可用词缀、抗性、异常交互。
2. 伤害与承伤必须进入统一 `DamagePipeline` 和防御合同。
3. 怪物技能、光环、召唤行为必须接受触发预算与事件门禁。

## 2. 怪物种族与标签

### 2.1 种族（EnemyRace）

`EnemyRace::Type` 定义 9 个种族，配套静态表 `kRaceData` 提供基础属性与抗性标签：

| 枚举 | 名称 | 基础生命 | 基础伤害 | 基础移速 | 基础经验 | 基础护甲 | 抗性标签 |
| :--- | :--- | ---: | ---: | ---: | ---: | ---: | :--- |
| `UNDEAD` | 不死者 | 30 | 15 | 40 | 10 | 100 | Bleeding, Poison |
| `DEMON` | 恶魔 | 60 | 25 | 50 | 25 | 100 | Fire, Shadow |
| `CORRUPTED` | 腐蚀兽 | 25 | 20 | 60 | 15 | 100 | Stunned |
| `CULTIST` | 邪教徒 | 35 | 20 | 45 | 12 | 100 | Spell |
| `ElVES` | 堕落精灵 | 25 | 15 | 55 | 12 | 80 | 无 |
| `BEAST` | 兽人 | 40 | 18 | 45 | 15 | 90 | 无 |
| `GOBLIN` | 哥布林 | 20 | 10 | 55 | 8 | 100 | 无 |
| `MACHINE` | 机械兵 | 80 | 20 | 30 | 30 | 150 | Poison, Bleeding |
| `ELEMENTAL` | 元素精魂 | 50 | 30 | 40 | 20 | 120 | Physical |

抗性以位掩码 `Tag` 表示，直接写入 `EnemyRaceData`，不依赖脚本旁路。

### 2.2 派系（FactionType）

宿敌系统使用 4 个派系，`FactionComponent` 挂载于敌人实体标记其派系：

| 枚举 | 名称 |
| :--- | :--- |
| `Undead` | 亡灵 |
| `Void` | 异魔 |
| `Corrupted` | 腐化 |
| `Cultist` | 邪教 |

玩家侧以 `PlayerFactionAggro` 按派系累计仇恨，阈值 `NEMESIS_THRESHOLD = 100.0f`。

### 2.3 种族标签与约束（设计意图）

以下为按主题划分的种族标签与机制约束提案，尚未落地。

**亡灵（Undead）**

1. 常见标签：`[Undead]`、`[Physical]`
2. 机制：可声明对特定异常的免疫或减益抗性
3. 约束：免疫策略必须通过 Ailment 合同字段配置，不可脚本硬编码旁路

**异魔（Void）**

1. 常见标签：`[Void]`、`[Eldritch]`
2. 机制：偏高元素抗性，低物理防护
3. 行为：偏突进与后排压制

**腐化生物（Corrupted）**

1. 常见标签：`[Nature]`、`[Beast]`
2. 机制：群体联动、持续恢复或狂暴
3. 风险：高频恢复行为必须受预算策略约束

## 3. 行为模板（Archetypes）

`EnemyArchetype::Type` 定义 5 类原型，构造时按原型设置检测范围与攻击范围，攻击性行为默认以 `AIType::CHASE` 进入。

### 3.1 Fodder

1. 枚举 `FODDER`，检测范围 800，攻击范围 30
2. 以流场移动和简单追击为主
3. 目标是制造密度压力

### 3.2 Tank

1. 枚举 `TANK`，检测范围 800，攻击范围 40
2. 承伤与遮挡优先
3. 为后排怪创造输出窗口

### 3.3 Support

1. 枚举 `SUPPORT`，远离玩家并给友军施 Buff
2. Buff、Debuff、护盾、召唤支援
3. 所有支援效果通过统一事件与预算链路

### 3.4 Assassin

1. 枚举 `ASSASSIN`，检测范围 1000，攻击范围 35
2. 抓硬直窗口突袭
3. 爆发技能也需遵守触发深度与预算限制

### 3.5 Ranger

1. 枚举 `RANGER`，检测范围 1200，攻击范围 250
2. 保持远距离输出

### 3.6 AI 状态机（AIType）

`AIType` 为显式状态机，由 `AISystem` 以 `switch` 分派，非行为树：

1. `IDLE`：闲置，达到决策间隔后检测仇恨
2. `PATROL`：巡逻，往返于 `patrolStart` / `patrolEnd`
3. `CHASE`：追击，流场驱动（GPU）
4. `ATTACK`：攻击，进入攻击范围后停止移动
5. `FLEE`：逃跑，背离目标移动
6. `NEMESIS_HUNTER`：宿敌猎杀，主动搜索玩家
7. `SUPPORT_FLEE_BUFF`：支援，远离玩家并为友军施 Buff
8. `ASSASSIN_STEALTH`：刺客，潜行等待时机
9. `TANK_BLOCK`：坦克，阻挡视线保护远程友军

状态迁移：`IDLE` / `PATROL` 在仇恨范围内发现目标后，按原型进入 `TANK_BLOCK` / `ASSASSIN_STEALTH` / `SUPPORT_FLEE_BUFF`，其余进入 `CHASE`；带 `NemesisTag` 的实体进入 `NEMESIS_HUNTER`。`CHASE` 的目标进入攻击范围转 `ATTACK`；`ATTACK` 的目标超出攻击范围 `ATTACK_EXIT_MULT` 倍后回到 `CHASE`。

## 4. 精英词缀与 Boss 机制约束

1. 精英词缀必须映射为可验证合同字段，不得只做文案效果。
2. Boss 机制（阶段化、反制窗、惩罚）由 `combat_boss_framework` 统一约束。
3. Endgame 词缀通过 `combat_endgame_linker` 映射到战斗合同，不允许临时脚本跳过。

## 5. 宿敌系统（Nemesis）对接要求

1. 宿敌构成依赖玩家行为画像，但最终输出为标准怪物合同数据。
2. 宿敌反制能力（高抗、免控、反伤）必须可追踪且可调参。
3. 宿敌出现与奖励规则由终局系统控制，战斗层只负责合同执行。

## 6. 技术实现基线

### 6.1 ECS 组件

1. `AIComponent`：行为状态、目标、节流参数
2. `FactionComponent`：阵营关系与仇恨数据
3. `CombatStats`：统一战斗属性面板
4. `AilmentState` / `DefenseState`：合同运行态

### 6.2 更新分级（性能）

1. Level 0：屏内单位全量更新
2. Level 1：近屏单位降频更新
3. Level 2：远屏单位冻结或抽象

### 6.3 动态难度

推荐基式：

```text
Stats = Base * (1.08 ^ Tier) * (1 + Corruption / 100)
```

数值增长必须经过 release gate 指标验证，避免单项词缀导致指数爆炸。
