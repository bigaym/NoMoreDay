# Blade Ascendant Skill Contracts Refactor - 规格说明书

> **Track ID**: `blade_ascendant_skill_contracts_20260221`  
> **状态**: Pending  
> **输入设计**: `设计文档/职业设计草案_剑修.md`, `设计文档/特效和UI/GPU_Rendering_Quick_Reference.md`

---

## 1. 目标

将当前剑修技能系统从“松散节点配置 + 行为分散硬编码”重构为“合同化技能数据层”，并为后续行为与渲染重构提供稳定 ABI/接口。

## 2. Data Model

```cpp
enum class SpecNodeRole : uint8_t {
  Passive,
  Keystone,
  Trigger,
  Synergy,
  Transmuter
};

enum class ResistModel : uint8_t {
  None,
  TypeA_Penetration,
  TypeB_Shred,
  TypeC_Exposure,
  TypeD_StatToPenetration,
  TypeE_CapSuppression
};

enum class ScopePolicy : uint8_t {
  SkillOnly,
  GlobalWhileBuffActive,
  GlobalAlways
};

struct TriggerContract {
  uint32_t trigger_skill_id = 0;
  float effectiveness = 0.0f;
  float internal_cooldown = 0.0f;
  bool consumes_mana = false;
};

struct NodeContractData {
  uint32_t node_id = 0;
  SpecNodeRole role = SpecNodeRole::Passive;
  ResistModel resist_model = ResistModel::None;
  ScopePolicy scope_policy = ScopePolicy::SkillOnly;
  bool affects_sword_intent = false;
  bool affects_sword_step = false;
  TriggerContract trigger{};
};

struct SkillContract {
  uint32_t skill_id = 0;
  uint8_t min_nodes = 24;
  uint8_t max_nodes = 26;
  uint8_t max_transmuters = 2;
  uint8_t max_triggers = 1;
  bool has_sword_intent_node = true;
  bool has_synergy_node = true;
  std::array<uint32_t, 2> transmuter_node_ids{};
};

static_assert(sizeof(NodeContractData) <= 40);
static_assert(sizeof(SkillContract) <= 32);
```

## 3. ECS Components / Systems

- Component
  - `SkillContractComponent`（玩家或技能执行者持有已解析合同快照）
  - `SkillNodeRuntimeState`（节点运行时状态，含触发冷却戳）
- System
  - `SkillContractValidationSystem`（加载时验证合同）
  - `SkillContractRuntimeSystem`（运行时查询范围声明、触发冷却、互斥关系）
- Singleton
  - `SkillContractRegistry`（由 `assets/data/skills.json`+合同块初始化）

## 4. Persistence

在存档中追加“专精合同版本”和必要运行时状态，不保存可从配置重建的数据。

```json
{
  "skill_contract_runtime": {
    "version": 1,
    "skills": [
      {
        "skill_id": 2,
        "active_transmuter_node": 271,
        "trigger_cooldowns": [
          { "node_id": 233, "remaining": 1.4 }
        ]
      }
    ]
  }
}
```

## 5. VFX/UI 挂接点

- UI
  - `UISkillTalentTree` 读取 `SpecNodeRole` 显示 Keystone/Trigger/Synergy/Transmuter 徽标
  - Tooltip 强制显示范围声明（SkillOnly / Global）
- VFX
  - 节点激活事件通过 `SkillContractRuntimeSystem` 广播到 `SwordIntentVisualSystem` 与 `GPUSkillEffectSystem`
  - 不新增全局 SSBO binding，沿用 `SSBO_SKILL_EFFECTS = 6`

## 6. 引擎约束

- 不允许在非 `CompositePass` 写入 FBO 0
- SSBO 0-15 已占满，新增特效参数使用：
  - 现有 `GPUSkillEffect` 字段复用，或
  - Compute shader 本地 binding（仅 pass 内）
- 帧序保持：`Input -> Player Movement -> AI -> Combat -> Spatial Grid Rebuild -> Physics -> Render`

## 7. 验收标准

- [ ] 9 个剑修技能全部具备合同定义并通过加载校验
- [ ] 每技能合同满足：`24-26` 节点、`<=2` Transmuter、`<=1` Trigger、`>=1` Synergy
- [ ] 范围声明缺失率为 0（减抗/穿透/暴露/上限压制节点）
- [ ] 存档读写后合同运行态恢复正确
- [ ] `build.bat` 编译通过

