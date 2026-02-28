# 统一修饰器运行时系统（UMR）设计说明

最后更新：2026-03-01  
系统代号：`UMR`（Unified Modifier Runtime）

## 1. 系统定位

UMR 是玩家侧构筑数值的统一执行层。它将装备词缀、技能专精节点、天赋节点这三类修饰统一编译并执行，替代散落在多个系统中的硬编码分支。

一期边界（已落地）：

1. 装备词缀（含技能标签门槛）
2. 技能专精节点
3. 天赋节点（星盘）

二期预留（未迁移）：地图词缀、怪物词缀。

## 2. 设计目标

1. **单一执行路径**：所有玩家侧修饰效果统一进入 `ModifierEvaluator`。
2. **数据驱动**：效果定义在 JSON，不在 C++ 写死节点 id 或倍率常量。
3. **紧凑运行时**：离线编译为二进制 blob，运行时仅做整数/bitmask 运算。
4. **可测试可追踪**：Schema 校验、编译确定性、运行时一致性均有单测覆盖。

## 3. 核心架构

### 3.1 数据层（Schema V2）

目录：`assets/data/modifier_v2/`

- `modifier_catalog.json`
- `equipment_modifiers.json`
- `skill_spec_modifiers.json`
- `talent_modifiers.json`

每条 record 包含：

- `id`
- `domain`
- `priority`
- `filters`（职业、技能、标签、节点白名单等）
- `constraints`
- `ops`

### 3.2 编译层

脚本：`scripts/gen_modifier_runtime_v2.py`

产物：

- `assets/generated/modifier_runtime_v2.bin`
- `assets/generated/modifier_runtime_v2.debug.json`

编译器按 `(priority, id)` 稳定排序并生成结构化表：

1. `RecordTable`
2. `FilterTable`
3. `OpTable`
4. `IndexTable`

### 3.3 运行时层

模块：`src/game/systems/modifier/`

- `ModifierRuntimeRegistry` 负责加载和校验二进制（magic/version/offset/crc）。
- `ModifierEvaluator` 负责过滤命中与 Op 执行。
- 适配器负责收集上下文与 record ids：
  - `EquipmentModifierAdapter`
  - `SkillSpecModifierAdapter`
  - `TalentModifierAdapter`

## 4. 执行语义

当前 Op 覆盖（一期）：

1. `ADD_STAT_FLAT`
2. `ADD_STAT_PERCENT_ADD`
3. `ADD_STAT_PERCENT_MULT`
4. `ADD_SKILL_LEVEL`
5. `MANA_COST_MULT`（保留接线）

过滤命中基于：

1. 职业掩码
2. 技能白名单
3. 技能标签必须/禁止掩码
4. 武器/槽位掩码
5. 节点白名单

## 5. 一期实施结论

### 5.1 已完成

1. 装备词缀 record id 链路：`AffixDefinition.modifierRecordIds -> Affix.modifier_record_ids -> Adapter`。
2. 技能专精与天赋从硬编码常量迁为 runtime record 驱动。
3. `build.bat` precheck 接入 modifier runtime 生成校验。
4. 启动阶段加载 UMR 运行时二进制并校验。

### 5.2 关键约束

1. 业务逻辑禁止新增旁路；新修饰效果必须走 UMR。
2. 新增 OpCode 必须同步：schema 校验、编译器、evaluator、单测。
3. 修改 `ModifierRuntimeTypes.hpp` 的布局必须提高版本并补迁移说明。

## 6. 测试策略

单测重点：

1. Schema 结构合法性
2. 编译确定性（同输入同字节）
3. Runtime 加载完整性（header/offset/crc）
4. Evaluator 语义（filter + op）
5. Adapter 行为（装备/专精/天赋）

回归重点：

1. 换装实时属性与技能等级更新
2. 专精节点生效范围正确
3. 星盘节点加成与面板一致

## 7. 二期扩展建议

1. 地图词缀和怪物词缀引入独立 domain，复用编译器与 evaluator。
2. 增加事件型 Op（on-hit/on-kill）前，先定义预算与触发深度合同。
3. 增加性能基准测试，固定 95/99 分位预算阈值。
