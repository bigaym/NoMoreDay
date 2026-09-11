# 模块化技能交付原型库与专精系统深化重构设计 (Modular Skill Delivery Archetypes & Specialization Design)

- 日期：2026-09-06
- 状态：方案设计审阅 (Under Review)
- 对应前置设计：[`docs/designs/2026-09-06-modern-skill-system-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modern-skill-system-design.md)
- 前置流程文档：`docs/workflows/design.md`
- 代码规范与硬规则约束：`conductor/code_standard.md` (V2.1)
- 标杆对标：涵盖主流 ARPG（《Path of Exile 流放之路》、《Last Epoch 最后纪元》、《Grim Dawn 恐怖黎明》、《Diablo 4 暗黑破坏神4》）的底层技能交付与专精组合范式

---

## 1. 需求背景与问题陈述 (Problem Statement)

在之前的现代化技能系统重构中，引擎确立了动态标签推导、装备技能修饰器持久化分段、纯 POD 领域系统与两阶段安全派发的统一触发总线（`ProcEngine`）。然而，具体业务层面的 **9 个核心基础技能（ID 1~9）及其专精树实现** 依然处于高度碎片化、代码冗余、重复造轮子的过渡状态：

### 1.1 现状核心痛点与代码实锤

1. **专精查询三权分立，热路径开销严重**：
   - 在 [`FlowingThrust.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/FlowingThrust.cpp) 与 [`BladeFormation.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeFormation.cpp) 中，部分节点使用 `exec.active_nodes.test(NodeId % 100)` 查询，另一部分节点却在 `DoCast` 和 `DoHit` 中遍历 `ActiveSkillsComponent::specialized_slots` 进行 `allocated_points.find()`；
   - 在 [`BladeBoomerang.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeBoomerang.cpp) 与 [`PhantomTrance.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/PhantomTrance.cpp) 中，完全无视 `active_nodes`，每次施法都在热路径上执行多次 map 查找与线性扫描；
   - 技能 1 中仍残留大量被注释的遗留代码（如 `// ID 121: Jian Yi Ying Ying (Chance to gain Intent) - Legacy`）。
2. **`BakedSkillProfile` 存在严重烘焙断层**：
   - 核心系统 [`SkillSystem.cpp:2203-2287`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2203-L2287) 中的 `RebakeSkillProfiles` 目前**仅处理了装备修饰器（Item Skill Modifiers），完全遗漏了技能专精树（Talent Nodes）**；
   - 导致本应在洗点/换装时一次性烘焙的参数（冷却缩减、耗蓝减免、投射物增加、范围缩放、伤害倍率加成、异常状态注入），被迫在每个技能的 `.cpp` 中逐个手写 `if-else` 计算，严重违反 DRY 与开闭原则（OCP）。
3. **交付管线尚未彻底通用化，特例残留**：
   - 技能 6（剑阵）属于标准的地表领域，却自立门户维护 [`SwordArrayComponent`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SwordArray.cpp) 与私有更新循环，未接入通用 [`AreaFieldDeliverySystem`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/AreaFieldDeliverySystem.cpp)；
   - 通用引导管线 [`BeamChannelDeliverySystem.cpp:71, 89`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp#L71) 内部充满了 `if (chan.skill_id == 5) ... if (chan.skill_id == 7) ...` 的硬编码特例分支。
4. **跨技能强耦合穿透**：
   - 技能 1 [`FlowingThrust.cpp:538`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/FlowingThrust.cpp#L538) 直接硬编码调用 `RefundRendingWaveCooldown` 修改技能 2 的状态；技能 4 和技能 9 手写私有反击窗口，未统一归入 `ProcEngine`。

---

## 2. 架构设计哲学：面向数据的模块化拼装 (DOD Modular Composition)

### 2.1 传统面向对象（OOP）组件化的陷阱 vs DOD 组件化
用户提出的“做成模块化的组合”是现代技能系统设计的终极方向。但在高性能 C++ / ECS 引擎中，必须避开经典的 OOP 陷阱：
- **OOP 陷阱（严禁使用）**：定义 `ISkillModifier`、`IDeliveryBehavior` 虚基类接口，在运行时使用 `std::vector<std::unique_ptr<IModifier>>` 动态遍历。这会引发严重的虚函数寻址开销、破坏 CPU 缓存局部性（Cache Miss），并产生每帧数千次的堆分配，直接违背 `conductor/code_standard.md` §5 的性能红线。
- **DOD / ECS 最佳实践（本设计采纳）**：
  1. **静态全量烘焙 (AOT Baking on Change)**：在玩家调整天赋或更换装备时，专精烘焙器将 80% 的数值增减、属性转化和规则判定一次性写入纯 POD 的 `BakedSkillProfile`；
  2. **正交交付原型库 (Universal Delivery Archetypes)**：将技能运动与空间表现抽象为参数化的纯 POD 组件，由无状态的通用 System 批量迭代（Cache Friendly）；
  3. **标准伤害与状态载荷 (POD Payloads)**：交付碰撞后仅生成统一的 `DamageRequest` 与 `AilmentApplyRequest`；
  4. **事件驱动联动 (Unified Proc Bus)**：所有跨技能联动与反制派发统一由 `TriggerRuleComponent` 与 `ProcEngine` 闭环。

---

## 3. 十二大通用 ARPG 交付原型库 (Twelve Universal Delivery Archetypes)

通过对《Path of Exile》、《Last Epoch》、《Grim Dawn》、《Diablo 4》全品类技能形态的收敛提炼，将 NoMoreDay 技能交付载体扩展为 **12 种通用的标准交付原型**。所有组件均保证 **Standard Layout** 与 **零堆分配**：

```
                                  【技能激活 TryCast】
                                           │
       ┌───────────────────────────────────┼───────────────────────────────────┐
       ▼                                   ▼                                   ▼
┌───────────────────────┐       ┌───────────────────────┐       ┌───────────────────────┐
│  空间投射与轨迹类      │       │  地面、光束与持续类   │       │  机动、随从与反制类   │
├───────────────────────┤       ├───────────────────────┤       ├───────────────────────┤
│ 1. DirectStrike       │       │ 5. AreaField          │       │ 8. Mobility           │
│ 2. BallisticProj      │       │ 6. SkyfallImpact      │       │ 9. OrbitingSentinel   │
│ 3. BoomerangProj      │       │ 7. BeamChannel        │       │ 10. PhantasmClone     │
│ 4. ChainBranching     │       │                       │       │ 11. ReactiveWard      │
│                       │       │                       │       │ 12. StickyDetonation  │
└───────────────────────┘       └───────────────────────┘       └───────────────────────┘
```

### 3.1 详细原型定义与规格

#### 原型 1：近战定向挥砍与震击 (DirectStrikeDelivery)
- **应用形态**：扇形横扫（Cleave）、矩形突刺（Box Thrust）、周身环形震击（PBAoE 旋风斩）。
- **POD 数据结构**：
  ```cpp
  enum class StrikeShape : uint8_t { Sector = 0, Box = 1, Radial = 2 };
  struct DirectStrikeComponent {
      entt::entity owner = entt::null;
      uint64_t cast_id = 0;
      uint32_t skill_id = 0;
      StrikeShape shape = StrikeShape::Sector;
      float radius = 40.0f;
      float sector_angle = 120.0f; // 仅 Sector 模式有效
      Vector2 box_extents{20.0f, 60.0f}; // 仅 Box 模式有效
      float lifetime = 0.1f; // 命中检测窗口
      float timer = 0.0f;
      bool hit_once = true; // 是否单次打击
      CompactEntitySet<8> hit_entities{}; // SBO 防止重复打击
  };
  static_assert(std::is_standard_layout_v<DirectStrikeComponent>);
  ```

#### 原型 2：直线与弹道投射物 (BallisticProjectileDelivery)
- **应用形态**：单发飞弹、扇形多重投射、平行弹幕、穿透弹、碰撞/最大距离末端分裂。
- **POD 数据结构**：
  复用并硬化既有 `Projectile` 组件，确保具备：
  - `speed`, `lifetime`, `radius`, `pierce_count`, `max_pierce` (SBO 缓存);
  - 分裂参数：`split_count` (0~8), `split_angle_spread`, `split_skill_id`.

#### 原型 3：折返与悬停回旋物 (BoomerangProjectileDelivery)
- **应用形态**：飞锤回旋、回旋飞剑、游侠回旋之刃。出射 -> 远端停滞切割 -> 折返玩家 -> 接取回调。
- **POD 数据结构**：
  ```cpp
  enum class BoomerangPhase : uint8_t { Outward = 0, HoverApex = 1, Returning = 2 };
  struct BoomerangComponent {
      entt::entity owner = entt::null;
      uint64_t cast_id = 0;
      uint32_t skill_id = 0;
      BoomerangPhase phase = BoomerangPhase::Outward;
      float max_distance = 300.0f;
      float hover_duration = 0.3f;
      float hover_timer = 0.0f;
      float return_speed_accel = 800.0f;
      float pull_radius = 0.0f;    // 顶点黑洞牵引半径
      float pull_strength = 0.0f;  // 顶点牵引力度
      bool catch_by_owner = true;  // 接剑回调触发
      Vector2 apex_position{0.0f, 0.0f};
  };
  static_assert(std::is_standard_layout_v<BoomerangComponent>);
  ```

#### 原型 4：连锁与传导跳跃 (ChainBranchingDelivery)
- **应用形态**：连锁闪电、弧光（Arc）、感电在目标间的弹射与分叉。
- **POD 数据结构**：
  ```cpp
  struct ChainBranchComponent {
      entt::entity owner = entt::null;
      uint64_t cast_id = 0;
      uint32_t skill_id = 0;
      uint8_t remaining_chains = 3;
      float search_radius = 200.0f;
      float damage_falloff = 0.1f; // 每次跳跃伤害衰减比例
      entt::entity current_target = entt::null;
      CompactEntitySet<8> visited_targets{}; // 防止在相同目标间死循环
  };
  static_assert(std::is_standard_layout_v<ChainBranchComponent>);
  ```

#### 原型 5：地表领域与脉冲结界 (AreaFieldDelivery - 已有并完善)
- **应用形态**：诛仙剑阵、天剑领域、血海、烈焰余烬地表、剧毒泥潭。
- **POD 数据结构**：
  复用标准 `AreaFieldComponent`，包含 `remaining_duration`, `pulse_interval`, `radius`, `shape_type`, `injected_payloads`。**本计划范围内仅废弃私有的 `SwordArrayComponent`（技能 6）**；`HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent` 属技能 11/12（天剑降临/血海），**超出本计划 9 技能范围，另立计划迁移**——其依赖面保持不变：`HeavenlySwordDescent.cpp:184-400`、`BladeMasteryService.cpp:51-88`、`SkillSystem.cpp:1080-1092`，且 `AreaFieldDeliverySystem.cpp:25` 的双组件共存 skip 模式保持原样，避免通用管线误伤特例领域并保证"零特例"DoD 范围可控。

#### 原型 6：天降打击与流星轰击 (SkyfallImpactDelivery)
- **应用形态**：天剑降临巨型落剑、陨石暴雨（Meteor Rain）、暴风雪碎冰打击。
- **POD 数据结构**：
  ```cpp
  struct SkyfallImpactComponent {
      entt::entity owner = entt::null;
      uint64_t cast_id = 0;
      uint32_t skill_id = 0;
      Vector2 target_center{0.0f, 0.0f};
      float delay_before_impact = 0.4f; // 落地前摇指示
      float impact_radius = 80.0f;
      uint8_t wave_count = 1;          // 级联落点波数
      float wave_interval = 0.1f;
      float spread_radius = 0.0f;      // 多波次散布半宽
      uint8_t leave_field_skill_id = 0;// 落地后是否衍生留在地面的 AreaField
  };
  static_assert(std::is_standard_layout_v<SkyfallImpactComponent>);
  ```

#### 原型 7：持续引导光束与弹幕流 (BeamChannelDelivery - 参数化解耦)
- **应用形态**：射线切割（Disintegrate/MindBlade）、连珠弹幕喷射（Barrage/InfiniteBlades）。
- **POD 数据结构**：
  ```cpp
  enum class BeamChannelMode : uint8_t { ContinuousLaser = 0, BarrageEmitter = 1 };
  struct BeamChannelComponent {
      entt::entity owner = entt::null;
      uint64_t cast_id = 0;
      uint32_t skill_id = 0;
      BeamChannelMode mode = BeamChannelMode::ContinuousLaser;
      float max_channel_time = 5.0f;
      float current_channel_time = 0.0f;
      float tick_interval = 0.2f;
      float tick_timer = 0.0f;
      float turn_rate = 360.0f; // 转向角速度限制
      bool aim_assist = false;  // 索敌吸附锁定
      uint32_t finisher_trigger_skill_id = 0; // 引导结束衍生技 (如天剑降世)
  };
  static_assert(std::is_standard_layout_v<BeamChannelComponent>);
  ```

#### 原型 8：位移冲刺与机动闪避 (MobilityDelivery)
- **应用形态**：流云刺突进、绝影后撤步、法师闪现传送、旋风斩移动。
- **POD 数据结构**：
  ```cpp
  enum class MobilityType : uint8_t { Dash = 0, Blink = 1, Leap = 2 };
  struct MobilityComponent {
      entt::entity target_entity = entt::null;
      MobilityType type = MobilityType::Dash;
      Vector2 direction{0.0f, 0.0f};
      float speed = 500.0f;
      float duration = 0.3f;
      float timer = 0.0f;
      bool has_invulnerability = false; // 无敌帧/免控
      bool leaves_motion_trail = true;
      uint32_t spawn_clone_node = 0;    // 途中是否释放残影
  };
  static_assert(std::is_standard_layout_v<MobilityComponent>);
  ```

#### 原型 9：环绕守护与浮游伴身物 (OrbitingSentinelDelivery)
- **应用形态**：御剑术环绕灵剑、护体剑罡旋转剑刃、暗黑旋风护甲、祝福之锤。
- **POD 数据结构**：
  ```cpp
  struct OrbitingSentinelComponent {
      entt::entity anchor_entity = entt::null;
      uint32_t skill_id = 0;
      uint8_t count = 3;
      float orbit_radius = 60.0f;
      float current_angle = 0.0f;
      float angular_velocity = 180.0f; // 度/秒
      float interception_chance = 0.0f;// 拦截弹道概率
      float attack_scan_radius = 0.0f; // 离体索敌射击半径 (0 则保持常驻环绕)
      float attack_interval = 1.0f;
      float attack_timer = 0.0f;
  };
  static_assert(std::is_standard_layout_v<OrbitingSentinelComponent>);
  ```

#### 原型 10：伴随残影与幻象动作镜像 (PhantasmCloneDelivery)
- **应用形态**：流云刺留影、绝影绝剑双生影分身、七星斩剑影。
- **POD 数据结构**：
  ```cpp
  struct PhantasmCloneComponent {
      entt::entity creator = entt::null;
      uint32_t skill_id = 0;
      float delay_before_cast = 0.2f;
      float lifetime = 1.5f;
      float damage_scale = 0.5f;
      SkillSnapshot snapshot{}; // 捕获施法时的属性快照 (DamagePayloadContext)
  };
  static_assert(std::is_standard_layout_v<PhantasmCloneComponent>);
  ```

#### 原型 11：响应式护盾与反制屏障 (ReactiveWardDelivery)
- **应用形态**：护体剑罡阻挡反击、绝影绝剑免死形态窗口（由 `PhantomTrance` 组件承载）、荆棘反震。
- **POD 数据结构**：
  ```cpp
  struct ReactiveWardComponent {
      entt::entity owner = entt::null;
      float ward_duration = 3.0f;
      float counter_window = 0.5f; // 精确弹反有效窗口 (秒)
      float timer = 0.0f;
      float damage_absorb_pool = 0.0f;
      uint32_t counter_skill_id = 0; // 反制成功时触发的技能 ID
      bool triggered = false;
  };
  static_assert(std::is_standard_layout_v<ReactiveWardComponent>);
  ```

#### 原型 12：附着印记与延迟殉爆 (StickyDetonationDelivery)
- **应用形态**：裂空斩剑气烙印引爆、爆裂掌、尸爆、末日烙印。
- **POD 数据结构**：
  ```cpp
  struct StickyDetonationComponent {
      entt::entity attacker = entt::null;
      uint32_t source_skill_id = 0;
      uint8_t current_stacks = 1;
      uint8_t max_stacks = 5;
      float timer = 4.0f;
      float explode_radius = 80.0f;
      bool explode_on_death = true;
      bool explode_on_max_stacks = false;
  };
  static_assert(std::is_standard_layout_v<StickyDetonationComponent>);
  ```

---

## 4. 全量专精静态烘焙引擎 (Specialization Baking Pipeline)

为消除 9 个技能内部混乱的 `allocated_points.find()` 与 `active_nodes.test()`，必须在 [`SkillSystem::RebakeSkillProfiles`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2203) 中建立**全量专精数据烘焙流水线**。

### 4.1 烘焙器输入与输出流

```
 ┌───────────────────────────┐      ┌───────────────────────────┐
 │   ActiveSkillsComponent   │      │     EquipmentComponent    │
 │ (specialized_slots & pts) │      │  (item.skill_modifiers)   │
 └─────────────┬─────────────┘      └─────────────┬─────────────┘
               │                                  │
               └────────────────┬─────────────────┘
                                ▼
         ┌──────────────────────────────────────────────┐
         │     SkillSpecializationBaker::Bake()         │
         │                                              │
         │  1. 基础标签与变质树 (Transmuter Resolution)   │
         │  2. 数值累加 (Stats Accumulator: CD, Mana..) │
         │  3. 异常载荷注入 (Payload Injections)        │
         │  4. 交付原型形态改写 (Delivery Morph Rules)  │
         │  5. 触发规则注册 (Proc Rules -> TriggerRule) │
         └──────────────────────┬───────────────────────┘
                                │
               ┌────────────────┴────────────────┐
               ▼                                 ▼
┌─────────────────────────────┐   ┌─────────────────────────────┐
│      BakedSkillProfile      │   │     TriggerRuleComponent    │
│  (Cached on ActiveSkills)   │   │  (Two-phase Event Dispatch) │
└─────────────────────────────┘   └─────────────────────────────┘
```

### 4.2 扩展后的纯 POD `BakedSkillProfile`

```cpp
struct BakedDeliveryParams {
    uint8_t primary_archetype = 0; // 对应 12 大 Delivery 原型枚举
    uint8_t secondary_archetype = 0; // 组合第二原型 (例如 Mobility + DirectStrike)
    uint32_t feature_flags = 0;    // 位掩码: 如 IsBoomerang, HasApexVortex, HasSplit
    
    // 通用参数包 (无需堆分配的定长紧凑结构)
    float speed = 300.0f;
    float range = 200.0f;
    float duration = 1.0f;
    uint8_t sub_count = 0;         // 分裂数 / 连跳数
    float sub_interval = 0.0f;     // 脉冲间隔 / 弹幕发射间隔

    // 注意 (P1-2 单一事实源): 此处不再定义 count / width_or_radius —— 数量与尺寸的
    // 唯一事实源为 BakedSkillProfile 顶层既有字段 projectile_count (int, 默认 1) 与
    // area_radius (float, 默认 1.0f) (SkillDefs.hpp:527-544)，由 Baker 计算写入，
    // 交付系统直接读取，避免新旧字段双源漂移；delivery 仅存交付模式与行为标志。
};
static_assert(std::is_standard_layout_v<BakedDeliveryParams>);

struct BakedSkillProfile {
    uint32_t skill_id = 0;
    int effective_level = 1;
    float effective_cooldown = 0.0f;
    float effective_mana_cost = 0.0f;
    Tag effective_tags = Tag::None;
    float proc_coefficient = 1.0f;
    float more_damage_mult = 1.0f;
    
    // 烘焙后的交付参数
    BakedDeliveryParams delivery{};

    // 定长注入载荷 (Ailment, Impulses)
    static constexpr uint8_t kMaxInjectedPayloads = 4;
    std::array<PayloadDefinition, kMaxInjectedPayloads> injected_payloads{};
    uint8_t injected_count = 0;
};
static_assert(std::is_standard_layout_v<BakedSkillProfile>);
```

---

## 5. 现存 9 个核心技能重构与模块映射方案

有了 12 大通用交付原型与全量专精烘焙器后，原本每个技能几百行的混乱实现将被极大精简。每个技能的 `DoCast` / `DoHit` 仅需 **50 行以内的参数装配代码**：

```
                    ┌──────────────────────────────────────────────────┐
                    │               9 个技能模块装配映射表              │
                    ├────┬────────────────────┬────────────────────────┤
                    │ ID │ 技能名称           │ 组装的交付原型 (Archetypes)│
                    ├────┼────────────────────┼────────────────────────┤
                    │ 1  │ 流云刺             │ Mobility + DirectStrike│
                    │ 2  │ 裂空斩             │ Ballistic + Boomerang  │
                    │ 3  │ 御剑术             │ OrbitingSentinel       │
                    │ 4  │ 剑气护体           │ Orbiting + ReactiveWard│
                    │ 5  │ 万剑归宗           │ BeamChannel (Barrage)  │
                    │ 6  │ 剑阵·诛仙          │ AreaField              │
                    │ 7  │ 心剑·无影          │ BeamChannel (Laser)    │
                    │ 8  │ 御剑·回旋          │ BoomerangProjectile    │
                    │ 9  │ 绝影绝剑           │ Mobility (绝影形态)    │
                    └────┴────────────────────┴────────────────────────┘
```

### 5.1 技能 1：流云刺 (FlowingThrust)
- **原型组合**：`MobilityDelivery` (突进) + `DirectStrikeDelivery` (贯通打击盒) + `PhantasmCloneDelivery` (留影残影)。
- **专精映射解耦**：
  - 节点 100/101/103/110（攻速/耗蓝/增伤/减CD）：全进 `BakedSkillProfile`；
  - 节点 114（剑气回响）：烘焙为 `TriggerRule`（OnSkillHit 触发技能 2）；
  - 节点 130（留影）：配置 `MobilityComponent::spawn_clone_node = 130`，自动生成 `PhantasmCloneComponent`；
  - 节点 170/172（劫火/凛风变质）：直接由烘焙器推导 `effective_tags`，自动附加点燃/减速载荷；
  - 移除 `RefundRendingWaveCooldown` 硬编码，改由 `ProcEngine` 派发。

### 5.2 技能 2：裂空斩 (RendingWave)
- **原型组合**：`BallisticProjectileDelivery` (剑气弹道) + `BoomerangProjectileDelivery` (回旋变体) + `StickyDetonationDelivery` (烙印)。
- **专精映射解耦**：
  - 节点 210（多重剑气）：烘焙器设置 `projectile_count = 1 + pts`（单一事实源为 BakedSkillProfile 顶层字段，见 4.2），系统自动展开扇形发射；
  - 节点 211/213（分裂/引爆）：烘焙器设置 `split_count = 3` 与 `explode_on_hit = true`；
  - 节点 230/232（回旋劲/引力陷阱）：烘焙器切换原型至 `Boomerang`，并注入 `pull_radius = 120.0f`；
  - 节点 270/272（霜寒/雷光变质）：烘焙器直接转化 `Tag::Physical -> Cold/Lightning`。

### 5.3 技能 3：御剑术 (BladeFormation)
- **原型组合**：`OrbitingSentinelDelivery` (环绕灵剑)。
- **专精映射解耦**：
  - 节点 311（无尽剑匣）：烘焙器设置 `projectile_count = 8`，`more_damage_mult = 0.6f`；
  - 节点 330（巨剑模式）：烘焙器设置 `projectile_count = 1`，`delivery.range` 内半径参数以 `area_radius` 表达并设为 `80.0f`，`more_damage_mult = 3.0f`；
  - 节点 370/371（元素附魔）：烘焙器推导元素标签并改变剑灵渲染色；
  - 彻底废弃旧有的 `BladeFormationComponent`，完全由 `OrbitingSentinelDeliverySystem` 接管。

### 5.4 技能 4：剑气护体 (BladeWard)
- **原型组合**：`OrbitingSentinelDelivery` (护体剑罡) + `ReactiveWardDelivery` (受击拦截反制)。
- **专精映射解耦**：
  - 节点 401（拦截率）：烘焙器累加 `interception_chance`；
  - 节点 451（瞬影反击）：烘焙器注册 `counter_skill_id = 1`（流云刺反击）；
  - 节点 470（反击飞剑）：烘焙器注册 `counter_skill_id = 8`（回旋飞剑反击）；
  - 彻底解耦私有拦截判定，统一走反制屏障与 `ProcEngine`。

### 5.5 技能 5：万剑归宗 (InfiniteBlades)
- **原型组合**：`BeamChannelDelivery` (BarrageEmitter 模式)。
- **专精映射解耦**：
  - 从 `BeamChannelDeliverySystem` 中剔除 `if (skill_id == 5)`；
  - 节点 551（意气爆发）：施法时检查剑意，将发射频率提高 100%；
  - 节点 513（天剑降世终结技）：烘焙器设置 `finisher_trigger_skill_id = 11`（或技能 2）；
  - 节点 570（元素陨落）：烘焙器负责标签推导与视觉染色。

### 5.6 技能 6：剑阵·诛仙 (SwordArray)
- **原型组合**：`AreaFieldDelivery` (标准地表领域)。
- **专精映射解耦**：
  - 彻底删除 `SwordArrayComponent` 和私有更新；
  - 直接装配 `AreaFieldComponent`，设置 `pulse_interval = 0.3s`，`radius = 120.0f`；
  - 节点 630/631/633（缓速/破甲/斩杀）：直接转化为 `AreaFieldComponent::payloads` 中的 `PayloadDefinition`，由通用地表管线统一每帧无差别派发。

### 5.7 技能 7：心剑·无影 (MindBlade)
- **原型组合**：`BeamChannelDelivery` (ContinuousLaser 模式)。
- **专精映射解耦**：
  - 从 `BeamChannelDeliverySystem` 中剔除 `if (skill_id == 7)`；
  - 将手写的粒子逻辑剥离至 `GPUSkillEffectSystem`（通过 `SkillVfxEvent` 驱动）；
  - 节点 730（神识锁定）：烘焙器开启 `aim_assist = true`；
  - 节点 770（射线聚焦）：标签推导至 `Tag::Lightning`。

### 5.8 技能 8：御剑·回旋 (BladeBoomerang)
- **原型组合**：`BoomerangProjectileDelivery` (通用回旋管线)。
- **专精映射解耦**：
  - 节点 812（破空增伤）：烘焙器计算速度转化为 `more_damage_mult`；
  - 节点 830/833（磁力场/黑洞）：开启 `pull_radius` 与 `pull_strength`；
  - 节点 850（滞空切割）：设置 `hover_duration = 1.5f`；
  - 节点 831（接剑）：挂载接取回调，回复剑意与冷却。

### 5.9 技能 9：绝影绝剑 (PhantomTrance)
- **原型组合**：`MobilityDelivery` (3 秒绝影形态；破空一闪时附带瞬移) + `Buff` (免死/逆脉生存态 + 结束附魔窗口)。`ReactiveWardDelivery` 仅保留给技能 4 剑气护体，技能 9 不再使用。
- **专精映射解耦**：
  - 节点 935 `逆命反噬`：逆脉窗口内近战命中 20% 由 `ProcEngine` 触发技能 8 御剑·回旋（0.5s ICD）；
  - 节点 993 `影剑回响`：绝影期间御剑·回旋命中 40% 触发一次 50% 效果的影子回响（行为层手写规则，不占触发器结构位）；
  - 节点 954 `时光逆流`：绝影结束时按剩余冷却返还其他技能 3s；
  - 免死/逆脉：`CombatSystem::ApplyDamage` 致命伤锁 1 血 + `PhantomTranceComponent` 资源循环；附魔窗口由 `SkillModifierComponent.damage_modifiers` 的 GainExtra 承载。

---

## 6. 非目标与系统边界 (Non-Goals & Boundaries)

1. **非目标：不修改任何既有装备二进制存档格式**
   - 继续沿用 Section 13 二进制分段持久化，严禁改动 `ItemInstance` 192 字节 POD 结构；
2. **临时非目标（已到期）：Compact Contract 自动化测试矩阵的阶段性冻结**
   - 该条款仅适用于模块化迁移阶段，不应被解读为「旧临时形态受保护」。技能 8（提交 `9f9ad255`）已确立「按玩法设计重写关键节点契约、同步更新 fixture/helper」的先例；技能 9 及其后续技能的设计性整改同理，允许更新 `SkillKeyNodeMatrixTests`、`SkillBehaviorGuardTests` 与 `ExpectedKeyNodesBySkill` 相关断言与夹具，但禁止通过弱化断言换取通过；
3. **非目标：不引入运行时虚函数接口调用**
   - 坚决杜绝面向对象的 `ISkill` / `IModifier` 运行时多态，严格保证组件是 Standard Layout POD。

---

## 7. 验收标准与验证方案 (Acceptance Criteria & Verification)

### 7.1 静态与编译期断言
- 所有新建 Delivery 组件必须通过 `static_assert(std::is_standard_layout_v<T>)` 与 `static_assert(std::is_trivially_destructible_v<T>)`；
- 技能行为文件行数验收采用**逐文件绝对行数上限**（与实施计划 DoD 统一口径）。原"每文件缩减 60%"比率表述含混（614×40%≈246 行，与"100 行以内"上限互相矛盾），予以废除；如需统计缩减率仅作参考指标，度量口径为文件内手写逻辑行（装配骨架），不作为验收依据。逐文件目标表（当前实测行数依据审查报告核验）：

| 行为文件 | 当前实测行数 | 验收上限 |
|---|---|---|
| `FlowingThrust.cpp` | 614 | ≤100 |
| `RendingWave.cpp` | 558 | ≤100 |
| `SwordArray.cpp` | 436 | ≤100 |
| `MindBlade.cpp` | 405 | ≤100 |
| `BladeFormation.cpp` | 355 | ≤100 |
| `BladeBoomerang.cpp` | 315 | ≤100 |
| `PhantomTrance.cpp` | 809 | ≤100 |
| `BladeWard.cpp` | 219 | ≤100 |
| `InfiniteBlades.cpp` | 159 | ≤100 |
| **合计** | **3870** | **≤900** |

（159 行级的 `InfiniteBlades.cpp` 对 Barrage 装配模式同样适用 ≤100 绝对上限；各文件装配代码仅为其一部分，5 章"50 行以内参数装配代码"的描述与本表不冲突。）

### 7.2 自动化测试矩阵验证
- 执行构建命令：`build.bat`（RelWithDebInfo 模式，零警告/零错误）；
- 执行 CTest 守护测试（ctest 条目名为 `nmd.tests.*` 系列，`-R` 正则只作用于注册条目名，不作用于源码内 TEST_CASE 名）：
  ```powershell
  ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(unit|integration)" --output-on-failure
  ```
- TEST_CASE 级过滤（按需）：直接运行测试二进制并传 doctest `-tc=` 参数（如 `--test-case="*FlowingThrust*"`），详见实施计划 4.2；
- 确保所有 Key Node 测试、互斥变质测试、触发冷却测试全部 PASS。

### 7.3 性能基线验证
- 先以 [`tests/performance/SkillSystemBenchmark.cpp`](file:///d:/PRJ/NoMoreDay/tests/performance/SkillSystemBenchmark.cpp)（注意位于 `tests/performance/`）记录**重构前基线**（实施计划 Task 1.5）；
- 在该基准中压测 10,000 次批量施法，以重构前基线为参照，验证重构后的全量静态烘焙模式比旧的手写 `allocated_points.find()` 至少提升 **2.5x 吞吐量**，且施法热路径**零堆分配 (Zero malloc)**；
- 压测场景必须包含**"洗点+换装"触发的 Rebake 压力路径**（触发链已存在：洗点 `SkillSystem.cpp:1868` → `StatsDirty` → `StatsSystem.cpp:525-531/101` → `AttributePipeline.cpp:788`），不能只测施法热路径。

---

## 8. 实施阶段划分建议 (Next Steps)

1. **Phase 1（核心管线与全量烘焙器建立）**：
   - 实现 `SkillSpecializationBaker`，补齐 `BakedSkillProfile` 天赋烘焙链路；
   - 提炼 12 大通用 Delivery 原型组件定义；
   - 从零新建 `CompactEntitySet`（按原型区分 8/32 槽容量）并记录重构前性能基线（对应计划 Task 1.4/1.5）。
2. **Phase 2（通用 Delivery 基础设施就绪）**：
   - 扩展/重构 `AreaFieldDeliverySystem`、`BeamChannelDeliverySystem`（含 `ChannelingComponent` → `BeamChannelComponent` 全消费方迁移）；
   - 新增 `BoomerangDeliverySystem`、`MobilityDeliverySystem` 与 `OrbitingSentinelDeliverySystem`；
   - 为 DirectStrike / StickyDetonation（→ `ProjectileSystem` 状态机扩展）、SkyfallImpact（→ `AreaFieldDeliverySystem` 扩展）、ReactiveWard（→ `ProcEngine` 回调骨架扩展）、PhantasmClone（→ `MobilityDeliverySystem` 扩展）五个原型落实现有 System 消费方扩展（对应计划 Task 2.6），确保无死数据组件。
3. **Phase 3（9 个技能按切片试点迁移与消减特例）**：
   - 试点先行：技能 5/7（Task 3.3，仅依赖组件迁移）验证迁移模式；
   - 批次 A：技能 6（剑阵）、技能 1（流云刺）、技能 2（裂空斩）；
   - 批次 B：技能 3（御剑术）、技能 4（剑气护体）、技能 8（回旋飞剑）、技能 9（绝影绝剑）。
4. **Phase 4（死代码清理与全量回归验收）**。
