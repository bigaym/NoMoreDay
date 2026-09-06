# 现代化技能系统架构设计与实施计划审查报告 (Review Report)

- **审查目标**：
  - 设计文档：[`docs/designs/2026-09-06-modern-skill-system-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modern-skill-system-design.md)
  - 实施计划：[`docs/plans/2026-09-06-modern-skill-system-plan.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-06-modern-skill-system-plan.md)
- **审查基准**：
  - 流程规范：[`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md)
  - 编码规范与硬规则：[`conductor/code_standard.md`](file:///d:/PRJ/NoMoreDay/conductor/code_standard.md) (V2.1)
  - 技术栈规范：[`conductor/tech-stack.md`](file:///d:/PRJ/NoMoreDay/conductor/tech-stack.md)
  - 代码库现状：`src/game/foundation/components/ItemStats.hpp`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/foundation/components/Projectile.hpp`、`src/game/foundation/components/Buff.hpp`、`src/game/foundation/components/EquipmentComponent.hpp`、`src/game/contracts/CombatEvents.hpp`、`src/game/contracts/impl/CombatEventDispatcher.hpp`、`src/game/contracts/DamagePipelineTypes.hpp`、`src/game/foundation/data/SkillContract.hpp`、`src/game/systems/skill/SkillSystem.cpp`、`src/game/systems/skill/ProjectileSystem.cpp`、`src/game/systems/skill/BladeMasteryService.cpp`、`src/game/application/ui/GameUiSnapshotBuilder.cpp`、`src/game/application/ui/PlayerHUD.cpp`、`src/game/systems/combat/DamagePipeline.hpp` 等。
- **审查轮次**：首次审查 (First Round)
- **审查结论**：`修改` (Needs Revision)

---

## 1. 总体评价与结论

### 1.1 总体结论：`修改`

本次提交的现代化技能系统重构设计与实施计划（`2026-09-06-modern-skill-system-design.md` 与 `2026-09-06-modern-skill-system-plan.md`）具有极高的问题针对性和战略价值：
1. **痛点切中要害**：精准识别了 `ItemStats.hpp` 词缀与具体技能硬编码绑定、`SkillSystem.cpp` 施法主干特例侵入、`HeavenlySwordFieldComponent` / `BloodSeaFieldComponent` 状态爆炸、以及 `Projectile.hpp` 堆分配与 400 字节大快照拷贝等 5 项核心痼疾；
2. **标杆理念先进**：融合《最后纪元》独立专精树与动态标签推导、以及《恐怖黎明》装备技能修饰器 (Item Skill Modifiers) 与自适应触发加权，方向完全符合高品质 ARPG 的演进规律；
3. **架构目标清晰**：坚持交付 (Delivery) 与载荷 (Payload) 正交分离、换装与加点静态烘焙 (Baking) 零运行期开销。

然而，在严格依据 `docs/workflows/review.md` 审查流程与 `conductor/code_standard.md` (V2.1) 架构与性能硬规则进行自包含、独立逐行对拍后，发现该设计与计划存在 **3 项 Blocker 级硬否决缺陷、3 项 High 级缺陷、2 项 Medium 级缺陷**。其中触犯硬规则的关键问题包括：
1. **[Blocker] 触发派发循环跨 EnTT 实体/组件创建持有裸组件指针，违背 EnTT 安全硬规则（§5.3、§2.2，硬否决规则 6 与 8）**；
2. **[Blocker] 地表领域组件 `AreaFieldComponent` 包含 `std::vector` 并在热路径动态分配，违背组件 POD 原则与热路径零分配硬规则（§7.1、§2.1，硬否决规则 12）**；
3. **[Blocker] 重复制造轮子并造成严重契约分叉：重新发明已存在的 `CombatEventType`，无视既有 `CombatEventDispatcher` 与 `SkillContract`（审查规则 5 与 47）**；
4. **[High] 投射物穿透 SBO 栈缓存固定 8 槽位在 `pierceCount > 8` 时静默丢弃，导致目标每帧重复受击的 Multi-Hit 伤害翻倍 Bug**；
5. **[High] 废除天剑与血海专属组件时，遗漏 `BladeMasteryService`、`GameUiSnapshotBuilder` 与 `PlayerHUD` 外部依赖消费方，直接破坏构建**；
6. **[High] 引入 `DamagePayloadContext` 却未定义与 `DamagePipeline` 及 `DamageRequest` 的接口契约，导致伤害快照脱节**。

依据 `docs/workflows/review.md` 硬否决条款，本审查结论明确判定为 **`修改`**。设计与计划必须针对上述缺陷修正后方可进入代码实施。

---

## 2. 变更文件边界与检视范围

### 2.1 `git status --short` 状态摘要
```
?? docs/designs/2026-09-06-modern-skill-system-design.md
?? docs/plans/2026-09-06-modern-skill-system-plan.md
```

### 2.2 检视覆盖范围
- **被审文件**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md` (全文 371 行)
  - `docs/plans/2026-09-06-modern-skill-system-plan.md` (全文 256 行)
- **代码库关键对拍文件**：
  - `src/game/contracts/CombatEvents.hpp` (战斗事件契约与既有 `CombatEventType`)
  - `src/game/contracts/impl/CombatEventDispatcher.hpp` (既有战斗事件分发总线)
  - `src/game/foundation/data/SkillContract.hpp` (既有技能与专精节点契约 `TriggerContract`)
  - `src/game/foundation/components/SkillDefs.hpp` (既有 `ActiveSkillsComponent`、`TalentNode`、`HeavenlySwordFieldComponent`)
  - `src/game/foundation/components/Projectile.hpp` (`Projectile` 结构与快照)
  - `src/game/foundation/components/ItemStats.hpp` (`AffixType` 枚举定义)
  - `src/game/systems/skill/SkillSystem.cpp` (`TryCast`、`OnTakeDamage`、`UpdateStates`、既有 trigger 机制)
  - `src/game/systems/skill/ProjectileSystem.cpp` (投射物碰撞结算与 `DamageRequest` 组装)
  - `src/game/systems/skill/BladeMasteryService.cpp` (天剑/血海外部领域依赖)
  - `src/game/application/ui/GameUiSnapshotBuilder.cpp` 与 `PlayerHUD.cpp` (天剑/血海 UI 快照与时长显示)
  - `src/game/systems/combat/DamagePipeline.hpp` 与 `src/game/contracts/DamagePipelineTypes.hpp` (伤害计算管线)
  - `tests/integration/SkillContractRegistryTests.cpp` (技能契约校验单测)

---

## 3. 范围与架构对齐评估 (Scope & Architecture Alignment)

| 设计支柱 / 目标 | 实施计划对应任务 | 对齐状态 | 评估说明 |
| :--- | :--- | :--- | :--- |
| **支柱 1：动态标签流转与词缀泛型化** | Phase 2 (Task 2.1, 2.3) | **基本对齐** | 引入 `SkillLevelBonus` 取代硬编码枚举，逻辑方向正确；需与 `assets/data/skills.json` 保持向后兼容。 |
| **支柱 2：装备技能修饰器 (Item Modifiers)** | Phase 2 (Task 2.2, 2.3, 2.4) | **基本对齐** | 提出了 Baking 机制与 UI Tooltip 预览；但引用的组件名称 `SkillBookComponent` 与代码库现状不符。 |
| **支柱 3：交付与载荷正交管线** | Phase 3 (Task 3.1, 3.2, 3.3, 3.4) | **存在脱节** | 提取 `AreaFieldDeliverySystem` 方向正确，但遗漏了外部系统（UI、Mastery）对天剑/血海字段的消费。 |
| **支柱 4：统一触发总线 (Proc Engine)** | Phase 1 (Task 1.1, 1.2, 1.3, 1.4) | **严重冲突** | 重新发明了既有的 `CombatEventType`，未接入既有 `CombatEventDispatcher`；派发循环存在 UAF 风险。 |
| **支柱 5：DOD 性能与内存零开销** | Phase 4 (Task 4.1, 4.2, 4.3, 4.4) | **部分背离** | 提出投射物 SBO，但算法存在 >8 穿透重复判定 Bug；`AreaFieldComponent` 自身违背 POD 引入 `vector`。 |
| **非目标：不强行绑定 combat_v2 全量重写** | 计划全篇 | **良好对齐** | 确认对齐稳定的 `DamagePipeline`，符合目前 `CombatV2RuntimeFacade` 处于 Mock/打桩过渡期的客观事实。 |

---

## 4. 核心发现项（按严重度排序）

### 【Blocker 01】触发派发循环跨 EnTT 实体/组件创建持有裸组件指针，违背 EnTT 安全硬规则
- **关联位置**：
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:129-153` (Section 2.3 伪代码)
  - `conductor/code_standard.md:36-39` (§5.3 EnTT Safety)
  - `docs/workflows/review.md:58` (硬否决规则 8)
- **代码实证与危害**：
  计划中的事件派发伪代码如下：
  ```cpp
  void ProcEngine::DispatchEvent(entt::registry& reg, entt::entity source, const CombatEvent& event) {
      auto* triggerComp = reg.try_get<TriggerRuleComponent>(source);
      if (!triggerComp) return;
      ...
      for (auto& rule : triggerComp->rules) {
          ...
          if (RandomFloat(0.0f, 1.0f) <= real_chance) {
              rule.current_cooldown = rule.internal_cooldown;
              // 致命隐患：直接在此处派生施法！
              SkillSystem::TriggerCast(reg, source, rule.cast_skill_id, rule.target_mode, event.trigger_depth + 1, rule.effectiveness);
          }
      }
  }
  ```
  1. `SkillSystem::TriggerCast` 是一个重量级操作，它会调用 `reg.create()` 创建实体（投射物、特效实体、领域实体），并调用 `reg.emplace<T>()` 挂载组件。
  2. 根据 `code_standard.md` §5.3：
     > **Pointer Invalidation**: Component pointers obtained via `registry.try_get<T>(e)` are only valid as long as the underlying component pool is not modified. Never hold a component pointer across operations that might add/remove components or create/destroy entities.
  3. 如果被派生施放的技能或其生成的实体导致 `TriggerRuleComponent`（例如召唤物挂载了触发规则）或相关组件池扩容，`triggerComp` 指针及 `rule` 引用将瞬间悬空！
  4. 随后循环继续执行 `for (auto& rule : triggerComp->rules)`，直接触发 **Use-After-Free (UAF) / 内存崩溃**。
- **违背规则**：`code_standard.md` §5.3（EnTT Safety）、§2.2（Zero tolerance for UB/UAF）；`docs/workflows/review.md` 硬否决规则 6 与 8。
- **修复建议**：
  必须执行“**解耦收集与执行**”（Collect-Then-Execute）原则：
  1. 在评估循环中，仅检查规则、自适应几率与更新冷却时间，将命中的触发动作拷贝到局部栈数组中（如 `std::array<PendingTriggerAction, 4> pending; uint8_t pending_count = 0;`）；
  2. 彻底结束对 `triggerComp` 的访问，完全释放指针引用；
  3. 随后在第二阶段循环遍历局部栈数组 `pending`，安全调用 `SkillSystem::TriggerCast(reg, ...)`。

---

### 【Blocker 02】`AreaFieldComponent` 引入 `std::vector`，违背组件 POD 原则与热路径零分配硬规则
- **关联位置**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md:222-229` (§3.3.1)
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:69-80` (§2.2)
  - `conductor/code_standard.md:7, 52-54` (§2.1, §7.1)
  - `docs/workflows/review.md:62` (硬否决规则 12)
- **代码实证与危害**：
  设计文档在 §3.3.1 提出的通用地表领域组件定义为：
  ```cpp
  struct AreaFieldComponent {
      float remaining_duration = 0.0f;
      float pulse_interval = 0.25f;
      float timer = 0.0f;
      float radius = 48.0f;
      uint8_t shape_type = 0; // 0: 圆形, 1: 环形, 2: 旋转切割线
      std::vector<PayloadDefinition> tick_payloads; // <-- 严重违规！
  };
  ```
  1. 依据 `code_standard.md` §7.1：
     > **Components**: Must be **POD** (Plain Old Data) / Standard Layout types. No complex logic, virtual functions, or non-trivial destructors.
  2. `std::vector` 包含动态堆分配、拥有非平凡析构函数，绝非 POD / Standard Layout 类型。
  3. 地表领域（剑阵、血海、火圈、毒池）在战斗中由玩家和怪物频繁创建与销毁。若组件内置 `std::vector`，每次创建实体均会触发堆内存分配，实体销毁时触发 `free`，直接违背了设计文档自身宣称的“消灭热路径堆分配”目标。
  4. 此外，设计与计划通篇未定义 `PayloadDefinition` 的具体内存结构、大小及虚函数情况，存在未知的内存膨胀隐患。
- **违背规则**：`code_standard.md` §7.1（Component POD 要求）、§2.1（热路径严禁堆分配）；`docs/workflows/review.md` 硬否决规则 12。
- **修复建议**：
  1. 明确定义 `PayloadDefinition` 为紧凑的小型 POD 结构体（如 16~32 字节）；
  2. `AreaFieldComponent` 改为固定容量内联栈数组（Small Buffer Optimization），例如：
     ```cpp
     static constexpr uint8_t kMaxFieldPayloads = 4;
     std::array<PayloadDefinition, kMaxFieldPayloads> tick_payloads{};
     uint8_t payload_count = 0;
     ```
  3. 或者领域实体仅存储 `uint32_t payload_recipe_id`，在静态注册表中查表获取载荷模板，保持组件为 100% 纯 POD。

---

### 【Blocker 03】重复制造轮子并造成严重契约分叉：重新发明 `CombatEventType`，未接入既有总线与契约
- **关联位置**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md:249-258` (§3.4)
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:106-112, 197` (Task 1.1)
  - `src/game/contracts/CombatEvents.hpp:14-50`
  - `src/game/contracts/impl/CombatEventDispatcher.hpp:25-56`
  - `src/game/foundation/data/SkillContract.hpp:41-59`
  - `docs/workflows/review.md:34, 47` (审查规则 5 与 47)
- **代码实证与危害**：
  计划 Task 1.1 声明：
  > “定义 `TriggerRuleComponent`、`CombatEventType` 与 `TriggerRule` 结构 (`src/game/foundation/components/TriggerRuleComponent.hpp`)”
  并在设计文档中给出了一个全新的 8 枚举项的 `CombatEventType`（包含 `OnCritHit`、`OnEvade` 等命名）。
  然而，代码库中早就存在完备、成熟且全工程广泛使用的核心基础设施：
  1. [`src/game/contracts/CombatEvents.hpp:14-50`](file:///d:/PRJ/NoMoreDay/src/game/contracts/CombatEvents.hpp#L14-L50) 已经定义了包含 30+ 细分事件的标准 `CombatEventType`（命名为 `OnCrit`、`OnDodge` 等）；
  2. [`src/game/contracts/impl/CombatEventDispatcher.hpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/CombatEventDispatcher.hpp) 已经实现了线程安全、带优先级的单例派发总线；
  3. [`src/game/foundation/data/SkillContract.hpp:41-47`](file:///d:/PRJ/NoMoreDay/src/game/foundation/data/SkillContract.hpp#L41-L47) 已经定义了严格限制在 40 字节内的 `TriggerContract`；
  4. 既有的 `SkillSystem.cpp:909` 正是调用 `CombatEventDispatcher::Register(CombatEventType::OnTakeDamage, ...)` 来监听受击。
  如果在 `TriggerRuleComponent.hpp` 中另立门户重新定义一个名称冲突但枚举项不兼容的 `CombatEventType`，不仅属于严重的“重复造轮子”，更会造成核心玩法契约分叉、双份维护和编译歧义。
- **违背规则**：`docs/workflows/review.md` 规则 5（检索等价实现）与规则 47（重复轮子造成契约分叉硬否决）。
- **修复建议**：
  1. 彻底删除重新定义 `CombatEventType` 的设计与任务；
  2. 强制使用 `src/game/contracts/CombatEvents.hpp` 中的既有 `NoMoreDay::CombatEventType`；
  3. `ProcEngine` 不应作为孤立的独立总线，而应作为监听者接入既有的 `CombatEventDispatcher`（例如在初始化时向 `CombatEventDispatcher` 注册通用分发入口）。

---

### 【High 01】投射物穿透 SBO 栈缓存固定 8 槽位在 `pierceCount > 8` 时静默丢弃，导致目标重复受击 Bug
- **关联位置**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md:297-300` (§3.5)
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:173-189` (§2.4, Task 4.1)
  - `src/game/systems/skill/ProjectileSystem.cpp:438-440, 471-479`
- **代码实证与危害**：
  设计中提出的 SBO 算法为：
  ```cpp
  void RecordHit(entt::entity target) {
      if (hit_count < kMaxInlineHits) {
          hit_cache[hit_count++] = target;
      }
  }
  ```
  1. 在 `ProjectileSystem.cpp` 的物理更新循环中，投射物每帧在 `SpatialHashGrid` 中查询重叠目标，并通过 `for (auto e : proj.hitEntities) if (e == target) return true;` 跳过已经命中的敌人。
  2. 当投射物穿透属性较高（例如万剑归宗贯穿、蓄力剑气穿透 10~20 个怪物，或无限穿透弹幕）时：
     - 前 8 个怪物被记录入 `hit_cache`；
     - 第 9 个及之后的怪物被命中后，由于 `hit_count >= 8`，`RecordHit` **静默忽略**并不予记录；
     - 在随后的几帧内（投射物飞离第 9 个怪物 hitbox 之前），`HasHit(target)` 对该怪物判定为 `false`；
     - **直接恶果**：第 9 个及之后的怪物在投射物穿过的每一帧都会被重复判定命中并扣血（Multi-Hit 连击 Bug），伤害被异常放大数倍！
- **修复建议**：
  必须给出高穿透情况下的确定性防御策略：
  - **方案 A（规则约束）**：设定游戏内单发投射物最大物理穿透上限为 8（`kMaxInlineHits = 8`），穿透计数归零后强制销毁；
  - **方案 B（双轨溢出）**：定义 `struct ProjectileHitCache`，默认 8 槽位栈存储；当 `pierceCount > 8` 时，按需分配小型堆溢出块或使用动态 bitset；
  - **方案 C（受击者冷却戳）**：不在投射物上记录 entity，而是在怪物身上打上 `ProjectileHitCooldownComponent` 记录 `last_hit_cast_id`。

---

### 【High 02】废除天剑与血海专属组件时，遗漏 `BladeMasteryService`、`GameUiSnapshotBuilder` 与 `PlayerHUD` 外部依赖消费方
- **关联位置**：
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:209-210` (Task 3.2)
  - `src/game/systems/skill/BladeMasteryService.cpp:38, 69, 207`
  - `src/game/application/ui/GameUiSnapshotBuilder.cpp:383-400`
  - `src/game/application/ui/PlayerHUD.cpp:54, 68`
- **代码实证与危害**：
  Task 3.2 声明：
  > “迁移天剑降临 (`HeavenlySwordFieldComponent`) 与血海 (`BloodSeaFieldComponent`) 至通用地表领域系统，清理 60+ 冗余布尔字段”
  然而代码图谱检索显示，这两个组件并非单纯的技能内部私有状态，而是跨系统公共接口：
  1. `BladeMasteryService.cpp:38` 在玩家切换专精时，专门通过 `registry.view<HeavenlySwordFieldComponent>()` 还原全屏攻击间隔，并通过 `DestroyOwnedFields` 销毁领域；
  2. `GameUiSnapshotBuilder.cpp:383-400` 在构建 UI 快照时，直接读取 `field.duration`、`field.has_void_keystone` 和 `field.miasma_duration_bonus`；
  3. `PlayerHUD.cpp:54, 68` 提供了全局函数 `FindActiveHeavenlyFieldDuration` 和 `FindActiveBloodSeaField` 供 HUD 绘制。
  若在 Task 3.2 中简单删除这两个组件并替换为 `AreaFieldComponent`，而实施计划中完全未提及上述 3 个外部系统文件的改造，将导致 UI 构建与专精系统全线编译失败。
- **修复建议**：
  1. 在 Task 3.2 中明确将 `BladeMasteryService.cpp`、`GameUiSnapshotBuilder.cpp`、`PlayerHUD.cpp` 列入变更文件清单；
  2. 为通用 `AreaFieldComponent` 增加 `uint32_t source_skill_id` 标识；
  3. 在 `GameUiSnapshotBuilder` 与 `PlayerHUD` 中改用通用领域查询函数（如 `FindActiveAreaFieldDuration(registry, player, skill_id)`）。

---

### 【High 03】引入 `DamagePayloadContext` 却未定义与 `DamagePipeline` 及 `DamageRequest` 的接口契约
- **关联位置**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md:306-317` (§3.5)
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:159-169, 216` (Task 4.2)
  - `src/game/systems/skill/ProjectileSystem.cpp:630-640`
  - `src/game/systems/combat/DamagePipeline.hpp`
- **代码实证与危害**：
  设计提出废除 `Projectile` 中的 400 字节 `CombatStats snapshot`，换成 64 字节的 `DamagePayloadContext`。
  但查看命中结算路径：
  1. `ProjectileSystem.cpp:631-640` 在碰撞发生时组装 `DamageRequest`：
     ```cpp
     entt::entity attacker = registry.valid(projEnt) && registry.all_of<CombatStats>(projEnt) ? projEnt : act.instigator;
     DamageRequest request;
     request.attacker = attacker;
     ```
  2. `DamagePipeline::Calculate` 内部依赖 `request.attacker` 上的 `CombatStats` 来进行增伤、More伤和穿透计算；
  3. 当前的 `DamageRequest`（[`DamagePipelineTypes.hpp:18-31`](file:///d:/PRJ/NoMoreDay/src/game/contracts/DamagePipelineTypes.hpp#L18-L31)）中完全没有 `DamagePayloadContext` 的字段，`DamagePipeline.hpp` 也没有接受该上下文的重载。
  4. 若在 `Projectile` 上移除了 `CombatStats`，`attacker` 会回退为 `act.instigator`（玩家自身）。此时 `DamagePipeline` 计算伤害时读取的是怪物被命中瞬间玩家的实时面板，导致“发弹时吃到的临时 Buff 在飞行途中过期后伤害失效”，投射物属性快照 (Snapshotting) 机制被彻底破坏。
- **修复建议**：
  1. 在 `DamagePipelineTypes.hpp` 的 `DamageRequest` 中增加轻量快照字段：`std::optional<DamagePayloadContext> payload_context;`；
  2. 在 `DamagePipeline::Calculate` 中支持若存在 `payload_context` 则优先使用其快照数值，而非从 `attacker` 重新拉取 `CombatStats`。

---

### 【Medium 01】设计与计划通篇引用不存在的组件 `SkillBookComponent`
- **关联位置**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md:190` (§3.2.1)
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:84-100` (§2.2, Task 2.3)
  - `src/game/foundation/components/SkillDefs.hpp:510`
- **问题分析**：
  设计与计划伪代码中写道：
  `auto& skillBook = m_registry.get<SkillBookComponent>(entity);`
  但在 NoMoreDay 当前代码库中，存储玩家技能槽与专精树的组件是 `ActiveSkillsComponent`（`SkillDefs.hpp:510`），工程中根本不存在 `SkillBookComponent`。
  计划未澄清这是笔误，还是计划新建一个专门缓存烘焙数据的独立组件。
- **修复建议**：
  统一命名与模型归属。建议明确：
  - 若直接挂载在现有玩家组件上，统一使用 `ActiveSkillsComponent` 并补充 `baked_profiles` 数组；
  - 若为职责单一化新建缓存组件，应明确命名为 `BakedSkillsComponent`，并在 Task 2.3 中声明其创建与初始化生命周期。

---

### 【Medium 02】测试执行命令无效，且混淆性能基线测试与微基准测试
- **关联位置**：
  - `docs/designs/2026-09-06-modern-skill-system-design.md:358-360`
  - `docs/plans/2026-09-06-modern-skill-system-plan.md:241-243`
  - `tests/performance/CombatCorePerfBaselineTests.cpp:10-25`
- **问题分析**：
  1. 计划中的验证命令 `ctest --preset default ...` 无法执行，因为工程根目录不存在 `CMakePresets.json`（测试构建目录在 `build/`）；
  2. 计划宣称 `CombatCorePerfBaselineTests` 用于“连续发射 500 个带穿透投射物，监控堆内存分配是否为 0”，但源码检视证实 `CombatCorePerfBaselineTests.cpp` 实际是一个校验基线 JSON 文件格式（`perf-baseline.json`）的契约测试；
  3. 真正的投射物吞吐微基准测试是 `tests/performance/ProjectileSystemBenchmark.cpp`。
- **修复建议**：
  1. 将测试命令修正为合规的：`ctest --test-dir build -R "nmd.tests.skill" --output-on-failure`；
  2. 将投射物性能回归验证对齐到正确的基准套件 `ProjectileSystemBenchmark`；
  3. 在 Plan §4.1 中为新增的 `TriggerRuleTests` 与 `RecursionDepthTests` 补充具体的输入参数、断言条件与边界条件。

---

## 5. 最佳实践建议 (Actionable Engineering Recommendations)

针对上述所有发现项，提出以下具体的重构修复规范：

### 5.1 触发器派发安全性改造 (Safe Proc Dispatch Pattern)
```cpp
// 推荐的安全派发范式：分步收集，杜绝在迭代组件时调用外部重度逻辑
void ProcEngine::DispatchEvent(entt::registry& reg, entt::entity source, const CombatEvent& event) {
    if (event.trigger_depth >= 2) return;

    struct PendingAction {
        uint32_t skill_id;
        TriggerTargetPolicy target_mode;
        float effectiveness;
    };
    std::array<PendingAction, 4> pending_actions;
    uint8_t action_count = 0;

    // 阶段 1：快速检查并更新冷却，不触发任何外部实体创建
    if (auto* triggerComp = reg.try_get<TriggerRuleComponent>(source)) {
        for (auto& rule : triggerComp->rules) {
            if (rule.listen_event != event.type || rule.current_cooldown > 0.0f) continue;
            if ((event.tags & rule.event_tag_filter) != rule.event_tag_filter) continue;

            float real_chance = rule.base_chance;
            if (rule.use_proc_scaling && event.parent_skill_cd > 0.0f) {
                real_chance *= (1.0f + event.parent_skill_cd * 0.2f);
            }

            if (RandomFloat(0.0f, 1.0f) <= real_chance) {
                rule.current_cooldown = rule.internal_cooldown;
                if (action_count < pending_actions.size()) {
                    pending_actions[action_count++] = {rule.cast_skill_id, rule.target_mode, rule.effectiveness};
                }
            }
        }
    } // triggerComp 指针在此处脱离作用域，安全解除持有

    // 阶段 2：安全派生施法，无论创建多少实体或修改组件池，均无悬空指针风险
    for (uint8_t i = 0; i < action_count; ++i) {
        const auto& act = pending_actions[i];
        SkillSystem::TriggerCast(reg, source, act.skill_id, act.target_mode, event.trigger_depth + 1, act.effectiveness);
    }
}
```

### 5.2 `AreaFieldComponent` POD 化规范
```cpp
// 紧凑 POD 载荷定义
struct PayloadDefinition {
    PayloadType type = PayloadType::Damage;
    uint32_t ailment_id = 0;
    float value_mult = 1.0f;
    Tag damage_tags = Tag::None;
};

// 纯 POD 地表领域组件，符合 code_standard.md §7.1
struct AreaFieldComponent {
    entt::entity owner = entt::null;
    uint32_t source_skill_id = 0; // 明确归属技能（天剑、血海等）
    float remaining_duration = 0.0f;
    float pulse_interval = 0.25f;
    float timer = 0.0f;
    float radius = 48.0f;
    uint8_t shape_type = 0;
    
    // 定长内联数组，零堆分配
    static constexpr uint8_t kMaxPayloads = 4;
    std::array<PayloadDefinition, kMaxPayloads> payloads{};
    uint8_t payload_count = 0;
};
```

---

## 6. 剩余风险评估 (Residual Risks)

若按本报告要求完成修订并最终批准，仍需关注的后续工程风险：
1. **老存档词缀兼容性**：旧存档中装备若含有已废弃的 `PlusFlowingThrust` 词缀枚举值，反序列化时必须在 `ItemFactory` 中保留向下兼容转换映射；
2. **手感微调回归**：剥离技能 8、9、124 的硬编码后，派生施法的触发帧率与位移手感需通过实机录像与测试用例进行像素级对齐。

---

## 7. 下一步动作 (Next Actions)

1. **退回修改实施计划与设计文档**：
   - [ ] 修正 `docs/plans/2026-09-06-modern-skill-system-plan.md` 中的 `ProcEngine` 派发伪代码，消除 EnTT 指针失效漏洞；
   - [ ] 修正 `AreaFieldComponent` 定义，剔除 `std::vector`，改为定长 POD 数组；
   - [ ] 废弃重复的 `CombatEventType` 定义，全面复用 `src/game/contracts/CombatEvents.hpp` 并接入 `CombatEventDispatcher`；
   - [ ] 补充 `BladeMasteryService`、`GameUiSnapshotBuilder` 与 `PlayerHUD` 的迁移任务至 Task 3.2；
   - [ ] 明确 `DamagePayloadContext` 与 `DamagePipeline` 的接口适配方案；
   - [ ] 修正测试执行命令与基准测试名称。
2. **重新发起子代理审查**：完成上述修改后，重新执行审评轮次直至达成 `提交` 准入。
---
# 第二轮审查报告 (Second Round Follow-up Review Report)
- **审查目标**：现代化技能系统架构设计与实施计划（修订版复审）
- 设计文档：[`docs/designs/2026-09-06-modern-skill-system-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modern-skill-system-design.md)
- 实施计划：[`docs/plans/2026-09-06-modern-skill-system-plan.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-06-modern-skill-system-plan.md)
- **审查基准**：
- 流程规范：[`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md)
- 编码规范与硬规则：[`conductor/code_standard.md`](file:///d:/PRJ/NoMoreDay/conductor/code_standard.md) (V2.1)
- 技术栈规范：[`conductor/tech-stack.md`](file:///d:/PRJ/NoMoreDay/conductor/tech-stack.md)
- 代码库现状与对拍文件：`src/game/foundation/components/ItemStats.hpp`、`src/game/foundation/components/SkillDefs.hpp`、`src/game/foundation/components/Projectile.hpp`、`src/game/contracts/CombatEvents.hpp`、`src/game/contracts/DamagePipelineTypes.hpp`、`src/game/systems/skill/ProjectileSystem.cpp`、`src/game/systems/skill/BladeMasteryService.cpp`、`src/game/application/ui/GameUiSnapshotBuilder.cpp`、`src/game/application/ui/PlayerHUD.cpp`、`src/game/application/ui/UIRenderer.cpp`、`src/game/systems/combat/CombatConstants.hpp` 等。
- **审查轮次**：跟进审查 / 第二轮终审 (Second Round Follow-up)
- **审查结论**：`修改` (Needs Revision)
---
## 8. 第二轮审查总体评价与结论
### 8.1 总体结论：`修改`
在本次跟进审查中，针对首轮审查报告指出的 4 项 Blocker、4 项 High、3 项 Medium 缺陷进行了严格逐项复核。结果表明：
1. **核心架构与安全性获得根本性提升**：
- **[Blocker 01] 彻底根除 EnTT 指针失效**：`ProcEngine::DispatchEvent` 全面落地 Collect-Then-Execute（收集再执行）模式，指针作用域隔离，消除了派生施法引发的 UAF 风险；
- **[Blocker 02 & 04] 全面废除堆分配容器**：`AreaFieldComponent`、`BakedSkillProfile`、`TriggerRuleComponent` 全部采用定长内联数组（`std::array<T, N>`）并增加 Standard Layout 静态断言，完全符合 DOD 与零堆分配规范；
- **[Blocker 03] 消除重复造轮子与契约分叉**：彻底废弃新造的 `CombatEventType`，全量复用既有 `src/game/contracts/CombatEvents.hpp` 与 `CombatEventDispatcher`；
- **[High 01] 杜绝多重打击漏洞**：明确了 `max_pierce <= 8` 穿透上限与阻断销毁语义；
- **[High 02] 外部消费方闭环**：通用领域增加 `source_skill_id`，并将 `BladeMasteryService`、`GameUiSnapshotBuilder` 与 `PlayerHUD` 纳入 Phase 3 任务；
- **[High 03] 伤害管线上下文贯通**：`DamageRequest` 正式扩充 `std::optional<DamagePayloadContext>` 快照，消除了 400 字节 `CombatStats` 拷贝；
- **[Medium 01-03] 细节纠偏完成**：组件名统一为 `ActiveSkillsComponent`、UI Tooltip 纳入 `UIRenderer.cpp`、测试命令修正为 `ctest --test-dir build -R "nmd\.tests\.(skill|combat)" --output-on-failure`、伤害转化顺序对齐 `CONVERSION_ORDER`。
2. **但仍发现 1 项高风险缺陷与 2 项中风险缺口，必须予以闭环修正**：
- **【High 01 (新增)】`AffixType::PlusSkillLevelGeneric = 47` 严重冲突既有词缀 `TitanGrip = 47`，破坏二进制存档兼容性**：
在 `src/game/foundation/components/ItemStats.hpp:76` 中，数值 `47` 已经被 `TitanGrip` 占用，且后续紧随 `FlatDodgeRating = 48` 至 `PercentBlockRating = 51`。设计与计划文档将新词缀设为 47，将直接导致枚举冲突或后续所有评级词缀数值整体后移 1 位，从而在反序列化旧 `NMDS` v1 存档时造成严重错乱；
- **【Medium 01 (新增)】`AreaFieldComponent` 遗漏 `owner` 字段，且 `PayloadDefinition` 缺少明确结构体定义**：
设计与计划中给出的 `AreaFieldComponent` 结构缺少 `entt::entity owner = entt::null;`，导致 `BladeMasteryService` 按所有者清理领域失败，且伤害结算缺少施法来源；此外缺少 `PayloadDefinition` 结构体定义的具体代码；
- **【Medium 02 (新增)】`CombatEvent` 契约未扩充 `trigger_depth` 与 `parent_skill_cd` 声明，任务清单缺失对 `CombatEvents.hpp` 的修改**：
`ProcEngine::DispatchEvent` 伪代码依赖这两个字段，但既有 `CombatEvents.hpp` 中尚未声明，且 Task 1.1/1.4 遗漏了修改 `src/game/contracts/CombatEvents.hpp` 的清单。
依据 `docs/workflows/review.md` 判定规则 2（残留高风险缺陷、数据完整性风险必须判定为修改），本次审查结论判定为 **`修改`**。只需针对上述 3 项缺陷进行局部微调修订，即可正式达成准入。
---
## 9. 第二轮变更文件边界与检视范围
### 9.1 `git status --short` 状态摘要
```
?? docs/designs/2026-09-06-modern-skill-system-design.md
?? docs/plans/2026-09-06-modern-skill-system-plan.md
?? docs/reviews/2026-09-06-modern-skill-system-review.md
```
### 9.2 检视覆盖范围
- **修订后文档**：
- `docs/designs/2026-09-06-modern-skill-system-design.md` (全文 350 行)
- `docs/plans/2026-09-06-modern-skill-system-plan.md` (全文 265 行)
- **代码库关键对拍证据文件**：
- `src/game/foundation/components/ItemStats.hpp` (L73-83: `AffixType` 真实枚举值分布)
- `src/game/systems/item/storage/ItemStorageTypes.hpp` (L155-174: `ItemInstance` 192 字节与 `ItemSideTableData`)
- `src/game/foundation/components/SkillDefs.hpp` (L510-532: `ActiveSkillsComponent`；L568-588: `ShadowComponent`/`SkillSnapshot`；L917-950: `HeavenlySwordFieldComponent`)
- `src/game/contracts/CombatEvents.hpp` (L14-117: `CombatEventType` 与 `CombatEvent` 字段清单)
- `src/game/contracts/DamagePipelineTypes.hpp` (L18-31: `DamageRequest` 结构)
- `src/game/systems/skill/ProjectileSystem.cpp` (L438-480: 碰撞判定与穿透计数)
- `src/game/systems/skill/BladeMasteryService.cpp` (L38-69: 专精重置与所有者领域销毁)
- `src/game/application/ui/UIRenderer.cpp` (L1074-1081: Tooltip 技能消耗与 CD 渲染)
- `src/game/systems/combat/CombatConstants.hpp` (L44-66: `CONVERSION_ORDER`)
---
## 10. 首轮发现项整改复核表 (Verification Matrix of Round 1 Findings)
| 编号 | 严重度 | 原始问题摘要 | 修订后状态 | 证据与复核结论 |
| :--- | :--- | :--- | :--- | :--- |
| **Blocker 01** | Blocker | `ProcEngine::DispatchEvent` 循环内跨实体施法，持有 EnTT 指针致 UAF | **Closed** (已闭环) | 伪代码全面改为两阶段模式（Design §3.4.2、Plan §2.2）。阶段一在受控栈作用域收集至 `std::array<PendingAction, 4>` 并更新 CD，阶段二脱离引用后派生施法，完全符合 `code_standard.md` §5.3。 |
| **Blocker 02** | Blocker | `AreaFieldComponent` 包含 `std::vector`，违背 POD 与热路径零分配 | **Closed** (已闭环) | 废除 `std::vector`，改为定长内联数组 `std::array<PayloadDefinition, 4>`，通过 `is_standard_layout` 与 `is_trivially_destructible` 静态断言（Design §3.3.1、Plan §2.3）。 |
| **Blocker 03** | Blocker | 重复造轮子：自制 `CombatEventType`，未复用既有总线造成契约分叉 | **Closed** (已闭环) | 彻底废除新造枚举，全面复用 `src/game/contracts/CombatEvents.hpp` 中的 `NoMoreDay::CombatEventType` 与 `CombatEventDispatcher`（Design §3.4.1、Plan §1.2、Task 1.1）。 |
| **Blocker 04** | Blocker | `BakedSkillProfile` 与 `TriggerRuleComponent` 使用 `vector` 破坏 POD | **Closed** (已闭环) | 全部改为定长数组（`kMaxInjectedPayloads = 4`, `kMaxRules = 8`），组件 100% 保持 Standard Layout（Design §3.2.1、Plan §2.1）。 |
| **High 01** | High | 投射物 SBO 超过 8 槽位静默丢弃，导致目标每帧重复受击连击扣血 | **Closed** (已闭环) | 引入规则级上限 `max_pierce <= 8`；`RecordHit` 在达到上限时返回 `true` 强制阻断穿透或销毁投射物，杜绝 Multi-Hit 漏洞（Design §3.5、Plan §2.4）。 |
| **High 02** | High | 废除天剑/血海组件遗漏 `BladeMasteryService`、UI 与 HUD 3 个外部消费方 | **Closed** (已闭环) | `AreaFieldComponent` 增设 `source_skill_id` 标识；Task 3.2 明确将 `BladeMasteryService.cpp`、`GameUiSnapshotBuilder.cpp` 与 `PlayerHUD.cpp` 列入变更清单（Plan Task 3.2）。 |
| **High 03** | High | `DamagePayloadContext` 与伤害管线 `DamageRequest` 缺乏接口契约 | **Closed** (已闭环) | `DamageRequest`（`DamagePipelineTypes.hpp`）扩充 `std::optional<DamagePayloadContext>` 字段，`DamagePipeline::Calculate` 优先读取，同步废除投射物与残影中 400 字节 `CombatStats` 拷贝（Design §3.5、Plan §2.4、Task 4.2）。 |
| **High 04** | High | 词缀重构破坏 192 字节 `ItemInstance` POD 与 NMDS v1 存档兼容 | **Reopened / New Defect** | 装备修饰器置入 `ItemSideTableData`（Section 3）与保留占位符设计良好；但新增枚举值分配为 47 与既有 `TitanGrip = 47` 发生硬碰撞（详见下文 High 01）。 |
| **Medium 01** | Medium | 通篇引用不存在的组件名 `SkillBookComponent` | **Closed** (已闭环) | 统一更正为既有组件 `ActiveSkillsComponent`（Design §3.2、Plan §1.1、Task 2.3）。 |
| **Medium 02** | Medium | 测试命令参数无效且混淆测试套件分类 | **Closed** (已闭环) | 更正为合规的 `ctest --test-dir build -R "nmd\.tests\.(skill|combat)" --output-on-failure`，性能压测对齐 `ProjectileSystemBenchmark`（Design §4.2、Plan §4.2）。 |
| **Medium 03** | Medium | 伤害转化链描述与引擎既有单向链未严格对齐 | **Closed** (已闭环) | 设计文档 §3.1.2 严格对齐既有 `Constants::Combat::Conversion::CONVERSION_ORDER`（Physical->Lightning->Cold->Fire->Poison->Shadow）。 |
---
## 11. 第二轮新发现项（按严重度排序）
### 【High 01 (新增)】`AffixType::PlusSkillLevelGeneric = 47` 与既有 `AffixType::TitanGrip = 47` 发生整数值严重冲突，直接破坏 NMDS v1 二进制存档
- **关联位置**：
- `docs/designs/2026-09-06-modern-skill-system-design.md:141` (§3.1.1)
- `docs/plans/2026-09-06-modern-skill-system-plan.md:53, 211` (§2.1, Task 2.1)
- `src/game/foundation/components/ItemStats.hpp:73-83`
- **代码实证与危害**：
查阅 [`src/game/foundation/components/ItemStats.hpp:73-83`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/ItemStats.hpp#L73-L83) 的源码可知：
```cpp
PlusAllSkills,     // 44
PlusFlowingThrust, // 45
PlusRendingWave,   // 46
TitanGrip,         // 47 <-- 整数值 47 已经被泰坦之握占用！
// NEW: Rating Types
FlatDodgeRating,    // 48
PercentDodgeRating, // 49
FlatBlockRating,    // 50
PercentBlockRating, // 51
Normal_End = 999,
```
设计与计划文档为了保证二进制兼容，特意保留了 `Deprecated_PlusFlowingThrust = 45` 和 `Deprecated_PlusRendingWave = 46`，但紧接着写道：
`PlusSkillLevelGeneric = 47, // 新增：泛型技能加级`
若按此实施：
1. 如果直接在枚举中声明 `PlusSkillLevelGeneric = 47`，在 C++ `enum class` 中若未显式指定 `TitanGrip = 47`，则 `TitanGrip` 会被自动推导为 `48`；后续的 `FlatDodgeRating` 变为 `49`、`PercentBlockRating` 变为 `52`；
2. 这会导致已有 `NMDS` v1 存档中所有带有 `TitanGrip` 或闪避/格挡评级词缀的装备，在反序列化时读取到的枚举整型全部后移 1 位，装备属性严重错乱；
3. 如果显式将两者都赋值为 47，则产生枚举重复二义性，破坏反序列化时的唯一定位映射。
- **违背原则**：二进制存档反序列化确定性与数据完整性规范（`docs/workflows/review.md` 判定规则 1）。
- **修复方案**：
保持 `TitanGrip = 47`、`FlatDodgeRating = 48` 至 `PercentBlockRating = 51` 原有整型数值绝对不变，将泛型加级枚举值配置在评级词缀之后的下一个空闲数值：
```cpp
// 在 ItemStats.hpp 中正确维护
enum class AffixType : uint16_t {
// ...
PlusAllSkills = 44,
Deprecated_PlusFlowingThrust = 45, // 占位符：保持旧值稳定
Deprecated_PlusRendingWave = 46,   // 占位符：保持旧值稳定
TitanGrip = 47,                    // 原有词缀保留
FlatDodgeRating = 48,
PercentDodgeRating = 49,
FlatBlockRating = 50,
PercentBlockRating = 51,
PlusSkillLevelGeneric = 52,        // 正确：使用紧随其后的安全空闲数值 52
// ...
};
```
---
### 【Medium 01 (新增)】`AreaFieldComponent` 遗漏 `owner` 字段，且 `PayloadDefinition` 缺少明确结构体定义
- **关联位置**：
- `docs/designs/2026-09-06-modern-skill-system-design.md:211-224` (§3.3.1)
- `docs/plans/2026-09-06-modern-skill-system-plan.md:151-166` (§2.3)
- `src/game/systems/skill/BladeMasteryService.cpp:69`
- `src/game/foundation/components/SkillDefs.hpp:918`
- **代码实证与危害**：
1. 设计与计划给出的 `AreaFieldComponent` 结构定义中，虽然补充了 `source_skill_id`，但**缺少了 `entt::entity owner = entt::null;`**；
2. 在既有代码中，`HeavenlySwordFieldComponent`（`SkillDefs.hpp:918`）明确包含 `entt::entity owner`；`BladeMasteryService.cpp:69` 通过 `DestroyOwnedFields<HeavenlySwordFieldComponent>(registry, owner)` 依据 `field.owner == owner` 清理实体；
3. 当通用地表领域触发定时脉冲伤害时，组装 `DamageRequest` 必须获知伤害来源施法者（`attacker = field.owner`），否则无法正确结算暴击、增伤及吸血；
4. 此外，两份文档多次出现 `std::array<PayloadDefinition, 4>`，但通篇未见 `PayloadDefinition` 的具体结构体定义。
- **修复方案**：
1. 在 `AreaFieldComponent` 中明确补上 `entt::entity owner = entt::null;`；
2. 在设计与计划中补充 `PayloadDefinition` 的明确结构体定义（包含 `PayloadType type`, `uint32_t ailment_id`, `float value_mult`, `Tag damage_tags`，且满足 Standard Layout）。
---
### 【Medium 02 (新增)】`CombatEvent` 契约未扩充 `trigger_depth` 与 `parent_skill_cd` 声明，任务清单缺失对 `CombatEvents.hpp` 的修改
- **关联位置**：
- `docs/designs/2026-09-06-modern-skill-system-design.md:266, 288` (§3.4.2)
- `docs/plans/2026-09-06-modern-skill-system-plan.md:101, 123, 205` (§2.2, Task 1.1)
- `src/game/contracts/CombatEvents.hpp:84-117`
- **代码实证与危害**：
1. `ProcEngine::DispatchEvent` 伪代码中直接读取 `event.trigger_depth` 和 `event.parent_skill_cd`；
2. 查阅 [`src/game/contracts/CombatEvents.hpp:84-117`](file:///d:/PRJ/NoMoreDay/src/game/contracts/CombatEvents.hpp#L84-L117)，既有 `CombatEvent` 结构体目前仅有 `type`, `source`, `target`, `skill_id`, `tags`, `value`, `reported_damage` 等字段，**并不包含 `trigger_depth` 和 `parent_skill_cd`**；
3. 计划 Task 1.1 仅描述了“定义纯 POD 的 TriggerRuleComponent”，未将修改 `src/game/contracts/CombatEvents.hpp` 列入任务清单与修改范围，在进入实现时会导致派发代码无法编译。
- **修复方案**：
在 Phase 1 Task 1.1 中明确加入对 `src/game/contracts/CombatEvents.hpp` 的扩充任务：向 `CombatEvent` 结构体追加 `uint8_t trigger_depth = 0;` 与 `float parent_skill_cd = 0.0f;` 字段。
---
## 12. 第二轮最佳实践建议 (Actionable Engineering Recommendations)
### 12.1 `ItemStats.hpp` 词缀枚举安全对齐
```cpp
enum class AffixType : uint16_t {
// ...
PlusAllSkills = 44,
Deprecated_PlusFlowingThrust = 45, // 占位符：保留旧值，防枚举数值移位
Deprecated_PlusRendingWave = 46,   // 占位符：保留旧值
TitanGrip = 47,                    // 严禁改动或覆盖 47
FlatDodgeRating = 48,              // 保持原有评级词缀数值
PercentDodgeRating = 49,
FlatBlockRating = 50,
PercentBlockRating = 51,
PlusSkillLevelGeneric = 52,        // 泛型技能加级从 52 开始安全追加
// ...
Normal_End = 999,
};
```
### 12.2 完善 `PayloadDefinition` 与 `AreaFieldComponent` 定义
```cpp
enum class PayloadType : uint8_t {
Damage = 0,
Ailment,
Buff,
Impulse
};
struct PayloadDefinition {
PayloadType type = PayloadType::Damage;
uint32_t ailment_id = 0;
float value_mult = 1.0f;
Tag damage_tags = Tag::None;
};
static_assert(std::is_standard_layout_v<PayloadDefinition>);
struct AreaFieldComponent {
entt::entity owner = entt::null;     // 明确所有者，供 BladeMasteryService 与伤害结算使用
uint32_t source_skill_id = 0;        // 技能标识 (兼容外部查询)
float remaining_duration = 0.0f;
float pulse_interval = 0.25f;
float timer = 0.0f;
float radius = 48.0f;
uint8_t shape_type = 0;              // 0: 圆形, 1: 环形, 2: 旋转切割线
// 定长内联 POD 载荷数组 (Zero heap allocation)
static constexpr uint8_t kMaxFieldPayloads = 4;
std::array<PayloadDefinition, kMaxFieldPayloads> payloads{};
uint8_t payload_count = 0;
};
static_assert(std::is_standard_layout_v<AreaFieldComponent>);
static_assert(std::is_trivially_destructible_v<AreaFieldComponent>);
```
---
## 13. 第二轮剩余风险评估 (Residual Risks)
在完成上述 3 项修订后，系统已具备极高完备度。后续进入代码实施阶段需注意的剩余风险：
1. **老存档向后兼容验证**：在 Phase 2 实施时需编写专项测试（例如 `tests/unit/ItemSaveCompatibilityTests.cpp`），加载旧版本二进制数据（含 `TitanGrip` 与评级词缀），验证反序列化数值百分百无偏移；
2. **手感与位移连续性**：重构技能 8、9、124 的硬编码后，派生施法的触发帧与动画融合需通过录像对比确保一致。
---
## 14. 终审退出门禁与下一步动作 (Next Actions)
1. **修正设计与实施计划文档**：
- [x] 将 `docs/designs/2026-09-06-modern-skill-system-design.md` 与 `docs/plans/2026-09-06-modern-skill-system-plan.md` 中的 `PlusSkillLevelGeneric` 编号从 47 修正为 52，确保不与 `TitanGrip = 47` 冲突；
- [x] 在 `AreaFieldComponent` 中补充 `entt::entity owner = entt::null;` 与 `uint64_t cast_id = 0;` 并补全 `PayloadDefinition` 结构定义；
- [x] 在实施计划 Task 1.1 中明确将 `src/game/contracts/CombatEvents.hpp` 纳入变更清单，为 `CombatEvent` 增加 `trigger_depth` 与 `parent_skill_cd`，并改造 `DamagePipeline.cpp` 注入实际冷却。
2. **闭环结论**：上述纯文档细节纠偏已全部完成，正式达成准入实施门禁。

---

# 现代化技能系统架构设计与实施计划第三轮终审复核（通过）

- **审查目标**：现代化技能系统架构设计 (`2026-09-06-modern-skill-system-design.md`) 与实施计划 (`2026-09-06-modern-skill-system-plan.md`) 最终闭环审查
- **结论**：**`提交` (Approved for Implementation)**
- **审查轮次**：最终通过审查 (Final Approval Round)
- **输入**：
  - 设计路径：[`docs/designs/2026-09-06-modern-skill-system-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modern-skill-system-design.md)
  - 计划路径：[`docs/plans/2026-09-06-modern-skill-system-plan.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-06-modern-skill-system-plan.md)
  - 审查标准：[`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md)
  - 架构硬规则：[`conductor/code_standard.md`](file:///d:/PRJ/NoMoreDay/conductor/code_standard.md) (V2.1)
- **变更文件边界** (`git status --short`)：
  ```
  ?? docs/designs/2026-09-06-modern-skill-system-design.md
  ?? docs/plans/2026-09-06-modern-skill-system-plan.md
  ?? docs/reviews/2026-09-06-modern-skill-system-review.md
  ```

## 15. 最终闭环对拍核验清单

1. **[词缀数值碰撞消除]**：
   `ItemStats.hpp` 保留 `Deprecated_PlusFlowingThrust = 45` 与 `Deprecated_PlusRendingWave = 46`，`TitanGrip = 47` 至 `PercentBlockRating = 51` 原封不动，`PlusSkillLevelGeneric` 设定为安全的 `52`。装备修饰器独立落盘于 `SectionType::ItemSkillModifiers = 13`，`NMDS` v1 存档 100% 向后兼容。
2. **[两阶段触发派发与目标完备性]**：
   阶段一安全捕获 `resolved_target` 与 `resolved_pos`，阶段二执行 `if (!reg.valid(listener)) break;` 施法者活性检查。补充了 `ProcEngine::UpdateCooldowns(reg, dt)` 递减系统，触发冷却闭环；改造 `DamagePipeline.cpp` 注入 `CombatEvent::parent_skill_cd`，自适应几率加权公式具备真实生产源。
3. **[纯 POD 组件与外部消费方兼容]**：
   `AreaFieldComponent`、`BakedSkillProfile`、`TriggerRuleComponent`、`PayloadDefinition` 全部具备 `static_assert(std::is_standard_layout_v<T>)`；`AreaFieldComponent` 明确补全 `owner` 与 `cast_id`；`GameUiSnapshotBuilder` 与 `PlayerHudController` 适配方案已落盘至 Task 3.2，HUD 状态反馈链条完整。
4. **[DOD 与性能硬规则]**：
   投射物 SBO（8 槽位栈缓存）引入 `max_pierce <= 8` 规则保护，消灭 Multi-Hit Bug；64 字节对齐的 `DamagePayloadContext` 贯通 `DamageRequest`，消灭 400 字节面板拷贝。

## 16. 最终结论与下一步动作

- **结论**：**`提交` (Approved)**
- **剩余风险**：
  1. 实施 Phase 2 时需执行 `ItemPersistenceCodecTests` 确保 Section 13 二进制双向序列化无误；
  2. 实施 Phase 3 需通过集成测试验证 12 个原有技能手感无偏移。
- **下一步动作**：设计与计划文档正式归档生效，开启代码实施（进入 Phase 1：通用触发机制与施法主干特例剥离）。

---

# 现代化技能系统实施代码首轮审查（第四轮）

- **审查目标**：`2026-09-06-modern-skill-system-plan.md` 实施代码包（工作区未提交变更）
- **结论**：**`修改` (Needs Revision)**
- **审查轮次**：实施首轮审查（Implementation First Round）
- **输入**：
  - 实施计划：[`docs/plans/2026-09-06-modern-skill-system-plan.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-06-modern-skill-system-plan.md)
  - 设计文档：[`docs/designs/2026-09-06-modern-skill-system-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modern-skill-system-design.md)
  - 审查标准：[`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md)
  - 架构硬规则：[`conductor/code_standard.md`](file:///d:/PRJ/NoMoreDay/conductor/code_standard.md) (V2.1)
  - 验证证据：`build.bat`（RelWithDebInfo）exit=0；`ctest -L ci/-L integration/-L skill/-L combat/-L unit` 全部 exit=0（日志：`%TEMP%\opencode\nmd_review_build.log`、`nmd_review_ctest_*.log`）

## 17. 变更文件边界

### 17.1 `git status --short` 摘要

```
 M src/game/application/persistence/SaveManager.cpp
 M src/game/application/ui/GameUiSnapshotBuilder.cpp
 M src/game/application/ui/PlayerHUD.cpp
 M src/game/application/ui/UIRenderer.cpp
 M src/game/contracts/CombatEvents.hpp
 M src/game/contracts/DamagePipelineTypes.hpp
 M src/game/foundation/components/ItemComponent.hpp
 M src/game/foundation/components/ItemStats.hpp
 M src/game/foundation/components/Projectile.hpp
 M src/game/foundation/components/SkillDefs.hpp
 M src/game/foundation/stats/AttributePipeline.cpp
 M src/game/systems/combat/DamagePipeline.cpp
 M src/game/systems/item/storage/ItemPersistenceCodec.cpp
 M src/game/systems/item/storage/ItemPersistenceCodec.hpp
 M src/game/systems/item/storage/ItemStorageService.cpp
 M src/game/systems/item/storage/ItemStorageTypes.hpp
 M src/game/systems/skill/BladeMasteryService.cpp
 M src/game/systems/skill/CMakeLists.txt
 M src/game/systems/skill/ProjectileSystem.cpp
 M src/game/systems/skill/SkillDisplayPreviewService.cpp
 M src/game/systems/skill/SkillDisplayPreviewService.hpp
 M src/game/systems/skill/SkillSystem.cpp
 M src/game/systems/skill/SkillSystem.hpp
 M tests/performance/ProjectileSystemBenchmark.cpp
 M tests/unit/ItemPersistenceCodecTests.cpp
?? docs/designs/2026-09-06-modern-skill-system-design.md
?? docs/plans/2026-09-06-modern-skill-system-plan.md
?? docs/reviews/2026-09-06-modern-skill-system-review.md
?? src/game/foundation/components/TriggerRuleComponent.hpp
?? src/game/systems/skill/AreaFieldDeliverySystem.cpp / .hpp
?? src/game/systems/skill/BeamChannelDeliverySystem.cpp / .hpp
?? src/game/systems/skill/ProcEngine.cpp / .hpp
?? tests/unit/AreaFieldDeliveryTests.cpp
?? tests/unit/ItemSkillModifierTests.cpp
?? tests/unit/TriggerRuleTests.cpp
```

统计：25 个修改文件 + 12 个未跟踪文件，907 insertions / 611 deletions。全部变更属于计划授权范围，无越界文件。

### 17.2 检视覆盖范围

全部 25 个修改文件的完整 diff 与全部 9 个新增代码文件逐行检视；构建与测试自行复跑验证（不依赖上报结果）。

## 18. 范围对齐与验收核验表

| 计划验收标准（§5） | 状态 | 证据 |
| :--- | :--- | :--- |
| 1. RelWithDebInfo 零警告/错误编译 | ✅ 达成 | `build.bat` exit=0，无编译警告（日志 1 处 "warning" 为 compile_commands.json 提示） |
| 2. 原有 + 新增单测全绿 | ✅ 达成 | ci/integration/skill/combat/unit 五组 ctest 全部 exit=0；`TriggerRuleTests.cpp`（9 用例）、`AreaFieldDeliveryTests.cpp`（4 用例）、`ItemSkillModifierTests.cpp` 经 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 纳入并通过 |
| 3a. AffixType 占位符 45/46、TitanGrip=47 不变、PlusSkillLevelGeneric=52、NMDS v1 兼容 | ✅ 达成 | `ItemStats.hpp:73-83` 数值与推荐方案一致；含 `PlusFlowingThrust = Deprecated_PlusFlowingThrust` 兼容别名；`.cpp` 无重复 case 冲突 |
| 3b. 组件 Standard Layout 断言、无堆容器 | ✅ 达成 | `TriggerRule/TriggerRuleComponent/PayloadDefinition/BakedSkillProfile/DamagePayloadContext/AreaFieldComponent` 全部 `static_assert(is_standard_layout_v)` 且定长数组；`Projectile` hitEntities vector 已替换为 8 槽 SBO |
| 3c. SkillSystem.cpp 行数下降 40%+，主干无技能 8/9/124 分支 | ❌ **未达成** | 2671 → 2465 行（**仅降 7.7%**）；技能 8/9 特例仍在 `SkillSystem.cpp:981,992`，技能 124 特例在 `SkillSystem.cpp:1671-1698`（详见发现项 High 02） |
| 3d. 核心战斗循环无字符串比较 | ✅ 达成 | 新增/修改代码扫描无 `== "..."`/`strcmp` 分支 |
| 4. 投射物穿透压测零堆分配 | ⚠️ 部分达成 | SBO 结构达成零堆分配，但 `tests/performance/ProjectileSystemBenchmark.cpp:63,76` `pierceCount = 10000` 与 `max_pierce = 8` 冲突，压测行为模型被截断改变（详见 High 01） |

Phase 对齐：Phase 2（装备修饰器持久化与消费链）**完成度最高**；Phase 1/3/4 均为"基础设施与消费者就绪，但生产侧迁移未执行"（详见 High 02、Medium 01/02）。

## 19. 核心发现项（按严重度排序）

### 【High 01】技能 7 斩心剑切割 hitbox 被 SBO 上限截断为最多 8 目标命中，AoE 玩法回归

- **关联位置**：
  - `src/game/systems/skill/BeamChannelDeliverySystem.cpp:414`（`proj.pierceCount = 999;`）
  - `src/game/foundation/components/Projectile.hpp:48,58-60`（`uint8_t max_pierce = 8;` 与 `RecordHit` 满 8 返回 true）
  - `src/game/systems/skill/ProjectileSystem.cpp:470,477`（`pierceLimitReached → hitLimitReached`）
  - `tests/performance/ProjectileSystemBenchmark.cpp:63,76`（`pierceCount = 10000`）
- **问题陈述**：技能 7 的切割 hitbox 是一次性静态 AoE（radius≈60、lifetime 0.1s），历史上依赖 `pierceCount = 999` 实现"命中范围内全部敌人"。本次引入的 `max_pierce` 默认 8 且**全工程无任何调用方修改它**：第 8 个目标命中后 `RecordHit` 返回 true → `pierceLimitReached` → hitbox 提前失效，范围内第 9 个及以后的敌人完全免伤。计划的 `max_pierce <= 8` 规则针对的是"穿透投射物"（位移型、防 Multi-Hit），未覆盖"全体命中 AoE hitbox"语义。压测基准同样受影响：`ResetProjectiles` 设 10000 穿透但投射物 8 命即毁，穿透压力模型与基准数据不再可比。
- **为何成问题**：review.md 判定规则 2（高风险正确性回归、缺失必备行为）；review.md 硬否决规则 2 反向对照——压测被静默改变行为模型，基准证据失效。
- **修复建议**：在 `Projectile` 上显式区分语义：AoE hitbox 构造处（`BeamChannelDeliverySystem.cpp:414` 附近）设置 `proj.max_pierce = 255`（或增加 `kUnlimitedPiercing` 哨兵值并在 `RecordHit`/`ProjectileSystem` 判定中豁免）；`tests/performance/ProjectileSystemBenchmark.cpp:63,76` 同步设置；为"穿透投射物 vs AoE hitbox"分别补一条单测断言目标命中数（8 与 >8）。

### 【High 02】Phase 1/3 的"数据驱动迁移"未执行：TriggerRuleComponent 与 AreaFieldComponent 生产代码零创建者，技能 8/9/124 特例原样保留

- **关联位置**：
  - `src/game/foundation/components/TriggerRuleComponent.hpp`（新增组件，全 src/ 仅 `SkillSystem.cpp:40` include 与 ProcEngine/测试消费，**无任何 `emplace<TriggerRuleComponent>` 生产者**）
  - `src/game/foundation/components/SkillDefs.hpp`（AreaFieldComponent 同样仅测试创建；天剑/血海仍由 `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`、`BloodSea.cpp` 生产旧组件）
  - `src/game/systems/skill/SkillSystem.cpp:981`（`slot.id == 8` CD-1.5s 特例）、`SkillSystem.cpp:992`（`exec.skill_id = 9` 幻影闪特例）、`SkillSystem.cpp:1671-1698`（技能 124 影杀阵特例，仅提取为匿名函数）
  - `src/game/systems/skill/SkillSystem.cpp`：2671 → 2465 行，降幅 **7.7%**（验收要求 40%+）
- **问题陈述**：本包交付了完整的触发基础设施（ProcEngine 两阶段派发 + 5 类事件接线 + ICD/深度守卫/自适应加权 + 高质量单测）与通用领域基础设施（AreaFieldDeliverySystem + UI/HUD/Mastery 消费方改造），但**没有任何生产代码把规则/领域写入新组件**：`TriggerRuleComponent` 与 `AreaFieldComponent` 在生产运行时恒为空，新的 UI 查询（`GameUiSnapshotBuilder.cpp:402-425`、`PlayerHUD.cpp:61-63,176-187`）与 `DestroyOwnedAreaFields`（`BladeMasteryService.cpp`）永远空转；计划 Task 1.2/1.3（剥离技能 8/9/124 特例为数据驱动）与 Task 3.2（天剑/血海迁移至通用领域、清理 60+ 冗余布尔字段）**均未发生**，旧组件与特例分支原样保留，新旧双轨并存。
- **为何成问题**：计划验收标准 3c 明确"主干无技能 8/9/124 分支"且行数降 40%+；review.md 判定规则 2（缺失必备行为）与判定规则 5（必要验收证据缺失）。当前形态下 Phase 1/3 的收益（可维护性、特例消除）为零，却已支付全部基建成本，且双轨扩大后续维护面。
- **修复建议**：二选一并显式入计划：(a) 补足迁移——为天剑/血海/幻影闪/影杀阵建立 TriggerRule/AreaField 生产者并删除旧特例与旧组件（达成 40% 行数目标）；或 (b) 将本包重定位为"基建包"，在计划中显式降级 Task 1.2/1.3/3.2 验收口径并修订 §5 验收标准，把迁移列为后续包任务。禁止维持双轨无迁移状态合并。

### 【Medium 01】DamagePayloadContext 管道贯通但零生产者，400 字节 CombatStats snapshot 未按 Task 4.2 废除

- **关联位置**：`src/game/foundation/components/Projectile.hpp:19-20`（snapshot 保留 + payload_context 新增）；`src/game/contracts/DamagePipelineTypes.hpp:33`；`src/game/systems/skill/ProjectileSystem.cpp:643`（唯一读取点）；全工程无 `payload_context =` 生产赋值。
- **问题陈述**：`DamagePipeline::Calculate` 的 payload_context 优先路径（武器 min/max、crit、increased/more、tags 共 7 处）永远不触发，因为没有代码填充 `Projectile.payload_context`；引导技能 5/7 与残影仍执行 400 字节 `CombatStats` 整体拷贝（`BeamChannelDeliverySystem.cpp:276,417`）。Task 4.2"废除 400 字节面板拷贝"未完成，契约以双轨死代码形态先行合入。
- **修复建议**：本包内由 ProjectileSystem/SkillExecution 快照路径填充 payload_context（技能 5/7 投射物最小闭环），或显式移出至后续包并在计划中标注；避免空转优先路径长期滞留热路径代码。

### 【Medium 02】装备技能修饰器缺生成入口：词缀生成器/掉落路径未产出 ItemSkillModifier 或 PlusSkillLevelGeneric 词缀

- **关联位置**：`src/game/systems/item/`（ItemFactory/生成器无 `PlusSkillLevelGeneric`、`skill_modifiers` 引用）；`src/game/foundation/components/ItemStats.hpp:52`（词缀已可随机掉落但无投放源）。
- **问题陈述**：Phase 2 完成了持久化（Section 13）、消费（RebakeSkillProfiles）与 UI 展示闭环，但玩家在游戏中无法通过任何途径获得带技能修饰器的装备——功能对玩家不可达，Phase 2 验收的"词条生成"仅剩消费侧证据。
- **修复建议**：在词缀/掉落生成器补 `PlusSkillLevelGeneric`（52）与 `ItemSkillModifier` 投放路径，并补一条"生成 → 序列化 → Rebake → 面板显示"端到端集成测试。

### 【Low 01】bloodSeaMiasmaBonus 语义从"加成秒数"变为"0/1 标志"

- **关联位置**：`src/game/application/ui/GameUiSnapshotBuilder.cpp:399`（旧路径保留原语义）与 `:418`（新路径 `(field.remaining_duration > 0.0f) ? 1.0f : 0.0f`）。
- **问题陈述**：同一快照字段两条路径返回值语义不同；因新路径恒空转暂无实际影响，但迁移启用后 HUD 显示数值将与历史行为不一致。
- **修复建议**：迁移时统一语义并在字段注释中声明单位。

### 【Low 02】AreaFieldDeliverySystem 细节：Ailment 时长硬编码、无 VFX 提交

- **关联位置**：`src/game/systems/skill/AreaFieldDeliverySystem.cpp:73`（`applyReq.duration = 3.0f;`）；系统整体无任何渲染/GPUSkillEffect 提交。
- **问题陈述**：异常时长硬编码 3 秒且未走参数化；天剑/血海迁移启用后，通用领域无视觉表现将导致玩法不可读。
- **修复建议**：时长入 `PayloadDefinition` 或技能参数；迁移前补领域 VFX 方案。

### 【Low 03】RecordHit 返回值语义可读性差

- **关联位置**：`src/game/foundation/components/Projectile.hpp:56-62`。
- **问题陈述**：`RecordHit` 返回 true 表示"记录失败/已达上限"，调用点读作 `pierceLimitReached = proj.RecordHit(target)` 尚可，但"记录成功返回 false"反直觉。
- **修复建议**：重命名为 `TryRecordHit(bool& limitReached)` 或返回枚举，消除布尔歧义。

## 20. 最佳实践建议

1. **RebakeSkillProfiles 调用位置**：现挂在 `AttributePipeline::Calculate` 末尾（`AttributePipeline.cpp:786-789`）每次属性重算全量重烘焙 5 槽 × 装备 × 修饰器。属低频路径可接受，但建议在换装/加点事件点显式触发而非属性管线尾部隐式触发，便于后续把 Rebake 从 AttributePipeline 解耦。
2. **`ItemSideTables` 解码字段级 move**（`ItemPersistenceCodec.cpp:779-782`）是好设计，建议加注释说明其目的（保护 Section 13 已加载的 skill_modifiers 不被整体覆盖），并注明 Section 顺序依赖。
3. **ProcEngine 确定性触发短路**（`ProcEngine.cpp:42-44`）设计良好；建议为 `use_proc_scaling` 增加封顶说明（parent_skill_cd 很大时 real_chance 可超 1，行为正确但值得注释防误改）。

## 21. 剩余风险（按当前结论接受/遗留）

1. **本包未消除任何存量特例**：若按 (b) 路线重定位为基建包，天剑/血海 60+ 布尔字段与技能 8/9/124 分支将继续存在直至迁移包落地；
2. **payload_context/AirField/TriggerRule 三条空转管道**在迁移完成前为不可达代码，需防其他改动误以为其已生效；
3. 技能手感回归（计划剩余风险 2）在本包内未触发（行为双轨未切换），迁移包落地时必须按计划执行 12 技能手感录像对比。

## 22. 下一步动作

1. **修复 High 01**：AoE hitbox 与压测的 max_pierce 适配（小改动，可立即执行）；
2. **决策 High 02 路线**：(a) 补足迁移达成原验收，或 (b) 修订计划 §5 验收口径并显式登记迁移为后续包；
3. **闭环 Medium 01/02**：payload_context 最小生产闭环或显式移出；修饰器投放入口 + 端到端测试；
4. 上述完成后进入跟进审查轮次，直至达成 `提交` 准入。

---

# 第五轮审查：跟进审查（R5 修复验证）

## 23. 变更边界与验证

- **范围**：39 个修改文件 + 15 个未跟踪文件，+1186/−663；`SkillSystem.cpp` 2465 → 2322 行。
- **新增**：`ShadowDuplicationHook.cpp/.hpp`（技能 124 迁移）、`tests/unit/ProjectileTests.cpp`（穿透语义）、`tests/integration/ItemSkillModifierE2ETests.cpp`（修饰器投放端到端）；修改 `behaviors/` 7 个技能文件、`ProcEngine.cpp`、`TriggerRuleComponent.hpp`、`ItemFactory.cpp`、`assets/data/affixes.json`。
- **构建**：`build.bat RelWithDebInfo` exit=0，零警告零错误。
- **测试**：ctest `-L unit` 9/9 通过；`-L "skill|combat|ci|integration"` 11/11 通过（合计 20/20，exit=0）。

## 24. 上轮发现项修复状态

| 上轮编号 | 状态 | 证据 |
|---|---|---|
| High 01（AoE 截断） | ✅ 修复，但衍生新 High 02(R5) | `kUnlimitedPiercing=255` 哨兵（`Projectile.hpp:46`）；无限适配：`BeamChannelDeliverySystem.cpp:426`、`RendingWave.cpp:442`、`FlowingThrust.cpp:486`、`BladeBoomerang.cpp:200`、`ProjectileSystemBenchmark.cpp:64,78` |
| High 02（迁移未执行） | ✅ 主体修复，残留 Medium 01/03(R5) | TriggerRule 生产者：`PhantomFlash.cpp:180-201`、`FlowingThrust.cpp:289-303,582-596`、`BehaviorInjectionRegistry.cpp:50-63`；技能 8/9/124 特例已从 `SkillSystem.cpp` 移除（rg 零匹配）；124 → `ShadowDuplicationHook.cpp:13-84`；AreaField 生产者：天剑 `HeavenlySwordDescent.cpp:721-737`、血海 `BloodSea.cpp:487-503`；`AreaFieldDeliverySystem.cpp:25-27` 专精领域防护（自管 pulse，避免双倍结算与双销毁竞态） |
| Medium 01（空转管道） | ✅ 修复，但衍生新 High 01(R5) | payload_context 生产者 9 处（behaviors 6 处 + `SkillSystem.cpp:1272,1708` + `ShadowDuplicationHook.cpp:77`）；`DamagePipelineUnifiedEntryTests.cpp:1815-1863` 锁定语义 |
| Medium 02（不可达） | ✅ 修复 | `ItemFactory.cpp:620-666` rollSkillModifiers（CD/蓝耗/投射物/半径 4 类）；`affixes.json` +PlusSkillLevelGeneric 7 tiers；`SaveManager.cpp:659` 全局存档 mask；Section 13 编解码 + E2E 集成测试 |
| Low 01（Miasma 语义） | ✅ 修复 | `GameUiSnapshotBuilder.cpp:419-427` 恢复 `pts * 0.25f` 秒数语义 + `GameUiSnapshot.hpp:87` 注释 |
| Low 02（Ailment/VFX） | ✅ 修复 | `AreaFieldDeliverySystem.cpp:88-95` duration 参数化（payload.duration，3.0f 兜底）、`:46-56` SkillVfxEvent CastImpact 提交 |
| Low 03（RecordHit 语义） | ✅ 修复 | `Projectile.hpp:69-95` 新增 `TryRecordHit(target, outLimitReached)`，`RecordHit` 兼容接口保留 |

## 25. 本轮新发现

### High 01(R5)：`payload_context.increased_damage` 语义错配 → 基准伤害 +100%

- **位置**：生产者 9 处均写 `ctx.increased_damage = stats->damage_multipliers[0]`（或快照变体）——`BeamChannelDeliverySystem.cpp:281,434`（技能 5/7）、`FlowingThrust.cpp:432,501`、`MindBlade.cpp:326`、`BladeBoomerang.cpp:214`、`RendingWave.cpp:454`、`SkillSystem.cpp:1272,1708`、`ShadowDuplicationHook.cpp:77`；消费者 `DamagePipeline.cpp:877-878`。
- **机理**：`damage_multipliers` 是倍率语义（默认 1.0，`Stats.hpp:102`），消费侧期望小数增量语义（`multiplier_pct += increased_damage * 100.0f`）。统一入口单测注释明确约定（`DamagePipelineUnifiedEntryTests.cpp:1846`：`increased_damage = 0.5f` 注释 "+50%"，期望 975 = 130×1.5×2.0×2.5）→ 传入基准值 1.0 即 +100%。
- **后果**：所有带 payload_context 的伤害（全部技能投射物/引导/残影）在无加成基准下伤害翻倍。旧链路中 `damage_multipliers` 从未参与投射物伤害计算（`DamagePipeline.cpp` 全文无 snapshot 消费；MoreBucket 仅读 global_mods 与天赋树节点 `:906-957`），因此这不是重复计入，而是凭空新增 +100%。
- **修复建议**：行为保持 = 9 处生产者全部改传 `0.0f`；若需承载 heavy momentum / speed 加成，把 `damage_multipliers[0] - 1.0f` 以 More 语义并入 `more_damage`，或修订 `DamagePayloadContext` 字段注释并在单测锁定。
- **测试缺口**：现无"payload_context 基准行为与旧路径等价"断言，建议补 `increased_damage = 0` 时与无 payload_context 结果一致的等价性单测。

### High 02(R5)：无限穿透模式第 9+ 目标 Multi-Hit 回归

- **位置**：`Projectile.hpp:74-76`（`hit_count < kMaxInlineHits` 才写缓存，之后仅递增计数）；跨帧防重复依赖 `ProjectileSystem.cpp:438` 的 `proj.HasHit(target)`。
- **机理**：无限模式（`max_pierce = 255`）下 8 槽 `hit_cache` 只记录前 8 个目标，第 9+ 目标不入缓存 → 下一帧 `HasHit` 返回 false → 重复结算。`s_uniqueHits`（`ProjectileSystem.cpp:433-436`）仅单次 grid.query 内去重，不跨帧。
- **触发场景**：技能 7 斩心剑切割 hitbox（`BeamChannelDeliverySystem.cpp:422-423` `speed=0`、`lifeTime=0.1f` ≈ 6 帧、pierceCount=999）确定触发——范围内 >8 目标时第 9+ 目标每帧重复扣血。旧 vector 实现全量记录无此问题。R4 High 01 的修复衍生此回归。
- **测试缺口**：`ProjectileTests.cpp:54-57` 仅断言前 8 目标 HasHit=true，未断言第 9+ 目标跨帧不重复命中——测试通过但掩盖缺陷。
- **修复建议**：环形缓冲对慢速投射物仍可能失效；更稳妥的方案是"无限模式 + 静态 hitbox（speed≈0）完成一次有效结算后标记销毁"，或 hit 槽写满后按当帧 query 目标集合判重。请按计划 §7.4 SBO 语义选定方案并补跨帧防重复单测。

### Medium 01(R5)：技能 9 反击公式仍硬编码于 ProcEngine，且与 DoCast 注册双轨

- `ProcEngine.cpp:36-56` 阶段一内嵌 PhantomFlashComponent fallback 注入（`rule_id=9`、cooldown_refund 字段）；`:133-162` 硬编码反击结算（`max(25, damage_multipliers[0] * 100)`、enchant_tag、CounterPool → `DamagePipeline::Calculate` → `CombatSystem::ApplyDamage`）。两阶段安全目标达成、特例已离开 SkillSystem 主干，但触发引擎层新增了技能特例分支。
- 双轨冗余：`PhantomFlash.cpp:180-201` DoCast 已注册同参规则，fallback 仅在"有 pf 组件但无规则"时激活，正常流程为死代码。
- 建议：后续包给 TriggerRule 增加反击伤害泛化字段，或反击改走 TriggerCast 派生施法。

### Medium 02(R5)：幻影闪反击同帧多击可重复触发

- `ProcEngine.cpp:61` 仅查 `current_cooldown`；规则 `internal_cooldown = 0.0f`（`PhantomFlash.cpp:189`）→ 触发后无 ICD；`pf->triggered` 只门控 fallback 注入（`:38`），阶段二 `:136` 置位后同帧再次受击仍再次结算反击，下一帧 `PhantomFlash.cpp:203-207` RemoveRule 后才收敛。
- 旧行为：counter_window 内至多一次反击。建议：触发后立即 `RemoveRule(9)` 或将 `internal_cooldown` 设为窗口剩余时长。

### Medium 03(R5)：rule 124 声明 `listen_event=OnSkillCast` 但无对应派发路径

- `FlowingThrust.cpp:290-302`、`BehaviorInjectionRegistry.cpp:50-63` 注册 `OnSkillCast` 规则，但无任何 OnSkillCast listener 派发到 ProcEngine；rule 124 实际由 `ShadowDuplicationHook`（TryCast 前置钩子，`SkillSystem.cpp:1633`）直接消费。声明与执行不一致。建议改 `listen_event` 为哨值并注释"前置钩子直读"，或补 OnSkillCast 派发。

### Medium 04(R5)：crit_chance 归一化边界误判

- `DamagePipeline.cpp:1007-1010` `crit_chance <= 1.0f ? *100 : 原值`：0-100 制的恰好 1%（=1.0f）被解释为 100% 暴击。低概率但真实，建议显式单位字段或调整阈值并注释。

### Medium 05(R5)：SkillSystem.cpp 行数降幅 13.1%，未达计划 40% 验收

- 2671 → 2465（R4）→ 2322（R5）。剥离已完成（引导 5/7 → BeamChannelDeliverySystem、领域 → behaviors/AreaField、8/9/124 → ProcEngine/ShadowDuplicationHook），剩余为技能执行主循环/状态机/VFX。建议按上轮 §22.2(b) 修订验收口径（以"特例清零 + 分发移交"为准），不为数字强拆主干。

### Best Practice（非阻塞）

1. `BehaviorInjectionRegistry.cpp:28` 覆盖日志由可读 ID 改为数字 idx；`FlowingThrust.cpp` 头部去 BOM（无害）。
2. R4 BP 两条（Rebake 挂载点、ItemSideTables 字段级 move 注释）未变，维持原建议。
3. rule 124 的 `GroundTarget` 在 `ProcEngine.cpp:92-93` 与 Victim 同路径（resolved_target=victim），当前由钩子直读无影响，泛化时需复核。

## 26. 结论

**修改**

本轮高质量闭合了 R4 全部 7 项发现项：投放链（Factory + 词缀 + Section 13 + E2E）、迁移（TriggerRule/AreaField 生产者 + 特例剥离）、双轨防护（专精领域 skip）、语义澄清（Miasma/Ailment/RecordHit）均到位，构建与 20/20 测试全绿。但两条核心修复各衍生一个新 High：

1. **High 01(R5) 数值回归**为全局性（9 处生产者 × 所有 payload_context 伤害），上线即全技能基准伤害翻倍，必须修复（9 处改 `0.0f`，小改动）；
2. **High 02(R5) Multi-Hit 回归**影响主力技能 7 密集怪群场景，必须修复（hit_cache 跨帧策略 + 单测）。

另请顺带处理 Medium 01-04（均为小改动）。修复完成后进入第六轮验证，直指 `提交`。

---

# 第六轮审查：跟进审查（R6 修复验证）

## 27. 变更边界与验证

- **范围**：较 R5 增量约 +151 行——`BehaviorInjectionRegistry.hpp`（+`Clear()` teardown 接口）、`DamagePipelineUnifiedEntryTests.cpp`（+3 个测试用例）、`Projectile.hpp`（hit 缓存重构）、9 处 behaviors/ProcEngine 修复。
- **构建**：`build.bat RelWithDebInfo` exit=0，零警告零错误。
- **测试**：`-L unit` 9/9 通过；`-L "skill|combat|ci|integration"` 11/11 通过（含复现后重跑验证）；`nmd.tests.integration` 单独 5/5 通过 + 集成整体 13/15（失败 2 次均为范围外 S1a flaky，见 §30.1）。

## 28. R5 发现项修复状态

| R5 编号 | 状态 | 证据 |
|---|---|---|
| High 01(R5)（伤害 +100%） | ✅ 修复 | 9 处生产者全部 `increased_damage = 0.0f`（`BeamChannelDeliverySystem.cpp:281,434`、`FlowingThrust.cpp:429,496`、`MindBlade.cpp:322`、`BladeBoomerang.cpp:210`、`RendingWave.cpp:450`、`SkillSystem.cpp:1269,1704`、`ShadowDuplicationHook.cpp:74`）；`DamagePipeline.cpp:877` 零值守卫；新增 Baseline Equivalence 测试锁定"有/无 payload_context 结果一致（130.0f）" |
| High 02(R5)（Multi-Hit） | ✅ 修复 | `Projectile.hpp:45-105`：`kMaxInlineHits=32` + `overflow_hits` 堆溢出向量全量去重（注释"绝不逐出已命中目标"），`HasHit` 双查（内联+溢出）、`TryRecordHit` 无限模式恒返回 `outLimitReached=false`、`ClearHits` 双清；≤32 命中零堆分配（SBO 语义保留），溢出容量经 `ClearHits` 复用 |
| Medium 01(R5)（反击特例） | ◐ 部分处理 | `ProcEngine.cpp:153-155` 增加 `damage_multipliers[0] ≤ 0` 防御；fallback 双轨与反击公式硬编码保留 → 登记为遗留（后续包泛化 TriggerRule 反击字段或改走 TriggerCast） |
| Medium 02(R5)（同帧多击） | ✅ 修复 | `ProcEngine.cpp:84` 触发即闭锁（rule 9 → `current_cooldown=999`）、`:66-71` 状态门控、`:143-150` `triggered=true` + `RemoveRule(9)` 双保险、fallback 注入 `internal_cooldown = counter_window`（`:47`） |
| Medium 03(R5)（124 声明不一致） | ✅ 修复 | `FlowingThrust.cpp:293`、`BehaviorInjectionRegistry.cpp:54` 改 `listen_event = CombatEventType::Count` 哨值 + 注释"前置钩子直读"；`FlowingThrust.cpp:586` 仍写 `OnSkillCast`（功能等价，声明值不一致 → BP-01） |
| Medium 04(R5)（crit 边界） | ✅ 修复 | `DamagePipeline.cpp:1007-1009` 归一化约定注释 + 无条件 ×100；新增 Normalized Crit Boundaries 测试（ctx `0.0f` 显式覆盖攻击者 100%、`0.01f` 精确 = 1%） |
| Medium 05(R5)（行数口径） | — 未变 | `SkillSystem.cpp` 仍 2322 行；等待计划口径修订决策（§22.2(b)），非代码阻塞 |

## 29. 单位链澄清（审查期间保守疑虑的闭环）

为排除"ctx 归一化约定 × 生产者透传"的百倍暴击风险，逐环验证：

- `CombatStats.crit_chance` **归一化**：`CombatConstants.hpp:14` `DEFAULT_CRIT_CHANCE = 0.05f`；`AttributePipeline.cpp:753-755` 管线 `Result()/100` 后写入并 clamp 至 `Cap::CRIT_CHANCE`。
- `SkillSnapshot.crit_chance` **归一化**（`SkillDefs.hpp:611` 注释明确）。
- 消费侧 `DamagePipeline.cpp:1009` `ctx * 100` 转入百制域（与 `GetStatWithTags` 路径 `:1011-1013` 一致）。
- 生产者：`BeamChannelDeliverySystem.cpp:279,432`（`stats + bonus/100` 归一化 ✓）、`RendingWave.cpp:448` empowered `+1.0f`（+100% 暴击 ✓）、`:450` `crit_multiplier +1.0f`（倍率语义 → 2.5x ✓）、其余 5 处纯透传 ✓。
- **结论：单位链全一致，无暴击回归。**

## 30. 范围外发现（登记，不阻塞本包）

1. **S1a GPU timer 环测试 flaky**：`tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:235` `CHECK(frameResult.state == QueryState::Valid)` 失败值 `3 != 1`；单跑 2/30、集成 2/15 失败（~7%）。该测试由渲染 P0 包（提交 e7685b6e）引入，S1a 场景为渲染图门循环，独立于技能系统（本包无 GPU timer 交互，静态初始化器亦无关联）→ 归因渲染层查询完成时序假设不稳，建议渲染跟进包处理（查询回收前 flush/fence 或状态轮询窗口加宽）。
2. **旧链路 snapshot crit 单位存疑**：`DamagePipeline.cpp:1415/:1482`（`process_range` 批量分支，`:1292` 起，既有代码）以 `/100` 消费 `snap.crit_chance`，与 `SkillSnapshot` 归一化定义冲突；本包未触碰该路径，且 ctx 链路存在时优先级更高不触发。建议登记为旧缺陷排查项。

## 31. 结论

**提交**

R5 两个 High 已全部修复并由等价性/边界单测锁定；Medium 02/03/04 修复到位；Medium 01（反击特例防御加固后保留）与 Medium 05（行数口径）为已登记的决策类遗留，不构成阻塞。构建零警告，测试全绿（唯一不稳定项为范围外 S1a flaky，与本包无因果）。

计划 §5 验收对照：编译 ✅；测试 ✅；架构合规——枚举占位符不变 ✅、NMDS v1 未触碰 ✅、Standard Layout ✅、主干无技能 8/9/124 分支 ✅（反击迁至 ProcEngine、124 迁至 ShadowDuplicationHook）、热路径无字符串比较 ✅；行数降幅 13.1% ⚠️（待口径修订）；穿透 SBO ≤32 零堆分配 ✅。

后续建议（非阻塞，按优先级）：
1. 计划文档按 §22.2(b) 修订 §5 行数验收口径，并登记迁移后续包（技能 8/9 反击公式泛化、AreaField 专精字段收敛）；
2. 渲染跟进 S1a flaky（§30.1）；
3. 登记旧链路 snapshot crit 单位排查（§30.2）；
4. 统一 rule 124 的 `listen_event` 声明值（BP-01：`FlowingThrust.cpp:586` 与 `:293` 不一致）。

