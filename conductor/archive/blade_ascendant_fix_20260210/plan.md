# 剑修技能行为修复与补全 - 实现计划

| 属性 | 值 |
|-----|---|
| **Track ID** | `blade_ascendant_fix_20260210` |
| **优先级** | **P0 URGENT** |
| **状态** | ✅ 已完成（Phase 1-5 全部完成，代码与游戏内回归均通过） |
| **规格书** | [spec.md](./spec.md) |
| **审查报告** | [blade_ascendant_review_20260210.md](../../reviews/blade_ascendant_review_20260210.md) |
| **预计总工时** | ~18-22h (3-4 工作日) |

---

## 任务清单

### Phase 1: ID 常量化基础设施 (Infrastructure) — ~5h

> **目标**: 消灭所有 Behavior 文件中的魔法数字，为后续修复建立安全基础。
> **参考模式**: `FlowingThrust.cpp` 的 `FlowingThrustNodes` 命名空间。

- [x] **1.1** 为 `BladeFormation.cpp` 创建 `BladeFormationNodes` 命名空间
  - 来源: `skills.json` Skill 3 (id: 300-373)
  - 所有 `constexpr uint32_t` 必须的中英文注释
  - **同步校对**: 逐行对比 JSON talent_tree 与代码引用
  - 📁 `behaviors/BladeFormation.cpp`
  - ⏱️ ~45min

- [x] **1.2** 为 `RendingWave.cpp` 创建 `RendingWaveNodes` 命名空间
  - 来源: `skills.json` Skill 2 (id: 200-272)
  - 📁 `behaviors/RendingWave.cpp`
  - ⏱️ ~30min

- [x] **1.3** 为 `InfiniteBlades.cpp` 创建 `InfiniteBladesNodes` 命名空间
  - 来源: `skills.json` Skill 5 (id: 500-573)
  - 📁 `behaviors/InfiniteBlades.cpp`
  - ⏱️ ~30min

- [x] **1.4** 为 `BladeWard.cpp` 创建 `BladeWardNodes` 命名空间
  - 来源: `skills.json` Skill 4 (id: 400-473)
  - 📁 `behaviors/BladeWard.cpp`
  - ⏱️ ~30min

- [x] **1.5** 为 `PhantomFlash.cpp` 创建 `PhantomFlashNodes` 命名空间
  - 来源: `skills.json` 对应 Skill ID
  - 📁 `behaviors/PhantomFlash.cpp`
  - ⏱️ ~30min

- [x] **1.6** 为 `SwordArray.cpp` 创建 `SwordArrayNodes` 命名空间
  - 来源: `skills.json` 对应 Skill ID
  - 📁 `behaviors/SwordArray.cpp`
  - ⏱️ ~30min

- [x] **1.7** 为 `MindBlade.cpp` 创建 `MindBladeNodes` 命名空间
  - 来源: `skills.json` 对应 Skill ID
  - 📁 `behaviors/MindBlade.cpp`
  - ⏱️ ~30min

- [x] **1.8** 为 `BladeBoomerang.cpp` 创建 `BladeBoomerangNodes` 命名空间
  - 来源: `skills.json` 对应 Skill ID
  - 📁 `behaviors/BladeBoomerang.cpp`
  - ⏱️ ~30min

- [x] **1.9** 全量编译验证
  - 运行 `.\build.bat`，确保零新增 Warning
  - ⏱️ ~15min

**Phase 1 检查点**: `git diff --stat` 确认仅修改 behavior 文件，无逻辑变更，仅重命名常量。

---

### Phase 2: P0 ID 逻辑错位修正 (Critical Fix) — ~4h

> **目标**: 修正 BladeFormation 和 InfiniteBlades 中确诊的 ID 错位，使天赋效果与 JSON 定义一致。
> **依赖**: Phase 1 完成（ID 已常量化）

- [x] **2.1** 修正 `BladeFormation.cpp` ID 311 映射
  - **现状**: `311 % 100` → `shockwave_on_crit = true` (震波效果)
  - **JSON 311**: "无尽剑匣" = 灵剑上限翻倍, 单发伤害 -40%
  - **操作**: 
    1. 将 `shockwave_on_crit` 逻辑替换为 `formation.max_swords *= 2; formation.damage_penalty = 0.6f;`
    2. 在 `BladeFormationComponent` 中添加 `damage_penalty` 字段（若不存在）
    3. 确保伤害计算管线应用此惩罚
  - **原震波功能处理**: 移除。若未来需要，应在 JSON 中新增节点承载。
  - 📁 `behaviors/BladeFormation.cpp`, `components/Common.hpp` 或 `SkillDefs.hpp`
  - ⏱️ ~1h

- [x] **2.2** 修正 `BladeFormation.cpp` ID 321→351 映射
  - **现状**: `321 % 100` → `mana_on_hit = true`
  - **JSON 351**: "气劲回流" = 命中 10%...50% 几率回 2 法力
  - **操作**: 
    1. 将 `321` 替换为 `BladeFormationNodes::QiReflow` (351)
    2. `active_nodes.test(351 % 100)` → `test(51)` ← 确认 bitset 索引正确
  - 📁 `behaviors/BladeFormation.cpp`
  - ⏱️ ~30min

- [x] **2.3** 清理 `InfiniteBlades.cpp` 冗余逻辑
  - **现状**: `551 % 100` 被检查了两次（第36行和第48行），第二次检查体是空 TODO
  - **操作**: 
    1. 移除重复的空检查块（第48-50行附近）
    2. 用 `InfiniteBladesNodes::IntentBurst` (551) 替换魔法数字
    3. 清理 520 相关注释
  - 📁 `behaviors/InfiniteBlades.cpp`
  - ⏱️ ~30min

- [x] **2.4** 审计 `BladeFormation.cpp` DoHit 中的 ID
  - **现状**: DoHit 中使用 `formation->shockwave_on_crit` 和 `formation->mana_on_hit`
  - **操作**: 确保 DoHit 逻辑与 DoCast 修正后的语义一致
    1. 震波逻辑已在 2.1 中移除，DoHit 中对应分支也需移除或重新绑定
    2. `mana_on_hit` 保留，但确保是由 351 节点驱动
  - 📁 `behaviors/BladeFormation.cpp`
  - ⏱️ ~30min

- [x] **2.5** 编译 + 手动冒烟测试
  - 运行 `.\build.bat`
  - 启动游戏，装备 Skill 3 灵剑决，分别点亮 311(无尽剑匣) 和 351(气劲回流)，验证效果
  - 进度：已在游戏内验证通过（你已确认）
  - ⏱️ ~30min

**Phase 2 检查点**: 灵剑决天赋树效果与 JSON 描述 100% 匹配。

---

### Phase 3: 元素系统补全 (Elemental Completion) — ~6h

> **目标**: 建立通用元素转换基础设施，补全 Skill 1/2/3 的元素分支。
> **依赖**: Phase 1 完成

- [x] **3.1** 在 `SkillBehaviorBase.hpp` 中添加 `ElementalConversion` 结构体和 `ResolveElementalConversion()` 静态方法
  - 参见 spec.md §3.2 的代码定义
  - 约定: `max_points` 对应的 `1=Fire, 2=Ice, 3=Lightning`
  - 📁 `behaviors/SkillBehaviorBase.hpp`
  - ⏱️ ~45min

- [x] **3.2** 补全 `FlowingThrust.cpp` [170] 元素幻化
  - **现状**: 仅实现 Frost
  - **操作**: 使用 `ResolveElementalConversion()` 替换硬编码的 Frost 逻辑
  - 📁 `behaviors/FlowingThrust.cpp`
  - ⏱️ ~1h

- [x] **3.3** 实现 `RendingWave.cpp` [270] 元素形态
  - **现状**: 完全缺失，代码中无 270 相关逻辑
  - **操作**: 
    1. 在天赋分支逻辑中添加 `RendingWaveNodes::ElementForm` (270) 检查
    2. 使用 `ResolveElementalConversion()` 获取元素参数
    3. 在投射物创建时应用颜色和 `SkillModifierComponent`
  - **注意**: 270 和 250(虚空转化) 互斥（不能同时生效），需添加互斥检查
  - 📁 `behaviors/RendingWave.cpp`
  - ⏱️ ~1.5h

- [x] **3.4** 实现 `BladeFormation.cpp` [370] 元素附魔
  - **现状**: 完全缺失
  - **操作**: 
    1. 在 DoCast 天赋检查中添加 `BladeFormationNodes::ElementEnchant` (370)
    2. 将元素转化应用到灵剑的 `ColorComponent` 和攻击 Tag
  - 📁 `behaviors/BladeFormation.cpp`
  - ⏱️ ~1.5h

- [x] **3.5** 编译 + 视觉验证
  - 分别为 3 个技能点亮元素节点，检查弹道/灵剑颜色是否正确变化
  - 进度：已完成（修复灵剑发射投射物颜色继承后，游戏内视觉验证通过）
  - ⏱️ ~30min

**Phase 3 检查点**: 3 个核心技能的元素分支 Fire/Ice/Lightning 均可点亮并产生对应视觉和伤害类型。

---

### Phase 4: 缺失机制修复 (Missing Mechanics) — ~4h

> **目标**: 实现审查中发现的三个未完成机制。
> **依赖**: Phase 1 完成

- [x] **4.1** 实现 FlowingThrust [113] 风行者 - 碰撞穿透
  - **前置调查**: 检查 `PhaseTag` 或等效 Tag 是否已存在于物理系统
  - 若不存在: 在 `TilemapCollisionSystem` 中添加 `PhaseTag` 豁免逻辑
  - 在 `FlowingThrust::DoCast` 中，突刺开始时添加 `PhaseTag`
  - 在 `FlowingThrust::DoEnd` 中移除 `PhaseTag`
  - 📁 `behaviors/FlowingThrust.cpp`, 可能涉及 `TilemapCollisionSystem.cpp`
  - ⏱️ ~1.5h

- [x] **4.2** 实现 FlowingThrust [150] 要害感知 - 暴击率翻倍
  - **现状**: `if (hp > 99%)` 内部为空 TODO
  - **操作**: 
    1. 在 DoHit 中，检测目标满血或受控状态 (`StunnedTag`, `FrozenTag`, `RootedTag`)
    2. 若条件满足，临时将攻击者的 `crit_chance` 翻倍用于本次 Hit
    3. 或通过 `CombatEventDispatcher` 发布一个 `CritChanceModifier`
  - 📁 `behaviors/FlowingThrust.cpp`
  - ⏱️ ~1h

- [x] **4.3** 补全 RendingWave [211] 碎裂之刃 - 子弹道参数
  - **现状**: 设置了 `OnDeathBehavior::Split` 和 `split_count = 3`，但无子弹道数据
  - **操作**: 
    1. 在 `Projectile` 组件中确认或添加 `split_damage_mult`, `split_speed_mult`, `split_radius_mult` 字段
    2. 在 `ProjectileSystem` 的 Split 处理逻辑中使用这些参数
    3. 在 RendingWave 中设置合理默认值 (damage 50%, speed 80%, radius 60%)
  - 📁 `behaviors/RendingWave.cpp`, `components/Projectile.hpp`, `systems/ProjectileSystem.cpp`
  - ⏱️ ~1.5h

- [x] **4.4** 编译 + 功能验证
  - 进度：已完成（`.\build.bat` + `NoMoreDayTests.exe` 通过，游戏内机制验证通过）
  - ⏱️ ~30min

**Phase 4 检查点**: 113 风行者穿墙、150 要害感知翻倍暴击、211 碎裂弹道均可在游戏中观察到效果。

---

### Phase 5: 自动化 ID 校验测试 (Automated Guard) — ~2h

> **目标**: 防止未来再次出现"幽灵 ID"问题。
> **依赖**: Phase 1-4 全部完成

- [x] **5.1** 编写集成测试：加载 `skills.json` 并提取所有技能的 `talent_tree[].id`
  - 构建一个 `std::unordered_set<uint32_t>` 包含所有合法 ID
  - 📁 `tests/integration/GameplaySystems.cpp`
  - ⏱️ ~45min

- [x] **5.2** 为每个 Behavior 的 `Nodes` 命名空间编写反射测试
  - 验证命名空间中的每个 `constexpr` 值都存在于 5.1 构建的合法 ID 集合中
  - 若 ID 不在 JSON 中，测试失败并输出具体文件和 ID
  - 📁 `tests/integration/GameplaySystems.cpp`
  - ⏱️ ~45min

- [x] **5.3** 运行测试 + 全量编译
  - `.\build.bat` → `./bin/NoMoreDayTests.exe`
  - 结果：`143/143` 用例通过（新增 ID 守卫测试已生效）
  - ⏱️ ~30min

**Phase 5 检查点**: 测试全部通过，且故意修改一个 ID 能使测试立即失败。

---

## 工时汇总

| Phase | 名称 | 预估工时 | 风险 |
|-------|------|----------|------|
| Phase 1 | ID 常量化基础设施 | ~5h | 🟢 低 (纯重构) |
| Phase 2 | P0 ID 逻辑错位修正 | ~4h | 🔴 高 (逻辑变更) |
| Phase 3 | 元素系统补全 | ~6h | 🟡 中 (新功能) |
| Phase 4 | 缺失机制修复 | ~4h | 🟡 中 (跨系统) |
| Phase 5 | 自动化 ID 校验测试 | ~2h | 🟢 低 |
| **总计** | | **~21h (3-4 工作日)** | |

---

## 执行注意事项

1. **Phase 1 必须先完成**: 它是所有后续 Phase 的基础，确保修改时引用正确的常量。
2. **Phase 2 和 Phase 3/4 可部分并行**: 但 Phase 2 涉及 BladeFormation，Phase 3.4 也涉及同文件，需串行。
3. **每个 Phase 结束必须编译通过**: 不允许累积编译错误。
4. **JSON 不可修改**: 所有适配工作在 C++ 侧完成。
