# 剑修剩余专精一次性完成设计（天剑 + 魔剑）

日期：2026-03-07  
状态：Design Approved

## 1. 背景与目标

`剑圣` 已经完成基础职业接管、专精选择、专精资源、招牌技能、专精树、HUD 与测试闭环。当前剩余工作不是重做框架，而是把 `天剑` 与 `魔剑` 以相同完成标准一次性补齐，避免再次经历 `MVP -> 补树 -> 补 HUD/VFX -> 补回归` 的拆轮返工。

本轮目标：

- 在现有 `Blade Ascendant` mastery 框架上补齐 `天剑` 与 `魔剑`
- 两个专精均按“完整交付”标准落地，而非先做半成品
- 把共享底座扩展、专精纵切实现、HUD/VFX 可读性、完整验证矩阵放入同一轮
- 保持改动聚焦在剑修专精，不反向重构无关技能系统

## 2. 近期经验总结（来自 Sword Saint）

- 共享骨架已经存在：`blade_masteries.json`、`mastery_skill_trees.json`、`BladeMasteryService`、`BladeResourceService`、`SwordIntentWidget`、存档与 UI 入口都已打通。
- 真正耗时项集中在“资源语义改写 + 招牌技能树节点落地 + HUD/反馈可读性 + 回归测试同步”。
- 因此本轮不应按“先做天剑，再重复一遍流程做魔剑”推进，而应先扩共享层，再分两条纵切完成专精内容，最后统一收尾。
- `Seven Star Slash` 的完成路径证明：专精签名技能仍应走普通技能数据/行为注册链路，专精树则走 `mastery_skill_trees.json` 的独立合同与数据结构。

## 3. 范围冻结

### 3.1 本轮包含

- `HeavenlySword` / `DemonBlade` mastery profile 与 resource kind
- `灵剑阶`、`嗜血/血誓` 的运行时语义、消耗/生成/衰减/展示逻辑
- `天剑降临`、`血海` 两个招牌技能的技能数据、行为注册、专精树合同与节点实现
- 两个专精的核心联动技能改造
- mastery 选择 UI、HUD 资源展示、必要的视觉阈值提示
- 存档、读档、合同校验、单元/集成/功能/UI 回归

### 3.2 本轮不包含

- 新的剑修基础技能
- 装备偏转、独特装备、传奇词缀上位路线
- 全职业通用 UI 重做
- 大规模 VFX 框架翻修或音频系统扩建
- 与本轮无关的技能树重构

## 4. 锁定决策

- 继续沿用现有 mastery 架构；不重写 `SkillRegistry + SkillSystem + specialization tree` 主链路。
- `天剑` 与 `魔剑` 都按完整交付完成：资源、招牌技能、完整专精树、HUD/UI、测试、收尾抛光在同一轮交付。
- 招牌技能仍作为普通技能注册到 `assets/data/skills.json`，建议占用新的连续 skill id：`11` = `天剑降临`，`12` = `血海`。
- 专精特有大树继续落在 `assets/data/mastery_skill_trees.json`，不回灌到普通 `skills.json` 的基础树里。
- 共享逻辑尽量数据驱动；只有在元素调谐、生命代法力等无法纯数据表达时，再向 `BladeResourceService` / 技能行为增加明确分支。
- 不再拆单独的 “post-MVP polish” 轮次；资源阈值提示、释放窗口提示、必要的 VFX/HUD 反馈在本轮完成。

## 5. 总体交付架构

### 5.1 Phase A：共享准备层

一次性扩展三专精通用底座：

- `BladeMasteryId` 增加 `HeavenlySword`、`DemonBlade`
- `BladeResourceKind` 增加 `SpiritBladeTier`、`Bloodthirst`
- `BladeMasteryProfile` 增加两专精 profile
- `BladeResourceComponent` / `BladeSignatureSkillComponent` 增加必要状态位
- `SaveData` / `SaveManager` 增加调谐元素与血誓语义的持久化字段
- `UISkillHub`、`PlayerHUD`、`SwordIntentWidget` 支持三种资源模式切换

该阶段只做“让共享系统知道还有两个专精存在”，不提前写大段专精技能逻辑。

### 5.2 Phase B：天剑纵切

围绕 `灵剑阶` 完成一整条可玩的中远程法剑循环：

- 资源主题：元素调谐、灵剑显化、构场、轰炸、消耗层数打爆发
- 招牌技能：`天剑降临`
- 核心联动技能：`灵剑决`、`万剑归宗`、`剑阵·诛仙`、`裂空斩`、`心剑·无影`
- 交付标准：完整树节点、元素适配、场域联动、HUD 阈值与当前调谐提示

### 5.3 Phase C：魔剑纵切

围绕 `血誓` / `嗜血` 完成一整条高风险低血循环：

- 资源主题：生命代法力、缺血增益、吸血回正、危险窗口、收割追击
- 招牌技能：`血海`
- 核心联动技能：`绝影绝剑`、`御剑·回旋`、`剑气护体`、`流云刺`、`心剑·无影`
- 交付标准：完整树节点、生命消耗防护、回血/溢出护盾、危险阈值反馈

### 5.4 Phase D：统一收尾层

最后统一完成：

- HUD 阈值提示、资源名称/颜色/图标模式切换
- 招牌技能释放反馈与消耗后窗口提示
- 必要的 VFX/文本提示最小集
- 完整 build + ctest + targeted doctest 验证矩阵
- 文档同步与平衡风险记录

## 6. 天剑设计落地

### 6.1 资源模型

- `灵剑阶` 直接复用现有剑修资源栏位，但切换显示语义与获取/消耗规则。
- 资源上限维持 `10`，获得一层时在角色身后显化一柄灵剑。
- `天剑` 选择一种调谐元素（雷 / 冰 / 火），其 50% 物理转为当前元素。
- 资源默认不自动消耗；`天剑降临` 与少量树节点在爆发时消耗层数。

### 6.2 签名技能落点

`天剑降临` 必须保留“双段身份”：

- 落地瞬间冲击负责爆发与定位
- 后续 5 秒领域负责持续压制与与其他御剑技能联动

其树节点应完整覆盖：

- 重击 / Boss 压制
- 消耗灵剑阶后的回流与连段
- 与 `剑阵·诛仙`、`万剑归宗`、`灵剑决` 的同源联动
- 调谐元素派生效果

### 6.3 核心技能联动

- `灵剑决`：灵剑存在数、攻击频率、元素异常与 `灵剑阶` 建立直接耦合
- `万剑归宗`：作为远程建场与消耗前铺垫主件；其命中应能被 `天剑降临` 领域识别
- `剑阵·诛仙`：承担场域共鸣与元素吃色；必须打通 `剑阵同调`
- `裂空斩`：补单体/补传播/补回响；触发与调谐元素一致
- `心剑·无影`：提供高压持续命中源，参与领域内的同源统计

### 6.4 HUD / 可读性要求

- 明确展示当前调谐元素
- 资源条区分 `灵剑阶` 与普通 `剑意/剑势`
- 达到 `5+ / 7+ / 10` 层时给出释放 `天剑降临` 的阈值提示
- 领域激活期间给出剩余持续时间或明显场域反馈

## 7. 魔剑设计落地

### 7.1 资源模型

- `魔剑` 选中后，法力技改为以生命支付，默认生命消耗为原法力消耗的 `200%`。
- `嗜血` 取代 `剑意` 作为风险资源：每层提供 `5% More Damage` 与 `3% Increased Damage Taken`，上限 `10`。
- 资源主要来源：生命施法、低血近战命中、溢出吸血、保命触发。
- 资源核心张力：鼓励压低生命，但必须保留“会死”的真实风险。

### 7.2 签名技能落点

`血海` 保留“移动追猎领域”身份：

- 施放时消耗全部 `嗜血`
- 在角色周围生成跟随性血雾领域
- 同时承担持续伤害、治疗回补、低血处决与危险窗强化

其树节点应完整覆盖：

- 斩杀与贴身爆裂
- 回复、溢出护盾、释放后返血
- 与 `绝影绝剑` 的保命窗口联动
- 两个形态改写与虚空偏转路线

### 7.3 核心技能联动

- `绝影绝剑`：承担魔剑“危险窗 / 免死 / 爆发开关”的主锚点
- `御剑·回旋`：高频 hit、回手、流血与追击，是 `血海` 稳定叠加器
- `剑气护体`：承接低血防守、格挡/反击与生命成本压力
- `流云刺`：承担近战切入、低血补层、追击重置
- `心剑·无影`：作为站场压制和元素/虚空侵蚀副核

### 7.4 HUD / 可读性要求

- 资源条明确标识为 `嗜血`
- 低生命阈值（如 35% / 20%）要有清晰危险提示
- 生命代法力施法时，浮字或 HUD 文案应区分“生命支付”而非普通受伤
- `血海` 激活期间要能读出剩余时间、回血效率增强或危险窗加成

## 8. 风险与缓解

### 8.1 天剑风险

- `万象轮转` 若回层过多，容易抹掉消耗层数的决策成本  
  缓解：回层严格上限，且只在领域判定命中时返还。

- 领域联动技能过多，可能出现“命中来源识别”不统一  
  缓解：统一在行为层抽象“领域可识别的剑系命中来源”。

### 8.2 魔剑风险

- `饮海而生` 若回复过强，会把低血流变成近乎无代价常驻  
  缓解：溢出护盾、返血和减伤分别设上限，不叠成无限续航。

- 生命代法力若直接改写全局施法，容易误伤其他职业  
  缓解：严格限制在 `Blade Ascendant + DemonBlade` 组合下生效。

### 8.3 共享风险

- 两专精都复用同一资源组件，若塞入太多临时字段会变脆  
  缓解：只把跨专精共用状态放组件，专精特有状态优先放 service/behavior。

## 9. 验证矩阵

### 9.1 定向验证

- `tests/unit/BladeMasteryTests.cpp`：profile/resource kind/解锁与模式切换
- `tests/integration/SkillSystemTests.cpp`：资源生成/消耗/生命代法力/调谐传播
- `tests/functional/SkillBehaviors.cpp`：`天剑降临`、`血海` 与核心联动技能行为
- `tests/integration/SkillContractRegistryTests.cpp`：专精树合同、trigger/transmuter/keystone 约束
- `tests/tech/UITests.cpp`：skill hub、HUD、resource widget 模式切换

### 9.2 仓库标准命令

- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

## 10. 影响范围（首轮预估）

- `assets/data/blade_masteries.json`
- `assets/data/skills.json`
- `assets/data/mastery_skill_trees.json`
- `assets/data/skill_contracts_compact.json`
- `src/game/data/BladeMasteryData.hpp`
- `src/game/data/SaveData.hpp`
- `src/engine/persistence/SaveManager.cpp`
- `src/game/components/SkillDefs.hpp`
- `src/game/systems/skill/BladeMasteryService.*`
- `src/game/systems/skill/BladeResourceService.*`
- `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`
- `src/game/systems/skill/behaviors/BladeFormation.cpp`
- `src/game/systems/skill/behaviors/InfiniteBlades.cpp`
- `src/game/systems/skill/behaviors/SwordArray.cpp`
- `src/game/systems/skill/behaviors/RendingWave.cpp`
- `src/game/systems/skill/behaviors/MindBlade.cpp`
- `src/game/systems/skill/behaviors/BladeWard.cpp`
- `src/game/systems/skill/behaviors/BladeBoomerang.cpp`
- `src/game/systems/skill/behaviors/FlowingThrust.cpp`
- `src/game/systems/skill/behaviors/PhantomFlash.cpp`
- `src/game/systems/ui/UISkillHub.cpp`
- `src/game/systems/ui/PlayerHUD.cpp`
- `src/game/systems/ui/SwordIntentWidget.*`
- `tests/unit/BladeMasteryTests.cpp`
- `tests/integration/SkillSystemTests.cpp`
- `tests/integration/SkillContractRegistryTests.cpp`
- `tests/functional/SkillBehaviors.cpp`
- `tests/tech/UITests.cpp`

---

本设计采用“共享底座一次扩完、两个专精纵切完成、最后统一收尾”的路径，目标是在一轮内交付 `天剑` 与 `魔剑` 的完整可玩版本，而不是制造新的尾款阶段。
