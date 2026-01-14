# 详细设计规范：剑修技能专精树重构 (Skill Spec Tree Refactoring)

## 1. 架构目标 (Architecture Goals)
将剑修的技能系统从“硬编码逻辑”演进为“数据驱动+组件注入”的混合模式。支持 9 个核心技能的 4 分支专精树，实现复杂的形态转化（如残影、折返、自动追踪），并确保数值计算层具有极高的扩展性，能无缝响应装备与套装的全局修正。

## 2. 核心系统与设计逻辑 (Core Systems)

### 2.1 专精节点数据结构 (JSON Schema)
每个技能的 `specialization_tree` 将包含以下定义：
- **NodeID**: `skill_id_branch_layer_index` (例如 `flowing_thrust_A_1_1`)。
- **Branch**: `[A, B, C, D]`。由于天然适合横屏，A/B 位于左侧（近战/影杀），C/D 位于右侧（爆发/元素）。
- **Logic Type**:
    - `STAT_MOD`: 修改标签或数值（例如 `+20% Inc Physical Damage`）。
    - `BEHAVIOR_INJECT`: 注入特殊组件 ID（例如 `shadow_caster`, `boomerang`）。
    - `TAG_CONVERSION`: 标签强制转化规则（例如 `Physical -> Fire`）。
- **Scaling Context**: 包含 `base_value` 和 `scaling_tag`，用于在 C++ 层匹配全局修饰符。

### 2.2 组件注入与行为引擎 (Behavior Injection)
采用 **A 模式（组件注入）**，定义以下核心组件：
- **`ShadowCasterComponent`**:
    - **属性**: `inheritDamageMult` (默认 0.3), `lifetime`, `maxActions` (重复次数)。
    - **边界逻辑**: 残影实体必须持有 `ShadowTag`，在 `RenderSystem` 中应用水墨着色器，在 `CombatSystem` 中其伤害来源追溯至玩家。
- **`BoomerangComponent`**:
    - **参数**: `turnBackDistance`, `acceleration`, `isReturning` 状态位。
    - **逻辑**: 当投射物位移达到阈值，强制反转 `Velocity` 向量并应用向心力指向玩家。

### 2.3 属性转换与标签引擎 (Tag & Damage Pipeline)
- **转换优先级**: 严格遵循 `设计文档/核心战斗与角色设计.md` 中的单向转化链。
- **动态修正**: `DamagePipeline` 增加 `PreCalculate` 钩子，扫描专精树激活状态并应用 `TagConversion`。

## 3. 边界条件与约束 (Boundaries & Constraints)

### 3.1 残影实体上限 (Shadow Capping)
- **硬限制**: 每个技能实例产生的残影总数不得超过 10 个（防止无限递归和过度渲染）。
- **生命周期**: 动作结束或持续时间到期必须通过 `DelayedDestroyComponent` 清理，严禁产生内存孤岛。

### 3.2 技能切换与持久化
- **状态一致性**: 玩家在释放技能期间切换专精点（若 UI 允许），当前已发出的投射物不回溯修改属性，仅后续释放生效。
- **同步频率**: 专精数值修正仅在“天赋点变动”时重新计算缓存到 `PlayerStatCache`，不应在每一帧 `Update` 中遍历 JSON 数据。

### 3.3 数值计算边界
- **外部修正系数**: 所有来自专精树的 `More` 乘法应独立累乘。
- **公式**: `FinalDamage = (Base + GlobalFlat) * (1 + Sum(Increased)) * Product(1 + More_i) * (1 + GlobalEquipmentMult)`。

## 4. 技术实现路径 (Technical Roadmap)

### 4.1 Python 自动化管道 (Python Pipeline)
- **脚本目标**: `scripts/sync_skills_spec.py`。
- **逻辑**: 读取 `草案.md` -> 提取节点文本 -> 按正则匹配分支标签 -> 映射到 JSON 模板 -> 增量写入 `assets/data/skills.json`。

### 4.2 C++ 模块化重构
1.  **Phase 1**: 扩展 `SkillComponent` 以容纳专精激活位图 (bitset)。
2.  **Phase 2**: 实现 `ShadowSystem` 和墨痕渲染材质。
3.  **Phase 3**: 按技能逐一重构（流云刺 -> 裂空斩 -> ...），每完成一个技能进行一次功能闭环测试。

## 5. 验收标准 (Acceptance Criteria)
- [ ] Python 脚本能将 Markdown 文档的变更无损同步到 JSON。
- [ ] 激活“残影”专精后，流云刺在起点产生视觉上具有水墨感的镜像实体，并正确造成 30% 伤害。
- [ ] “物理转元素”专精能正确触发对应的打击粒子（如火花、电弧）。
- [ ] 装备系统提供的“残影伤害 +10%”能正确作用于残影实体。
