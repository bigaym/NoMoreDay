# 装备系统演进设计（自拾定向 + 预算锻造）

日期：2026-02-27  
状态：Design Approved

## 1. 背景与目标

当前项目已具备以下装备系统基础：

- 底材 + 稀有度 + 词缀体系（含 T1-T7）
- 锻造潜能（FP）驱动的打造流程
- LP 传奇融合与符文语机制
- 掉落过滤与基础可视化链路

本次演进不推翻现有系统，而是在既有实现上强化长期循环，目标是：

- 让玩家能“定向追逐”目标底材，而非仅依赖全随机
- 让锻造从“能做”进化为“有策略可计算”
- 保留自拾可毕业路径，避免交易依赖
- 建立可感知的终局超级目标，稳定中长期留存

## 2. 设计原则

- 自拾优先：核心成长路径不依赖拍卖或高频交易
- 复杂度后置：减少掉落筛选负担，把深度放到打造决策
- 模块可插拔：掉落系统按职责拆分，便于赛季化扩展
- 可观测可回滚：关键模块具备 feature flag 与统计追踪

## 3. 目标架构

### 3.1 主循环

`选目标路线 -> 刷定向底材 -> FP 锻造成可用装 -> 冲高阶词缀/LP -> 传奇融合终局件`

### 3.2 模块拆分

将掉落流程拆分为四个核心模块，行为解耦但接口统一：

1. `DropSourceResolver`：解析掉落来源（怪物/区域/事件）
2. `BaseSelector`：选择底材与基础参数
3. `RarityAffixRoller`：决定稀有度与词缀生成
4. `DropPostProcessor`：执行过滤、强调、日志追踪

新增扩展模块：

- `TargetDropModule`：定向掉落规则（P2 接入）
- `SeasonalModifierModule`：赛季修正规则（P3 接入）

## 4. 数据与接口设计

### 4.1 上下文与结果对象

统一掉落输入输出，降低模块耦合：

- `DropContext`
  - `areaLevel`
  - `sourceId`
  - `targetPoolId`
  - `magicFind`
  - `difficulty`
  - `seed`
- `DropResult`
  - `base`
  - `rarity`
  - `affixes`
  - `flags`
  - `debugTraceId`

### 4.2 配置扩展

`assets/data/loot_tables.json` 增加：

- `target_pool_id`：定向池标识
- `target_weight`：定向池权重
- `source_tags`：来源标签（Boss/区域/事件）

`assets/data/affixes.json` 增加：

- `craft_group`：锻造组别
- `mutual_exclusion_group`：互斥组
- `tier_source`：词缀来源（`drop_only` / `craftable`）

## 5. 分阶段落地

### P1：稳定拆分层（行为等价）

- 引入 `DropContext` 与 `DropResult`
- 完成四模块拆分并接回现有链路
- 要求默认配置下掉落分布与旧版本偏差小于 3%

### P2：定向追逐层（主线强化）

- 接入 `TargetDropModule`
- 对 1-2 条 Boss/区域路线做白名单试点
- 保证 30 分钟内可稳定获得“可用胚子”

### P3：赛季扩展层（可开关）

- 接入 `SeasonalModifierModule`
- 支持赛季词缀池/掉落权重修正
- 模块关闭时分布应回退至 P2 基线

## 6. 锻造系统增强策略

在 `CraftingSystem` 中新增“策略钩子”，默认开关关闭，先小规模启用：

- 优先上线 1 个高价值操作（如保底类）
- 保持 FP 主循环不变，避免系统突变
- 同步补充 UI 提示与日志字段，确保玩家可理解

## 7. 终局目标设计

围绕 LP 融合与 Ancient 产出提供里程碑进度：

- 赛季追踪 1-2 个超级目标
- 强调“多段可感知进度”，而非一次性毕业
- 不改变核心融合公式，先加强目标表达与追踪能力

## 8. 观测、验证与回滚

### 8.1 观测

- 增加 `drop_trace` 采样日志（模块决策路径 + 关键权重）
- 增加统计指标：定向池命中率、可用胚子率、毕业时长中位数

### 8.2 验证

- 分布验证：稀有度、词缀层级、底材类型分布
- 体验验证：30 分钟可见提升、2 小时有明确追逐方向
- 性能验证：高掉落密度下帧时长无显著回归

### 8.3 回滚

所有新增能力挂 feature flag：

- `drop.target_pool.enabled`
- `crafting.strategy_hooks.enabled`
- `drop.seasonal_modifier.enabled`

支持分模块回退，避免整体开关导致风险扩大。

## 9. MVP 范围（首发最小集）

1. P1 全量落地（掉落模块解耦）
2. P2 最小定向池（至少两条目标路线）
3. 锻造策略钩子上线 1 个操作并支持开关
4. LP/Ancient 终局进度追踪 UI（不改融合核心公式）

## 10. 影响范围（初版）

- `src/game/systems/item/DropSystem.cpp`
- `src/game/systems/item/ItemFactory.cpp`
- `src/game/systems/item/CraftingSystem.cpp`
- `src/game/systems/item/LootFilter.cpp`
- `assets/data/loot_tables.json`
- `assets/data/affixes.json`

## 11. 非目标（本轮不做）

- 交易市场体系重构
- 全新稀有度阶层重做
- 彻底替换现有词缀与锻造底层公式

---

本设计采用“低风险演进”路径：先拆分与观测，再增强与扩展，确保系统可持续迭代。
