# 技能系统最终收尾与全链路闭环实施计划

- **设计输入**：[`docs/designs/2026-09-17-skill-system-final-closure-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-skill-system-final-closure-design.md) (v1.2)
- **计划状态**：已按源码实测复核修订（v1.2）
- **实施日期**：2026-09-17
- **系统代号**：`SKILL-SYSTEM-FINAL-CLOSURE`

---

## 1. 实施思路与原理

### 1.1 属性修饰符运行时来源通路修复（F-06）

- **现状与病根**：
  [`StatsSystem.cpp:301`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp#L301) 的 `apply_if_tags_match` 用一句启发式判断某个修饰符是否"已被 `AttributePipeline` 预烘焙"：
  ```cpp
  bool is_baked = (mod.required_tags == Tag::None);
  ```
  该启发式**只对 `AttributePipeline` 真正折叠过的来源成立**。而 `AttributePipeline::Calculate` 只折叠三类来源：`ModifierList`（[`AttributePipeline.cpp:579`](file:///d:/PRJ/NoMoreDay/src/game/foundation/stats/AttributePipeline.cpp#L579)）、`ActiveEffectsComponent`（`:588`）、`AstrolabeComponent`（`:615`）。它**从不遍历** `SkillModifierComponent`、`GlobalModifierComponent` 与 `ActiveSkillsComponent::specialized_slots`。
  于是这些"纯运行时来源"中所有 `required_tags == Tag::None` 的修饰符，都会被恒真误判为 `is_baked = true` 并直接跳过。

- **实测影响面（已用脚本统计）**：
  1. **32 个专精节点静默失效**：[`mastery_skill_trees.json`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json) 共 76 个节点带 `stat_modifiers` 键，其中非空仅 **32** 个（id：1000,1002,1003,1006,1016,1017,1018,1020,1021,1023,1024,1100,1102,1103,1108,1109,1111,1112,1116,1117,1202,1204,1208,1209,1210,1212,1215,1216,1217,1221,1222,1223；其余 44 个为空数组，例如节点 1015）。这些节点在 [`StatsSystem.cpp:487`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp#L487) 经 `apply_if_tags_match` 求值，全部被跳过。
  2. **`SkillModifierComponent.stat_modifiers` 同样失效**（[`StatsSystem.cpp:352`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp#L352)）。该组件从未被 `AttributePipeline` 折叠；[`skills.json`](file:///d:/PRJ/NoMoreDay/assets/data/skills.json) 中现有 10 个 `stat_modifier` 条目（id 100/131/202/400/411/430/450/653/800/802）均缺省 `required_tags`，本应生效却全部被跳过。
  3. **`GlobalModifierComponent.stat_modifiers` 存在条件词缀漏算**（[`StatsSystem.cpp:358`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp#L358)）。该来源也从未被折叠；当前之所以大体正常，仅因它只装载 `required_tags != Tag::None` 的词缀而"恰好"未被 `is_baked` 命中。但只要某条条件词缀的 `required_tags` 命中 `player_tags`（例如御剑架势 `Tag::SwordRiding`），`is_baked` 即被置真而被错误跳过。

- **治理原理**：
  把"是否需要检查 pipeline 烘焙"从**隐式启发式**改为**显式来源声明**。`apply_if_tags_match` 新增第二个形参 `bool source_prebaked`，**不设默认值**，强制 `StatsSystem.cpp` 内全部 5 个调用点逐一表态：

  | 调用点 | 来源 | `source_prebaked` | 依据 |
  | --- | --- | --- | --- |
  | `:334` | `ModifierList` | `true` | 由 `AttributePipeline` 折叠，动态查询须跳过已计入部分 |
  | `:342` | `AstrolabeComponent` | `true` | 同上 |
  | `:352` | `SkillModifierComponent` | `false` | pipeline 从不遍历 |
  | `:358` | `GlobalModifierComponent` | `false` | pipeline 从不遍历 |
  | `:487` | `specialized_slots` | `false` | `AttributePipeline` 从不为专精节点折叠，专精系统完全动态驱动 |

  去除默认值可避免"漏传 → 继续沿用旧 bug"的静默回归；`source_prebaked == false` 时 `is_baked` 恒为 `false`，随后仍按 `combined_query_tags` 做标签匹配，语义不变。
  外层既有的 `can_apply_scope`（`:384`，`SkillOnly` / `GlobalAlways` / `GlobalWhileBuffActive` 三态）、`is_keystone_excluded`（`:408`）、转质互斥（`:472`）守卫保持不变，`SkillOnly` 仍要求 `source_skill_id == query_skill_id`，作用域不会穿透。

### 1.2 档案只读查询安全统一（F-02）

- **现状与病根**：
  此前虽在 `ResolveBakedProfile` 增加了 `profile->is_baked` 守卫（[`SkillProfileResolve.cpp:9`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillProfileResolve.cpp#L9)），但战斗热路径与施法主干仍有 ~28 处直接调用 [`SkillSystem::GetBakedSkillProfile`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2766)。该接口直接返回槽位裸缓存指针（哨兵档案同样返回），哨兵档案的 `effective_mana_cost` / `effective_cooldown` / `effective_charges` 均为 0，会导致免蓝施法、无冷却、返还蓝量归零或默认范围渗入结算。
- **治理原理**：
  在 `SkillSystem` 增加 `[[nodiscard]] GetValidBakedSkillProfile(registry, entity, skill_id)`：仅当档案存在且 `is_baked == true` 时返回指针，否则返回 `nullptr`；**纯只读，不触发即时烘焙**（呼应 `BeamChannelDeliverySystem.cpp:157-164` 逐帧路径刻意避开 `ResolveBakedProfile` 的既有约束）。
  全量迁移只读直连点后，哨兵档案会被统一归一为 `nullptr`，各调用点既有的判空/静态回退分支随即生效，行为等价或更优。

### 1.3 数据与文案零漂移（F-01）

- **治理原理**：
  节点 1015 的 `stat_modifiers` 为空数组，其机制完全由 UMR 交付记录 `2010150`（[`skill_spec_modifiers.json:1774`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/skill_spec_modifiers.json#L1774)，`param_f32 = 0.03`）承担：满级 3 点增加 0.09s，基线 0.50s + 0.09s = 0.59s。仅修改其 `desc_key` 文案（[`mastery_skill_trees.json:563`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json#L563)），对齐 0.03 秒/级，消除与底层不存在的"减速/击退抗性"属性之间的漂移，并同步设计草案。

### 1.4 技能改造历史总账结项

- **治理原理**：
  核对 [`2026-09-12-skill1-9-followup-backlog.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-12-skill1-9-followup-backlog.md) 全部条目，核销 A-01/A-05 与本轮 F-01/F-02/F-06；B1-21~23 以既有自动化单测销项；性能 B1-24/26 与渲染 O-01~07 移交专项 Track；F-05（930/993、天剑多元素）移入未来玩法特性清单；最终盖戳 `CLOSED`。

---

## 2. 伪代码引导

### 2.1 `StatsSystem.cpp` 标签修饰符求值（与真实实现对齐）

```cpp
// StatsSystem.cpp — 在 GetStatWithTags 内
// source_prebaked: 该来源是否已被 AttributePipeline 折叠进基础属性；无默认值，必须显式声明。
auto apply_if_tags_match = [&](const std::vector<StatModifier> &modifiers,
                               bool source_prebaked,
                               float scale = 1.0f) {
  for (const auto &mod : modifiers) {
    bool type_match = (mod.type == type);

    // 伤害属性转换继承（保持既有语义，勿改动）
    if (!type_match && IsDamageStat(type) && IsDamageStat(mod.type)) {
      Tag mod_tag = GetTagFromDamageStat(mod.type);
      if (mod_tag != Tag::None && HasTag(combined_query_tags, mod_tag)) {
        type_match = true;
      }
    }
    if (!type_match) {
      continue;
    }

    bool is_baked = false;
    if (source_prebaked) {
      is_baked = (mod.required_tags == Tag::None);
      if (!is_baked && player_tags != Tag::None) {
        is_baked = HasTag(player_tags, mod.required_tags);
      }
    }
    if (is_baked) {
      continue;
    }

    // HasTag(x, Tag::None) 恒为 true，保留显式 None 分支用于表意。
    const bool tags_match = (mod.required_tags == Tag::None) ||
                            HasTag(combined_query_tags, mod.required_tags);
    if (tags_match) {
      ApplyStatCalculation(dynamic_calc, mod.mode, mod.value * scale);
    }
  }
};
```

五个调用点同步改为显式来源声明：

```cpp
apply_if_tags_match(modList->modifiers, true);                                   // :334
apply_if_tags_match(node->modifiers, true, static_cast<float>(points));          // :342
apply_if_tags_match(skillMods->stat_modifiers, false);                           // :352
apply_if_tags_match(global->stat_modifiers, false);                              // :358
apply_if_tags_match(node_it->second.stat_modifiers, false, static_cast<float>(pts)); // :487
```

### 2.2 `SkillSystem.hpp` / `SkillSystem.cpp` 安全只读查询接口

```cpp
// SkillSystem.hpp
/**
 * @brief 获取已成功烘焙的有效技能档案（哨兵 / 未配置一律返回 nullptr）。
 * @details 纯只读，不触发即时烘焙；适用于帧级热路径与施法主干。
 */
[[nodiscard]] static const BakedSkillProfile* GetValidBakedSkillProfile(
    const entt::registry &registry, entt::entity entity, uint32_t skill_id);

// SkillSystem.cpp
const BakedSkillProfile *SkillSystem::GetValidBakedSkillProfile(const entt::registry &registry,
                                                               entt::entity entity,
                                                               uint32_t skill_id) {
  const auto *profile = GetBakedSkillProfile(registry, entity, skill_id);
  return (profile != nullptr && profile->is_baked) ? profile : nullptr;
}
```

---

## 3. 原子任务拆分

### 阶段一：属性修饰符运行时来源通路修复（F-06）
- [ ] **Task 1.1**：在 [`StatsSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp) 中将 `apply_if_tags_match` 改为 `(modifiers, bool source_prebaked, float scale = 1.0f)`，去掉默认值式启发式，并按 §2.1 表格为 **全部 5 个调用点**显式传参（`:334/:342` 传 `true`；`:352/:358/:487` 传 `false`）。
- [ ] **Task 1.2**：新增单元测试 [`tests/unit/SkillSpecializationStatModifierTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationStatModifierTests.cpp)（由 [`tests/CMakeLists.txt:4`](file:///d:/PRJ/NoMoreDay/tests/CMakeLists.txt#L4) 的 `GLOB_RECURSE ... CONFIGURE_DEPENDS` 自动纳入，**新增后须重跑 CMake configure**），覆盖：
  - `ScopePolicy::SkillOnly` 专精节点在对应技能下生效应证，且按点数线性缩放（`scale = pts`）；
  - `ScopePolicy::SkillOnly` 异技能隔离（查询其他技能/`skill_id=0` 无加成）；
  - `ScopePolicy::GlobalAlways` 任意技能上下文生效应证；
  - `ScopePolicy::GlobalWhileBuffActive`：Buff 激活时生效，Buff 结束（`StatsDirty` 置位、缓存刷新）后失效；
  - `required_tags` 条件专精修饰符（非 None）匹配/不匹配两侧；
  - **Keystone 互斥阻断**：互斥组劣后节点即使分配点数，属性严格不生效；
  - **Transmuter 未激活阻断**：未激活转质节点属性不渗入；
  - **`SkillModifierComponent` 回归**：构造 `required_tags == Tag::None` 的技能修饰符，断言生效（旧代码必然失败）；
  - **`GlobalModifierComponent` 回归**：构造 `required_tags == Tag::SwordRiding` 且玩家处于御剑架势的条件词缀，断言生效（旧代码必然失败）。
- [ ] **Task 1.3**：新增资产不变量测试：扫描 `mastery_skill_trees.json` 与 `modifier_v2/skill_spec_modifiers.json`，断言不存在同一节点同时携带非空 `stat_modifiers` 与 UMR 交付算子（当前实测重叠数 = 0），作为防止属性双重累加的长期护栏。

### 阶段二：档案安全只读查询统一（F-02）
- [ ] **Task 2.1**：在 [`SkillSystem.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.hpp) / [`SkillSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp) 增加带 `[[nodiscard]]` 的 `GetValidBakedSkillProfile`（§2.2）。
- [ ] **Task 2.2**：全量迁移只读直连点至 `GetValidBakedSkillProfile`（按实际符号）：
  - **施法与触发主干**：
    - `SkillSystem::UpdateCooldowns`（[`SkillSystem.cpp:1805`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L1805)）
    - `SkillSystem::TryCast`（[`SkillSystem.cpp:2004`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2004)）
    - `SkillSystem::InitHooks` 触发 lambda（[`SkillSystem.cpp:925`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L925)）
    - `RefundManaCost`（[`SevenStarSlashShared.hpp:142`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SevenStarSlashShared.hpp#L142)）
  - **战斗与结算系统**：`DamagePipeline.cpp:337, 613`；`DamageMitigationService.cpp:131, 163, 318`
  - **交付与地面场系统**：`ElementPathSystem.cpp:69`；`AreaFieldDeliverySystem.cpp:447, 468`；`BeamChannelDeliverySystem.cpp:75, 161, 948, 1017, 1096, 1116, 1152, 1179`
  - **技能行为层**：`FlowingThrust.cpp:301`、`BladeBoomerang.cpp:252`、`InfiniteBlades.cpp:57`、`PhantomTrance.cpp:86`（移除其手写 `profile->is_baked` 判断）
  - **渲染与 UI**：`GameplayState.cpp:481, 884`；`GameUiSnapshotBuilder.cpp:846`；`SkillDisplayPreviewService.cpp:25`
  - **仅简化**：`BeamChannelDeliverySystem.cpp:161-164` 删除手工哨兵归一，直接改用新接口。
  - **维持原样**（不迁移）：`SkillSystem::RebakeSkillProfiles`（需与旧档案/默认构造档案做幂等比对）；`SkillProfileResolve.cpp:17`（内部已判 `is_baked`）；哨兵状态单测（如 `SkillProfileResolveSentinelTests.cpp`）。
- [ ] **Task 2.3**：逐点判空审计（**必做交付项**）。新接口把"非空哨兵"变为 `nullptr`，迁移后每个调用点必须在解引用前判空或走既有静态回退。已初查以下路径均已有判空/回退，迁移后语义等价或更优，实施时逐条复核并在 review 记录：
  - `DamagePipeline.cpp:337`（静态 CD 回退）、`:613`（if 守卫）；
  - `DamageMitigationService.cpp:131-137`、`:163-169`（`!profile` 时本地即时烘焙回退——哨兵当前恰好阻断了该回退，改后恢复，属预期修复）；`:318`（if 守卫）；
  - `SkillSystem.cpp:925`（else 装备降耗）、`:1806/:1833`（charges 判空 + CD 三元）、`:2048/:2116`（`bakedProfile != nullptr` + 静态回退）；
  - `SevenStarSlashShared.hpp:142`（`refund <= 0` 直接返回）；
  - `ElementPathSystem.cpp:69-72`（null → 0）、`AreaFieldDeliverySystem.cpp:447-450/468-474`（`(p && ...)` 常量回退）；
  - 行为层与渲染/UI 各点均为 `profile ?` / `profile &&` / if 守卫。
- [ ] **Task 2.4**：在 `SkillSpecializationStatModifierTests.cpp` 补充哨兵用例：
  - 未烘焙/哨兵槽位 → `GetValidBakedSkillProfile` 返回 `nullptr`；已烘焙 → 返回有效指针且 `is_baked == true`（可参考既有 `SkillProfileResolveSentinelTests.cpp` 的哨兵构造方式）；
  - 持有哨兵档案时调用 `SkillSystem::TryCast` 与 `UpdateCooldowns`，断言回退到技能静态基础蓝耗/冷却，**绝不出现 0 蓝耗或 0 冷却**。

### 阶段三：数据文案对齐与契约生成（F-01）
- [ ] **Task 3.1**：更新 [`mastery_skill_trees.json:563`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json#L563) 节点 1015 的 `desc_key` 为：
  `"延长七星斩起手与收招阶段的无敌帧持续时间，每级延长 0.03 秒容错窗口。"`
  同步更新 [`设计文档/职业设计草案_剑修.md`](file:///d:/PRJ/NoMoreDay/设计文档/职业设计草案_剑修.md) line 930 与 line 971 为 0.03 秒。
- [ ] **Task 3.2**：执行数据检查与同步脚本，全部以 exit code 0 通过：
  ```powershell
  python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism
  python scripts/sync_skill_node_icon_ids.py --check
  python scripts/validate_skill_spec_modifiers.py --check
  python scripts/gen_skill_mechanics_schema.py --check
  ```

### 阶段四：总账结项与验证交付
- [ ] **Task 4.1**：更新 [`2026-09-12-skill1-9-followup-backlog.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-12-skill1-9-followup-backlog.md)：核销 A-01、A-05、F-01、F-02、F-06（附本轮测试证据）；B1-21~23 注记由既有自动化单测销项；B1-24/26 与 O-01~07 移交专项 Track；F-05 移入未来玩法特性清单；整体盖戳 `CLOSED`。
- [ ] **Task 4.2**：执行完整构建与测试矩阵：
  - `build.bat RelWithDebInfo`（0 错误、0 新增警告）
  - `ctest --test-dir build -C RelWithDebInfo -L unit`
  - `ctest --test-dir build -C RelWithDebInfo -L integration`
  - `ctest --test-dir build -C RelWithDebInfo -R nmd.tests.ci.nonperf`

---

## 4. 测试方法

- **单元测试层级（Unit）**：
  - 核心用例集合：`tests/unit/SkillSpecializationStatModifierTests.cpp`（正向加成、跨技能隔离、`GlobalAlways`、`GlobalWhileBuffActive` 缓存时序、Keystone/Transmuter 负向阻断、`SkillModifierComponent`/`GlobalModifierComponent` 来源回归、哨兵拒绝与施法回退、资产不变量）。
  - 验证命令：`ctest --test-dir build -C RelWithDebInfo -R SkillSpecializationStatModifier`。
- **集成与功能测试层级（Integration & Functional）**：
  - 技能专精全套测试：`ctest --test-dir build -C RelWithDebInfo -L "skill|unit|integration"`。
- **非功能与代码卫生（Hygiene & Determinism）**：
  - 四项 Python 数据门禁脚本幂等性与一致性检验。
  - 工作区零污染检验：`git status` 无意外改动。

---

## 5. 验证完成标准（DoD）

1. **功能标准**：
   - `mastery_skill_trees.json` 中全部 **32** 个非空 `stat_modifiers` 专精节点，在配置点数并使用对应技能时经 `GetStatWithTags` 正确读出加成；`skills.json` 的 10 个 `SkillModifierComponent` 条目同步生效。
   - `ScopePolicy::SkillOnly` 节点对其他技能调用 `GetStatWithTags` 返回无加成基础值；`GlobalAlways` 全局生效；`GlobalWhileBuffActive` 随 Buff 生命周期正确起止。
   - 互斥 Keystone 与未激活 Transmuter 的属性修饰符 100% 阻断。
   - `GetValidBakedSkillProfile` 对未烘焙/哨兵档案严格返回 `nullptr`；`SkillSystem::TryCast` / `UpdateCooldowns` 遭遇哨兵槽位绝不出现 0 蓝耗或 0 冷却施法。
   - 资产不变量成立：无节点同时携带非空 `stat_modifiers` 与 UMR 算子。
2. **构建标准**：`build.bat RelWithDebInfo` 返回 0，无新增编译错误与警告。
3. **门禁标准**：四项 Python 数据门禁脚本以 exit code 0 通过；单元测试与集成测试全绿。
4. **文档标准**：`2026-09-12-skill1-9-followup-backlog.md` 标注为结项关闭。

---

## 6. 风险与回退

- **数值激增风险**：修复 F-06 后，此前静默失效的 **32** 个专精节点与 10 条 `SkillModifierComponent` 修饰符将立即生效，技能伤害/攻速等会上升；`GlobalModifierComponent` 中命中 `SwordRiding` 的条件词缀也会恢复计算。
  - **评估**：这些均为 GDD 规划内的成长收益，恢复属 Bug 修复而非数值通胀。若后续出现失衡，通过调整 `mastery_skill_trees.json` / `skills.json` 系数解决，不属代码逻辑问题。以阶段一新增回归用例锁定边界，避免超量计算。
- **迁移判空风险**：`GetValidBakedSkillProfile` 将哨兵归一为 `nullptr`，任一调用点漏判空即崩溃。以 Task 2.3 逐点审计 + Task 4.2 全量测试兜底。
- **回退策略**：改动集中在 `StatsSystem.cpp`、`SkillSystem.cpp/.hpp`、`skill_spec_modifiers`/`mastery_skill_trees` 数据文案与测试文件，可按阶段单独 commit 干净撤销。
