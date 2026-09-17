# 技能系统最终收尾与全链路闭环设计规范

- **文档状态**：已按源码实测复核修订（v1.2）
- **设计日期**：2026-09-17
- **系统代号**：`SKILL-SYSTEM-FINAL-CLOSURE`
- **输入来源**：
  - 技能系统重构总账：[`docs/plans/2026-09-12-skill1-9-followup-backlog.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-12-skill1-9-followup-backlog.md)
  - UMR 专精收尾与加固规范：[`docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md)
  - 第 5 轮复核报告：[`docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review-round5.md`](file:///d:/PRJ/NoMoreDay/docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review-round5.md)
  - 属性管线与战斗系统源码：[`StatsSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp)、[`AttributePipeline.cpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/stats/AttributePipeline.cpp)、[`SkillSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp)

---

## 1. 背景与问题定义

在完成了剑修 12 技能的专精抽象化（SpecState）、UMR 交付算子（Batch 1~4 全面合入）以及哨兵加固（commit `7d2b4697`）之后，技能改造的主体架构已经稳固建立。
然而，在多轮代码走查与架构复盘中，遗留了若干横跨技能与相邻子系统（尤其是属性系统 `StatsSystem`）的交界性未收口问题：

1. **属性修饰符运行时来源通路静默失效（F-06，最高严重度）**：
   - 根因：[`StatsSystem.cpp:318`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp#L318) 的 `apply_if_tags_match` 用一句启发式判断修饰符是否“已被 AttributePipeline 预烘焙”：
     ```cpp
     bool is_baked = (mod.required_tags == Tag::None);
     ```
     该启发式**只对 `AttributePipeline` 真正折叠过的来源成立**。`AttributePipeline::Calculate` 只折叠 `ModifierList`（`:579`）、`ActiveEffectsComponent`（`:588`）、`AstrolabeComponent`（`:615`），**从不遍历** `SkillModifierComponent`、`GlobalModifierComponent` 与 `ActiveSkillsComponent::specialized_slots`。因此这三类纯运行时来源中所有 `required_tags == Tag::None` 的修饰符都被误判为“已烘焙”并整体跳过。
   - 影响：
     - **32 个**（原稿误记为 42）非空 `stat_modifiers` 专精节点（`:487`）在运行时静默失效。`mastery_skill_trees.json` 共 76 个节点带该键，其中 44 个为空数组（如节点 1015）。
     - [`skills.json`](file:///d:/PRJ/NoMoreDay/assets/data/skills.json) 中 10 条 `SkillModifierComponent` 修饰符（`:352`，id 100/131/202/400/411/430/450/653/800/802，均缺省 `required_tags`）同样失效。
     - `GlobalModifierComponent`（`:358`）的条件词缀若 `required_tags` 命中 `player_tags`（如 `Tag::SwordRiding`）会被误判 `is_baked` 而漏算。

2. **`SkillSystem::GetBakedSkillProfile` 直连消费哨兵隐患（F-02）**：
   - 根因：此前收尾在 `ResolveBakedProfile` 统一增加了 `profile->is_baked` 校验并拦截哨兵档案，但全仓仍有约 28 处非测试生产代码直接调用 `SkillSystem::GetBakedSkillProfile`。
   - 影响：除了交付层（伤害/范围）可能读入默认值外，更致命的是施法主干路径（`SkillSystem::TryCast`、`SkillSystem::UpdateCooldowns`、`SkillSystem::InitHooks` 触发 lambda、`RefundManaCost`）：若遭遇哨兵档案（`is_baked == false`），其 `effective_mana_cost` 与 `effective_cooldown` 均为 0.0f，将直接导致施法免蓝、无冷却或返还蓝量被吞噬为 0 的严重逻辑破坏。

3. **技能 10 节点 1015 文案与数据语义漂移（F-01）**：
   - 根因：节点 1015 数据中原有的错误闪避修饰符已被清除，其真实机制完全由 UMR 交付记录 `2010150`（延长无敌帧时长，`param_f32 = 0.03f`，满级 3 点增加 0.09s，单测基线 0.50s + 0.09s = 0.59s）承担；但配置文案 `desc_key` 仍写着“降低减速和击退影响”，而底层 `StatType` 根本不存在减速/击退抗性枚举。
   - 影响：玩家端文案与真实逻辑脱节。若文案随意写成 0.05s 将与代码形成新的数据漂移，必须严格与 UMR 权威值（0.03s）对齐。

4. **技能系统改造总账未正式结项（Backlog 闭环）**：
   - [`docs/plans/2026-09-12-skill1-9-followup-backlog.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-12-skill1-9-followup-backlog.md) 中部分已被后续 Commit 解决的条目尚未正式完成文档结项注记；同时需将深水区扩展（技能 9 残影斩击、技能 11 多元素合流）明确移交给未来独立玩法特性包，使当前技能专精改造能够干净利落地画上句号。

---

## 2. 目标与非目标

### 2.1 目标
1. **彻底打通属性修饰符运行时通路（P0）**：在 `StatsSystem.cpp` 显式区分“已烘焙源”（`ModifierList`/`Astrolabe`）与“未烘焙运行时源”（`SkillModifierComponent`/`GlobalModifierComponent`/`specialized_slots`），恢复 32 个专精节点与 10 条技能修饰符的正常生效能力，并严格保证 `ScopePolicy::SkillOnly` 与 `ScopePolicy::GlobalAlways` 的作用域隔离。
2. **统一只读档案安全访问守卫（P0）**：在 `SkillSystem` 暴露官方的安全只读访问接口 `GetValidBakedSkillProfile`（仅返回 `is_baked == true` 的档案），并对核心施法、触发、返还与战斗交付系统中的直连点进行全量迁移，彻底杜绝哨兵档案引发的“0 蓝耗 / 0 CD / 默认范围”漏洞。
3. **文案数据零漂移（P1）**：修正节点 1015 的 `desc_key` 为每级 0.03 秒，确保其文案、数据、代码三方完全吻合。
4. **总账正式验收与结项（P1）**：对 `2026-09-12-skill1-9-followup-backlog.md` 逐项核对并盖戳结项，为后续全面推进其他系统扫清所有技术与文档障碍。

### 2.2 非目标
1. **不修改 `AttributePipeline` 基础折叠逻辑**：专精节点是动态依赖技能与词条上下文的，按既有架构设计应在 `GetStatWithTags` 动态计算，不应强行侵入装备/基础属性的预烘焙流水线。
2. **不实施技能 9/11 的深水区玩法机制**：技能 9 节点 930/993（残影斩击）与技能 11（多元素形态）已在设计上定稿，但属于玩法内容扩展包，明确列为未来特性开发，不在本次缺陷收口范围内。
3. **不改变 `GetBakedSkillProfile` 的底层缓存语义**：保留 `GetBakedSkillProfile` 作为返回裸缓存的底层接口（供测试断言哨兵状态及 `RebakeSkillProfiles` 幂等比对使用）。

---

## 3. 详细设计与技术方案

### 3.1 专精节点属性修饰符通路修复（F-06）

#### 3.1.1 机制分析与数据流
在 [`src/game/contracts/impl/StatsSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp) 的 `GetStatWithTags` 中：
1. 预烘焙源（`ModifierList` 与 `AstrolabeComponent`）：在 `AttributePipeline::Calculate` 时，若 `mod.required_tags == Tag::None` 或匹配 `player_tags`，其数值已经被静态计入 `CombatStats` / `StatCalculation`。因此在动态 `GetStatWithTags` 中，**必须跳过** 这些已烘焙的部分，防止双重累加。
2. 专精节点源（`ActiveSkillsComponent::specialized_slots`）：`AttributePipeline` 从未遍历也从未折叠专精节点。专精节点完全依赖 `StatsSystem.cpp:446-495` 实时求值。因此专精节点的修饰符**永远未被预烘焙**（`is_baked` 恒为 `false`）。

#### 3.1.2 方案设计
重构 `apply_if_tags_match`，引入**无默认值**的 `bool source_prebaked` 形参，把“该来源是否已被 AttributePipeline 折叠”从隐式启发式改为显式声明，强制全部 5 个调用点逐一表态：

| 调用点 | 来源 | `source_prebaked` | 依据 |
| --- | --- | --- | --- |
| `:334` | `ModifierList` | `true` | 已被 AttributePipeline 折叠 |
| `:342` | `AstrolabeComponent` | `true` | 已被 AttributePipeline 折叠 |
| `:352` | `SkillModifierComponent` | `false` | pipeline 从不遍历 |
| `:358` | `GlobalModifierComponent` | `false` | pipeline 从不遍历 |
| `:487` | `specialized_slots` | `false` | 专精系统完全动态驱动，从不预烘焙 |

```cpp
auto apply_if_tags_match = [&](const std::vector<StatModifier> &modifiers,
                               bool source_prebaked,
                               float scale = 1.0f) {
  for (const auto &mod : modifiers) {
    bool type_match = (mod.type == type);

    // 伤害属性转换继承
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

    // HasTag(x, Tag::None) 恒为 true；保留显式 None 分支用于表意
    bool tags_match = (mod.required_tags == Tag::None) ||
                      HasTag(combined_query_tags, mod.required_tags);
    if (tags_match) {
      ApplyStatCalculation(dynamic_calc, mod.mode, mod.value * scale);
    }
  }
};
```

五个调用点同步显式传参：
```cpp
apply_if_tags_match(modList->modifiers, true);                                        // :334
apply_if_tags_match(node->modifiers, true, static_cast<float>(points));               // :342
apply_if_tags_match(skillMods->stat_modifiers, false);                                // :352
apply_if_tags_match(global->stat_modifiers, false);                                   // :358
apply_if_tags_match(node_it->second.stat_modifiers, false, static_cast<float>(pts));  // :487
```

#### 3.1.3 作用域与 Keystone 互斥守卫验证
当前代码中已具备健全的外层守卫：
- `:469`: `if (!can_apply_scope(scope, source_skill_id)) continue;`
  - `ScopePolicy::SkillOnly`: 必须满足 `source_skill_id != 0 && source_skill_id == query_skill_id`。
  - `ScopePolicy::GlobalWhileBuffActive`: 需来源技能的 Buff 处于激活态（如 BeamChannel / PhantomTrance）。
  - `ScopePolicy::GlobalAlways`: 无条件放行。
- `:483`: `if (is_keystone_excluded(specialized, source_skill_id, node_id, node_contract)) continue;`
- `:474`: 转质互斥检查。
因此，解除误判 `is_baked` 后，作用域、转质与 Keystone 互斥规则均完全生效且不会产生穿透。

---

### 3.2 只读档案安全访问守卫统一（F-02）

#### 3.2.1 接口扩展
在 [`src/game/systems/skill/SkillSystem.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.hpp) 中新增安全查询方法（添加 `[[nodiscard]]`）：
```cpp
/**
 * @brief 获取已成功烘焙的有效技能档案。
 * @details 若档案未烘焙（哨兵）或槽位未配置，统一返回 nullptr。
 *          纯只读，不触发即时烘焙。适用于帧级热路径系统。
 */
[[nodiscard]] static const BakedSkillProfile* GetValidBakedSkillProfile(
    const entt::registry &registry, entt::entity entity, uint32_t skill_id);
```
在 [`src/game/systems/skill/SkillSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp) 实现：
```cpp
const BakedSkillProfile *SkillSystem::GetValidBakedSkillProfile(const entt::registry &registry,
                                                               entt::entity entity,
                                                               uint32_t skill_id) {
  const auto *profile = GetBakedSkillProfile(registry, entity, skill_id);
  if (profile && profile->is_baked) {
    return profile;
  }
  return nullptr;
}
```

#### 3.2.2 生产调用点全面收敛清单
对系统中的直接调用点进行分流替换：
1. **必须使用 `GetValidBakedSkillProfile`**：
   - **施法与触发主干路径（防 0 蓝耗/0 CD/返还失效）**：
     - `SkillSystem.cpp:1805`（`SkillSystem::UpdateCooldowns`，定义于 `:1789`：读取 `effective_charges` 与 `effective_cooldown`）
     - `SkillSystem.cpp:2004`（`SkillSystem::TryCast`，定义于 `:1963`：读取 `effective_charges`、`effective_mana_cost` 与 `effective_cooldown`）
     - `SkillSystem.cpp:925`（`SkillSystem::InitHooks` 触发 lambda，定义于 `:535`：读取 `effective_mana_cost`）
     - `SevenStarSlashShared.hpp:142`（`RefundManaCost`，定义于 `:130`：读取 `effective_mana_cost`）
   - **战斗伤害与抗性结算**：
     - `DamagePipeline.cpp:337, 613`（伤害与吸血结算）
     - `DamageMitigationService.cpp:131, 163, 318`（减伤与抗性结算）
   - **技能交付与地面场系统**：
     - `ElementPathSystem.cpp:69`（元素路径派生）
     - `AreaFieldDeliverySystem.cpp:447, 468`（地面场离开结算）
     - `BeamChannelDeliverySystem.cpp:75, 161, 948, 1017, 1096, 1116, 1152, 1179`
   - **具体技能行为层**：
     - `BladeBoomerang.cpp:252`、`FlowingThrust.cpp:301`、`InfiniteBlades.cpp:57`
     - `PhantomTrance.cpp:86`（替换旧手写 `profile != nullptr && profile->is_baked`）
   - **渲染与 UI 展示**：
     - `GameplayState.cpp:481, 884`（渲染与指示圈）
     - `GameUiSnapshotBuilder.cpp:846`、`SkillDisplayPreviewService.cpp:25`（UI 耗蓝与 CD 预览）
2. **维持原样（保留 `GetBakedSkillProfile`）**：
   - `SkillSystem::RebakeSkillProfiles`（需要与旧档案/默认构造档案进行幂等比较）
   - `SkillProfileResolve.cpp`（其内部已有严格的 `profile->is_baked` 判断）
   - 单元测试断言哨兵状态（如 `SkillProfileResolveSentinelTests.cpp`）
3. **迁移正确性约束（判空审计）**：新接口将“非空哨兵”归一为 `nullptr`，所有迁移点必须在解引用前判空或走既有静态回退，漏判即崩溃。已核 `DamagePipeline.cpp:337/613`、`SkillSystem.cpp:1805/2004/925`、`SevenStarSlashShared.hpp:142`、`ElementPathSystem.cpp:69`、`AreaFieldDeliverySystem.cpp:447/468`、`FlowingThrust.cpp:301`、`BladeBoomerang.cpp:252`、`InfiniteBlades.cpp:57`、`PhantomTrance.cpp:86`、`GameplayState.cpp:481/884`、`GameUiSnapshotBuilder.cpp:846`、`SkillDisplayPreviewService.cpp:25` 均已有判空/回退。
4. **附带简化**：`BeamChannelDeliverySystem.cpp:161-164` 现有手工哨兵归一（`if (profile != nullptr && !profile->is_baked) profile = nullptr;`）可直接由新接口替代。
5. **既有回退恢复（预期行为变更）**：`DamageMitigationService.cpp:131-137` 与 `:163-169` 在 `profile == nullptr` 时会以同 ID 专精即时烘焙补齐；当前哨兵恰好阻断该回退，迁移后将恢复生效，属本方案的预期修复。

---

### 3.3 文案与数据一致性对齐（F-01）

修改 [`assets/data/mastery_skill_trees.json:563`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json#L563) 中节点 1015 的 `desc_key`（该节点 `stat_modifiers` 为空数组，F-01 纯为文案修正，不属 F-06 影响面）：
- 旧文案：`"延长七星斩无敌帧前后的容错窗口，并降低释放期间 20%/40%/60% 的减速和击退影响。"`
- 新文案：`"延长七星斩起手与收招阶段的无敌帧持续时间，每级延长 0.03 秒容错窗口。"`
严格对齐权威事实源：
1. `assets/data/modifier_v2/skill_spec_modifiers.json:1774` 记录 `2010150`（其 `param_f32 = 0.03f` 位于 `:1799`）；
2. 既有单测 `SkillSpecializationBakerTests.cpp:2354-2361` 与 `SkillWrapupHardeningTests.cpp:334-336`（基线 `0.50 + 3 * 0.03 = 0.59s`）。
同步修改 [`设计文档/职业设计草案_剑修.md`](file:///d:/PRJ/NoMoreDay/设计文档/职业设计草案_剑修.md)（line 930 与 line 971）对应数值说明。
运行自动化同步与校验工具：
```powershell
python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism
python scripts/sync_skill_node_icon_ids.py --check
```

---

### 3.4 遗留总账文档结项与移交（Backlog Sign-off）

在 [`docs/plans/2026-09-12-skill1-9-followup-backlog.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-12-skill1-9-followup-backlog.md) 进行如下归档整理：
1. **A-01**：标记为已按新标准完成（废除 100 行限制，全 12 技能已由 SpecState 与 UMR 单源支撑）。
2. **A-05**：标记为已全部销项（B2 全部 24 项此前已全部落地）。
3. **F-06、F-02、F-01**：标注在本方案中完成彻底修复并附带测试证据。
4. **B1-21~23（实机观测项）**：标注为“已由既有自动化功能与集成测试（`InfiniteBladesNodes.cpp`、`AreaFieldDeliveryTests.cpp`、`MindBladeNodes.cpp`）提供充分逻辑证明，正式销项”。
5. **B1-24/26 与 O-01~07**：注记为移交对应的性能分析与渲染专项 Track 持续跟踪，移出技能专精主线。
6. **F-05（930/993、天剑多元素）**：从“技能债务”移入“未来玩法特性清单”。
7. **状态盖戳**：将文档标题与状态更新为 **已完成结项归档（CLOSED）**。

---

## 4. 验证与测试策略

### 4.1 单元测试（自动化护栏）
新增测试文件 [`tests/unit/SkillSpecializationStatModifierTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationStatModifierTests.cpp)：
1. **用例 1：`SkillOnly` 作用域生效验证**：
   - 构造实体配置技能 1 专精，点亮赋予 `PhysicalDamage` `PercentAdd` 的节点（默认 `required_tags == None`）。
   - 查询 `skill_id = 1` 时，断言增伤修饰符生效并按点数线性放大（`pts * value`）。
   - 可证伪性：在旧代码下由于 `is_baked == true`，此断言必然失败。
2. **用例 2：`SkillOnly` 跨技能严格隔离验证**：
   - 同上实体，查询 `skill_id = 2` 时，断言修饰符不生效（增伤为 0）。
3. **用例 3：`GlobalAlways` 作用域全局生效验证**：
   - 测试带 `ScopePolicy::GlobalAlways` 的专精节点，无论查询何种 `skill_id` 或 `skill_id = 0`，均能正确获得属性增益。
4. **用例 4：`GlobalWhileBuffActive` 缓存时序验证**：
   - Buff 激活时增益生效；Buff 结束置 `StatsDirty` 并刷新缓存后，增益严格消失。
5. **用例 5：Keystone 互斥阻断负向测试**：
   - 构造同一互斥组的双 Keystone，断言被排除的劣后 Keystone 即使分配了点数，其 `stat_modifiers` 亦严格不生效。
6. **用例 6：Transmuter 未激活阻断负向测试**：
   - 未处于激活状态的转质节点，其 `stat_modifiers` 不得渗入属性查询。
7. **用例 7：`SkillModifierComponent` 来源回归**：
   - 构造 `required_tags == Tag::None` 的技能修饰符，断言经 `GetStatWithTags` 生效（旧代码因 `is_baked` 误判必然失败）。
8. **用例 8：`GlobalModifierComponent` 条件词缀回归**：
   - 构造 `required_tags == Tag::SwordRiding` 且玩家处于御剑架势的词缀，断言生效（旧代码因 `is_baked` 误判必然失败）。
9. **用例 9：`GetValidBakedSkillProfile` 守卫与施法回退测试**：
   - 未烘焙/哨兵档案断言返回 `nullptr`（哨兵构造方式参考既有 `SkillProfileResolveSentinelTests.cpp`）；已烘焙档案断言返回有效指针且 `is_baked == true`。
   - 针对持有哨兵档案的技能槽位调用 `SkillSystem::TryCast` 与 `SkillSystem::UpdateCooldowns`，断言正确回退至技能静态基础蓝耗与冷却，杜绝 0 蓝耗免蓝施法。
10. **用例 10：资产不变量测试**：
    - 扫描 `mastery_skill_trees.json` 与 `modifier_v2/skill_spec_modifiers.json`，断言不存在同一节点同时携带非空 `stat_modifiers` 与 UMR 交付算子，防止属性双重累加。
11. **测试注册**：测试文件由 [`tests/CMakeLists.txt:4`](file:///d:/PRJ/NoMoreDay/tests/CMakeLists.txt#L4) 的 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` 自动纳入，新增文件后须重新执行 CMake configure。

### 4.2 离线全量门禁与构建验证
1. `build.bat RelWithDebInfo`：编译 0 错误、0 警告。
2. `scripts/validate_skill_spec_modifiers.py --check`：6/6 门禁全过。
3. `scripts/gen_skill_mechanics_schema.py --check`：0 drift。
4. `ctest --test-dir build -C RelWithDebInfo -L "unit|integration|skill"`：全量通过。
5. 工作区 `git status` 保证零意外改动与无污染。

---

## 5. 风险与回退路径

- **数值激增风险**：修复 F-06 后，此前因静默失效而“休眠”的 32 个专精节点、10 条 `SkillModifierComponent` 修饰符，以及 `GlobalModifierComponent` 中命中 `SwordRiding` 的条件词缀将立即生效，技能伤害/攻速会有所上升。
  - **评估**：这些均属 GDD 规划内的成长收益，恢复生效属于 Bug 修复而非数值通胀；若后续有数值失衡，可通过调整 `mastery_skill_trees.json` / `skills.json` 的系数解决，不属于代码逻辑问题。
- **判空风险**：`GetValidBakedSkillProfile` 将哨兵归一为 `nullptr`，任一迁移点漏判空即崩溃；以 §3.2.2 的逐点审计与全量测试兜底。
- **回退策略**：改动集中在 `StatsSystem.cpp`、`SkillSystem.cpp/.hpp`、数据文案与测试文件，可按阶段单独 commit 干净撤销。
