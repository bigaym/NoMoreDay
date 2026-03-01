# Modifier Runtime V2 设计草案（Draft）

日期：2026-02-28  
状态：Draft v0.1

## 1. 已锁定决策

- 一期范围：仅玩家侧，包含 `装备词缀 + 技能专精 + 天赋`。
- 不做向后兼容：直接进入 `schema_version = 2`，旧链路后续删除。
- 运行时产物：`binary blob + debug json`。
- 开发规范：以单元测试为主，手动测试矩阵补充体验与平衡验证。

## 2. 目标与非目标

### 2.1 目标

- 建立独立的 Modifier 子系统，作为长期可复用能力（当前项目与后续 ARPG 项目共用）。
- 将“装备词缀/专精节点/天赋节点”的效果表达统一为受限 DSL（白名单 Op）。
- 将内容配置离线编译为紧凑二进制，提升加载速度与运行时 cache 友好性。
- 提供可验证、可追踪、可回归的执行语义，避免继续扩展 `switch/case`。

### 2.2 非目标（一期不做）

- 地图词缀与怪物词缀迁移（仅预留接口，不改现有玩法链路）。
- 通用脚本虚拟机（Lua/自定义解释器）引入。
- 旧配置双轨长期共存。

## 3. 目标架构

### 3.1 模块拆分

新增目录：`src/game/systems/modifier/`

- `ModifierRuntimeTypes.hpp`：二进制布局定义、常量、枚举。
- `ModifierCompilerContract.hpp`：编译产物读取契约与版本校验。
- `ModifierRuntimeRegistry.{hpp,cpp}`：加载 `.bin`、持有只读表。
- `ModifierEvaluator.{hpp,cpp}`：按上下文执行过滤与 Op。
- `ModifierContext.hpp`：执行上下文（职业、技能、标签、节点、装备槽位等）。
- `ModifierApplyBridge.{hpp,cpp}`：将 evaluator 输出接入现有 `StatsSystem/AttributePipeline`。

### 3.2 领域适配器（一期）

- `EquipmentModifierAdapter`：装备词缀 -> Modifier 记录。
- `SkillSpecModifierAdapter`：技能专精节点 -> Modifier 记录。
- `TalentModifierAdapter`：天赋节点 -> Modifier 记录。

说明：适配器负责“取源数据+提供上下文”，执行语义只在 `ModifierEvaluator` 中维护一处。

## 4. Schema V2（内容层）

### 4.1 文件组织

- `assets/data/modifier_v2/equipment_modifiers.json`
- `assets/data/modifier_v2/skill_spec_modifiers.json`
- `assets/data/modifier_v2/talent_modifiers.json`
- `assets/data/modifier_v2/modifier_catalog.json`（可选，聚合索引与校验入口）

### 4.2 顶层字段

```json
{
  "schema_version": 2,
  "domain": "equipment",
  "records": []
}
```

### 4.3 Record 核心字段

- `id`：`uint32`，全局唯一。
- `domain`：`equipment | skill_spec | talent`。
- `priority`：执行优先级（小到大）。
- `filters`：上下文过滤（职业、技能、标签、武器、槽位、节点状态）。
- `constraints`：互斥组、最大生效数、唯一规则。
- `ops`：效果操作数组（白名单 OpCode）。
- `debug`：仅调试信息（名称、来源文案、注释）。

### 4.4 Filter 最小集合

- `profession_mask`：`uint8` 位掩码。
- `skill_id_whitelist`：`uint16[]`。
- `required_skill_tags_all`：`uint64`。
- `forbidden_skill_tags_any`：`uint64`。
- `weapon_class_mask`：`uint16`。
- `equip_slot_mask`：`uint32`。
- `node_id_whitelist`：`uint32[]`（专精/天赋节点）。

### 4.5 OpCode 白名单（一期）

- `ADD_STAT_FLAT`
- `ADD_STAT_PERCENT_ADD`
- `ADD_STAT_PERCENT_MULT`
- `ADD_SKILL_LEVEL`
- `MANA_COST_MULT`
- `COOLDOWN_MULT`
- `ADD_PROJECTILE_COUNT`
- `ADD_TAG`
- `REMOVE_TAG`
- `ENABLE_BEHAVIOR_FLAG`

说明：一期不引入脚本表达，所有效果必须落在可测的固定 OpCode 集合内。

## 5. Binary Runtime Layout（运行时层）

### 5.1 产物

- `assets/generated/modifier_runtime_v2.bin`
- `assets/generated/modifier_runtime_v2.debug.json`

### 5.2 Header

- `magic[4] = "NMDM"`
- `format_version = 2`
- `endianness = little`
- `record_count/op_count/filter_count/index_count`
- `offsets`：各表偏移（相对文件起始）
- `crc32`：内容校验

### 5.3 表结构（SoA/紧凑索引）

- `RecordTable`：定长记录，仅存索引与元信息。
- `FilterTable`：定长过滤器。
- `OpTable`：定长 Op 参数块。
- `IndexTable`：按域/技能/标签预构建倒排索引。

建议：以“定长结构 + 索引区间”替代运行时 vector of vector，减少碎片与分配。

### 5.4 运行时原则

- 启动一次加载，只读驻留。
- 执行无字符串比较，全部整数枚举 + bitmask。
- 顺序稳定：`priority -> id`，保证 deterministic。
- 失败策略：加载阶段报错并拒绝进入战斗运行（fail fast）。

## 6. 编译管线

新增脚本：`scripts/gen_modifier_runtime_v2.py`

支持命令：

- `python scripts/gen_modifier_runtime_v2.py`（生成产物）
- `python scripts/gen_modifier_runtime_v2.py --check`（仅校验）
- `python scripts/gen_modifier_runtime_v2.py --check-determinism`（确定性校验）

集成点：

- 将 `--check` 接入 `build.bat` 的 precheck 阶段。
- `scripts/validate_json.py` 增加 `modifier_v2` schema 检查入口。

## 7. 一期迁移计划（无兼容）

### M1：装备词缀迁移

- 将装备词缀效果迁入 `equipment_modifiers.json`。
- 打通 `requiredSkillTags -> required_skill_tags_all`。
- 旧 `AffixType` 分支保留到 M3 后统一删除。

### M2：技能专精迁移

- 将专精节点效果迁为 `skill_spec_modifiers.json`。
- 技能专精运行时从专用逻辑切到统一 evaluator。

### M3：天赋迁移

- 将天赋节点效果迁为 `talent_modifiers.json`。
- 天赋加点后的属性重算切到统一 modifier 链。

### M4：旧路径删除

- 删除重复计算分支与遗留解析逻辑。
- 收敛到单一来源与单一执行器。

## 8. 测试与验收

### 8.1 单元测试

- `SchemaValidation`：非法字段、越界值、互斥冲突。
- `CompilerDeterminism`：同输入二进制逐字节一致。
- `RuntimeLoad`：版本/CRC/偏移校验。
- `EvaluatorSemantics`：过滤命中、Op 叠加、优先级顺序。
- `RegressionSamples`：典型 BD 快照伤害/资源/冷却结果回归。

### 8.2 集成测试

- 换装 -> 属性更新 -> 技能面板数值一致。
- 专精切换/洗点 -> 战斗数值链路稳定。
- 天赋投入/重置 -> 角色实战表现与面板一致。

### 8.3 手动测试矩阵

- Build A：直伤暴击流（技能等级 + 伤害乘区验证）
- Build B：持续伤害流（标签过滤与 DOT 乘区验证）
- Build C：触发流（触发频率/资源消耗/冷却联动验证）

## 9. 风险与控制

- 风险：迁移期数据错误导致战斗数值突变。  
  控制：回归样本快照 + 单测阈值 + debug trace 对账。

- 风险：二进制布局演进破坏读取。  
  控制：header version + crc + 加载期硬校验。

- 风险：性能回归。  
  控制：批量角色重算 benchmark，设置预算阈值并纳入 CI 标签。

## 10. 里程碑建议

- P0（2-3 天）：Schema + Compiler + Runtime 空框架 + 基础测试。
- P1（3-5 天）：装备词缀全迁 + 关键回归。
- P2（3-5 天）：专精与天赋迁移 + 旧分支删除。
- P3（1-2 天）：调优、文档固化、验收与收尾。

## 11. 下一步

- 将本草案升级为 `v0.2`：补全字段级示例（每个 domain 各 3 条 record 样本）。
- 输出实现计划文档（任务粒度到文件与测试命令）。
- 开始 P0 落地分支开发。
