# NoMoreDay 战斗系统完善实施方案（Combat vNext）

更新时间：2026-02-25  
目标：将当前战斗系统升级为“统一结算、语义清晰、可平衡、可压测、可长期运营”的 ARPG 级系统  
适用范围：M1-M3 三阶段落地（兼容现有内容，不做一次性推翻）

---

## 1. 设计目标与非目标

## 1.1 设计目标

1. 统一伤害口径，所有伤害必须经 `DamagePipeline`。
2. 明确 Hit / DoT / Ailment 三类语义边界和事件合同。
3. 召唤物成为一级战斗子系统，具备完整归因、缩放、预算治理。
4. 建立可量化预算与发布门禁，支撑高压场景稳定运行。
5. 保持与现有 `SkillSystem`、`BladeFormation`、`SpiritSword` 的增量兼容。

## 1.2 非目标

1. 不重做渲染主架构（保持现有 RenderGraph 路线）。
2. 不一次性重写全部技能行为脚本。
3. 不在本轮引入新的职业大系统，仅做战斗合同收敛与扩展地基。

---

## 2. 目标架构（权责收敛）

## 2.1 系统职责重定义

1. `DamagePipeline`
- 唯一数值计算入口（Hit/DoT/Trigger/Summon 全部统一）。
- 负责：基础池、转换、增伤、More、防御、暴击、预算钩子。

2. `CombatSystem`
- 退化为生命结算与战斗动作层。
- 负责：命中判定结果消费、`ApplyDamage`、死亡与受击流程。
- 不再承载新的数值计算公式。

3. `EffectSystem`
- 负责状态驱动与 tick 调度。
- DoT tick 只做调度，不做“只算不扣”的悬空流程。

4. `SkillSystem`
- 负责施法生命周期与触发派发。
- 触发合同参数（含 `trigger.effectiveness`）进入结算上下文。

5. `SummonSystem`（拆分后）
- `SummonLifecycleSystem`：生成、续期、销毁、归属变更。
- `SummonAISystem`：目标选择与行为状态机。
- `SummonCombatBridge`：召唤伤害归因、继承、预算、统一进入 `DamagePipeline`。

---

## 3. 核心合同规范（必须落实）

## 3.1 Damage Contract

### 语义标签合同

1. Hit：必须包含 `Tag::Hit`，不得包含 `Tag::DamageOverTime`。
2. DoT：必须包含 `Tag::DamageOverTime`，不得走 Hit 收益分支。
3. Ailment：通过 `AilmentEngine` 统一派发和结算，不走临时脚本特判。

### 公式合同

统一基式：

`FinalDamage = ((Base + Added * AddedEff) * (1 + Increased) * More * TriggerEff) * Mitigation`

其中：

1. `AddedEff` 来自 `SkillData.added_damage_effectiveness`。
2. `TriggerEff` 来自 `NodeContractData.trigger.effectiveness`。
3. `Mitigation` 包含护甲、抗性、格挡、减伤顺序合同（见 3.3）。

## 3.2 Event Contract

1. 事件值必须与实际承伤值一致。
2. `CalculateBatch` 中所有事件使用 `final_damage`。
3. 统一事件载荷字段：
- `reported_damage`
- `final_applied_damage`
- `damage_tags`
- `owner/summon/source_skill`（有召唤时必填）

## 3.3 Defense Contract

建议顺序：

1. 命中与闪避判定
2. 格挡判定
3. 护甲/抗性减免
4. 全局减伤与特殊减伤
5. 屏障/护盾吸收
6. 生命值结算与死亡判定

要求：

1. 顺序固定且单一实现。
2. 各步结果可日志追踪（调试开关可控）。

## 3.4 Ailment Contract

每种异常统一声明以下参数：

1. `max_stacks`
2. `refresh_policy`（刷新持续 / 叠层延长 / 独立实例）
3. `overwrite_policy`
4. `immunity_and_resistance`
5. `tick_interval` 与 `damage_pool_policy`

## 3.5 Summon Contract

三元归因必须强制：

1. `owner`：收益归属
2. `summon`：执行实体
3. `source_skill`：技能来源

缩放模式：

1. `Dynamic`：实时读取主人关键属性与标签
2. `Snapshot`：生成时快照
3. `Mixed`：按白名单字段动态，其余快照

禁止项：

1. 禁止同来源重复乘区。
2. 禁止召唤路径绕过 `DamagePipeline` 直接写生命值。

## 3.6 Proc Budget Contract

预算维度：

1. `life_on_hit_per_sec`
2. `mana_on_hit_per_sec`
3. `ailment_proc_per_sec`
4. `trigger_proc_per_sec`
5. `event_emit_per_frame`

预算策略：

1. 先扣预算再触发收益。
2. 超预算按策略降采样或延迟，不可无限放行。

---

## 4. 数据模型建议（增量演进）

以下为建议结构，保持与现有 ECS 兼容：

```cpp
enum class SummonInheritMode : uint8_t { Snapshot, Dynamic, Mixed };
enum class SummonRole : uint8_t { Melee, Ranged, Support, Orbit };
enum class SummonCommandMode : uint8_t { Passive, Defend, Assist, Aggressive };

struct SummonComponent {
  entt::entity owner = entt::null;
  uint32_t source_skill_id = 0;
  float lifetime = 0.0f;
  float max_lifetime = 0.0f;
  bool persistent = false;
  uint16_t archetype_id = 0; // 用 ID 取代热路径 string
};

struct SummonCombatProfile {
  float damage_scale = 1.0f;
  float attack_speed_scale = 1.0f;
  SummonInheritMode inherit_mode = SummonInheritMode::Dynamic;
  float proc_budget_per_sec = 8.0f;
  float event_budget_per_sec = 64.0f;
};

struct SummonAIProfile {
  SummonRole role = SummonRole::Melee;
  SummonCommandMode command_mode = SummonCommandMode::Assist;
  float retarget_interval = 0.2f;
  float leash_radius = 320.0f;
};

struct SummonRuntimeState {
  entt::entity current_target = entt::null;
  float attack_cd = 0.0f;
  float proc_budget_runtime = 0.0f;
  float event_budget_runtime = 0.0f;
};
```

---

## 5. 分阶段落地计划（执行级）

## Phase A（P0 收敛，2-3 周）

目标：修正确认性错误，统一核心口径。  
任务：

1. DoT tick 必须 `Calculate + ApplyDamage` 闭环。
2. DoT 强制 `Tag::DamageOverTime`。
3. `CalculateBatch` 事件值改用 `final_damage`。
4. `added_damage_effectiveness` 接入主公式。
5. `trigger.effectiveness` 注入触发执行上下文。
6. `CombatSystem::CalculateDamage` 标记为兼容层，禁止新调用。

验收：

1. DoT 伤害与生命变化一致。
2. 事件值与承伤一致率 >= `99.9%`。
3. 回归矩阵（单体/AoE/DoT/Trigger）全通过。

## Phase B（召唤合同化，2-4 周）

目标：将召唤路径统一纳入主战斗合同。  
任务：

1. 提取 `SummonCombatBridge`，所有召唤伤害走 `DamagePipeline`。
2. `owner/summon/source_skill` 三元归因进入事件载荷。
3. 灵剑路径迁移为首个样板，保留原视觉表现。
4. 移除热路径 string 依赖（`SummonComponent.name` 下沉到非热层）。

验收：

1. 灵剑流派伤害、回蓝、触发行为与设计区间一致。
2. 召唤路径无直写生命值分叉。
3. 召唤事件链无漏发、无重复发。

## Phase C（系统深度，3-5 周）

目标：建立长期可运营的规则治理层。  
任务：

1. AilmentEngine v1（叠层/刷新/覆盖/免疫）。
2. DefenseContract v1（固定结算顺序）。
3. ProcBudgetManager v1（按秒预算与帧预算）。
4. 构筑反约束（互斥 Keystone、递减机制）接入。

验收：

1. 多召唤 + 高攻速场景无指数收益爆炸。
2. 直伤/DoT/召唤/触发四流派均可达标。
3. 平衡调参可配置化完成，不依赖硬编码热修。

---

## 6. 测试与验证策略

## 6.1 自动化测试矩阵

1. 单元测试：
- 公式正确性
- 标签语义隔离
- 触发深度与预算拦截

2. 集成测试：
- 单体/Boss/AoE/高攻速/多召唤
- 触发链 + 异常状态 + 防御系统联动

3. 配置一致性测试：
- `python scripts/gen_skill_contracts.py --check` 必须通过
- 新技能/新节点 PR 必须附合同校验结果

## 6.2 观测指标

1. `DamagePipeline`：avg/p95/p99
2. `StatsSystem::GetStatWithTags`：调用次数、缓存命中率、锁等待
3. `CombatEventDispatcher`：每帧总量与类型分布
4. Trigger：拦截率、深度分布、每秒调用量
5. Summon：单位数、目标切换频率、事件量、预算命中率

---

## 7. 风险与回滚策略

1. 风险：入口统一期间出现技能回归  
回滚：保留旧链兼容开关，仅用于回退，不接入新逻辑。

2. 风险：召唤迁移导致既有专精行为漂移  
回滚：灵剑先行灰度，逐技能迁移，使用对照日志比对。

3. 风险：预算策略导致体感“削弱过猛”  
回滚：预算参数配置化并提供分层阈值，先软限制后硬限制。

4. 风险：Ailment 新合同与旧 buff 冲突  
回滚：双轨期间由 AilmentAdapter 做兼容映射，逐步下线旧字段。

---

## 8. 交付标准

达到以下条件后，本方案可标记为“战斗系统完成度达标”：

1. P0 全部关闭且无回归。
2. Summon 合同化完成并通过高压回归。
3. Ailment + Defense + ProcBudget 三大合同上线。
4. 配置校验、性能门禁、战斗观测全部接入 CI/nightly。
