# Blade Ascendant Skill Behaviors Refactor - 规格说明书

> **Track ID**: `blade_ascendant_skill_behaviors_20260221`  
> **状态**: Pending  
> **依赖**: `blade_ascendant_skill_contracts_20260221`

---

## 1. 目标

将技能行为从当前“分技能硬编码 + 弱约束”重构为“合同驱动行为执行”，重点覆盖：

- 9 技能分支行为一致性
- Trigger 链路防无限触发
- 剑意、御剑步、元素互斥转化的统一执行逻辑

## 2. 行为执行模型

```cpp
struct SkillExecutionContext {
  uint32_t skill_id = 0;
  uint64_t cast_id = 0;
  bool is_empowered = false;
  bool is_shadow_cast = false;
  Tag effective_tags = Tag::None;
  entt::entity caster = entt::null;
  Vector2 origin{0, 0};
  Vector2 target{0, 0};
};

struct TriggerBudget {
  uint8_t depth = 0;
  uint8_t max_depth = 2;
  bool exhausted = false;
};
```

- `SkillBehaviorRegistry` 继续作为入口，但读取合同后执行统一 guard：
  - Trigger CD guard
  - Trigger depth guard
  - Transmuter 互斥 guard
  - ScopePolicy guard

## 3. ECS 影响面

- 修改系统
  - `SkillSystem`
  - `DamagePipeline`（作用域判定）
  - `BehaviorInjectionRegistry`
- 保持兼容的组件
  - `ActiveSkillsComponent`
  - `SwordIntentComponent`
  - `BladeFormationComponent`
  - `ChannelingComponent`

## 4. 持久化与兼容

- 保持 `ActiveSkillsComponent` 存档结构不破坏
- 新行为逻辑以合同字段驱动，不新增必须存档字段（仅依赖 Track 1 的运行态字段）
- 旧存档加载后可继续释放技能，不要求玩家重置天赋

## 5. 验收标准

- [ ] 9 技能至少 1 条主分支和 1 条关键分支行为可执行
- [ ] Trigger 节点均有内置 CD，且触发深度不会超过 2
- [ ] Transmuter 互斥生效（同技能不可能同时激活 2 条元素转化）
- [ ] 剑意满层强化、消耗、回流逻辑通过单测
- [ ] 关键行为回归测试通过（含裂空斩、灵剑决、御剑回旋、绝影）
- [ ] `build.bat` 编译通过

