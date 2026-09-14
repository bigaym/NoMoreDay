# 测试稳定性治理与工程护栏加固设计规范

- 日期：2026-09-14
- 状态：草案（方案设计阶段）
- 依据标准：`docs/workflows/design.md`
- 前置依赖：
  - `docs/reviews/2026-09-14-skill-abstraction-a02-review.md`（R2 兜底字面量断言、R3 格式化门禁、R4 告警基线）
  - `docs/reports/damage-pipeline-modernization/phase-P4/P4-report.md`（§4.3 偶发环境 flake 记录）
  - `docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md`（历史告警基线 C4834/C4002）
- 涉及范围：
  - `.clang-format`（代码格式化工具链配置）
  - MSVC 编译警告（C4834 nodiscard 丢弃、C4002 宏参数过多）
  - 核心 Flaky 测试：
    - `tests/unit/SkillBehaviorGuardTests.cpp`（血量时序断言）
    - `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp`（GPU Timer 状态）
    - `tests/functional/SkillBehaviors.cpp`（技能 10 随机性防护）
  - A-02 复审遗留 Low-2（技能 11/12 `constexpr` 兜底字面量单测断言）

---

## 1. 背景与核心痛点（Why & What）

### 1.1 现状与痛点分析

在近期的快速迭代和重构过程中，测试套件与开发工具链积累了若干工程卫生与稳定性债务：

1. **代码格式化门禁失效（Toolchain Incompatibility）**：
   - 仓库根目录 [`.clang-format:78`](file:///d:/PRJ/NoMoreDay/.clang-format#L78) 声明了 `Standard: Cpp20`。项目固化的 clang-format 工具链（22.1.3，见 A-02 复审报告 R3 实测记录）已将 C++ 标准枚举拼写规范化为小写 `c++20` 并移除旧版 CamelCase 别名，解析该行时直接报 `unknown enumerated scalar` 错误退出，使得代码格式化检查无法在本地或 CI 正常执行。
2. **Doctest 宏滥用引发编译警告（MSVC C4002）**：
   - 在 [`tests/unit/DamageElementIndexTests.cpp:92`](file:///d:/PRJ/NoMoreDay/tests/unit/DamageElementIndexTests.cpp#L92)（`CAPTURE(is_shadow, force_scalar);`）与 [`tests/unit/DamagePipelineP3bTests.cpp:269`](file:///d:/PRJ/NoMoreDay/tests/unit/DamagePipelineP3bTests.cpp#L269)（`CAPTURE(threads, round);`）中，开发者向仅接受单参数的 `CAPTURE(x)` 宏传入了两个实参。
   - 这引发了 MSVC C4002 警告（*“too many actual parameters for macro 'CAPTURE'”*），且第二个参数在捕获时被预处理器丢弃。
3. **未处理 nodiscard 返回值引发编译警告（MSVC C4834）**：
   - 全仓存在大量裸调用 EnTT `registry.get_or_emplace<...>(...)` 且丢弃返回引用的语句（EnTT 该方法标记 `[[nodiscard]]`），每处均触发 C4834（*“discarding return value of function with 'nodiscard' attribute”*）。以 [`src/game/systems/skill/behaviors/PhantomTrance.cpp:275-276`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/PhantomTrance.cpp#L275)（`get_or_emplace<BarrierComponent>` / `get_or_emplace<StatsDirty>`）为代表。
   - 实测清点（检测命令见 §2.2）：全仓共 **53 处**裸丢弃点，分布于 15 个文件：`SkillSystem.cpp`(10)、`BeamChannelDeliverySystem.cpp`(9)、`PhantomTrance.cpp`(8)、`AreaFieldDeliverySystem.cpp`(4)、`MovementStanceSystem.cpp`(3)、`InventorySystem.cpp`(3)、`BladeWard.cpp`(3)、`FlowingThrust.cpp`(3)、`BoomerangDeliverySystem.cpp`(3)、`ProgressionSystem.cpp`(2)、`StatsSystem.cpp`(1)、`EffectSystem.cpp`(1)、`BladeBoomerang.cpp`(1)、`InfiniteBlades.cpp`(1)、`SwordArray.cpp`(1)。达成「C4834 清零」目标必须全量处理，仅修代表点无法闭环。
4. **间歇性 Flaky 测试破坏 CI 确定性（Intermittent Flaky Tests）**：
   - **血量打空越界**：[`tests/unit/SkillBehaviorGuardTests.cpp:2317`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillBehaviorGuardTests.cpp#L2317) 中断言 `outcome.elite_health_after_delayed_window < outcome.elite_health_after_first_tick`。当测试配置的精英靶标血量在前期打击中被打至 0 时，断言变为 `0 < 0` 从而偶发失败。
   - **GPU Timer 状态就绪抖动**：[`tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:235`](file:///d:/PRJ/NoMoreDay/tests/integration/SingleGpuTimerOwnerRegressionTest.cpp#L235) 中断言 `frameResult.state == debug::QueryState::Valid`。在系统重载或 GPU 异步流水线延迟时，查询槽位尚未轮转回就绪状态，偶发非确定性失败。
5. **技能 11/12 兜底字面量断言缺失（Low-2 遗留）**：
   - Track A-02 退役手写机制绑定后，技能 11（天剑降临）和技能 12（血海）的 `DoCast` 中包含了各 5 个关键的降级兜底字面量（天剑 `90/140/5/0.18/14`，血海 `4.8/120/0.25/0.12/0.12`）。
   - 目前的功能测试仅覆盖了正常读取 JSON 配置的分支，缺少“当配置键缺失时正确回退到这些 `constexpr` 兜底常量”的防御性断言。

### 1.2 核心目标

1. **修正工具链配置**：将 `.clang-format` 修复为符合 LLVM 现代标准规范的配置，使代码格式化工具可直接无损运行。
2. **彻底清零编译警告基线**：修正多参 `CAPTURE` 宏调用和 `get_or_emplace` 废弃返回值，达成全仓构建零警告。
3. **加固核心测试确定性**：
   - 为战斗时序测试引入存活护栏与充沛血量预算，杜绝 `0 < 0` 假失败；
   - 为 GPU Timer 查询测试引入防抖与状态守卫；
   - 复核技能用例，确保所有战斗测试均锁定暴击率与命中率，阻断 RNG 漂移。
4. **补齐 A-02 兜底字面量测试**：在未载入 JSON 或数据键缺失的情境下，对天剑与血海的全部 10 个 `constexpr` 兜底字面量纳入回归覆盖（组件直断或经行为效果断言）。

---

## 2. 治理方案与详细设计

### 2.1 工具链与格式化配置修复

修改 [`.clang-format:78`](file:///d:/PRJ/NoMoreDay/.clang-format#L78)：

```yaml
# 原内容：
Standard: Cpp20

# 修改为：
Standard: c++20
```

- **兼容性论证**：`c++20` 为项目固化工具链 clang-format 22.1.3 实测接受的规范拼写（R3 已复现旧拼写的解析失败）；修改后以「配置被固化版本无错加载」为验收口径，不再对未验证的旧版工具链兼容性做断言，如需支持其他版本应先行实测。

### 2.2 宏参数与编译警告清理

1. **修复 `CAPTURE` 宏（MSVC C4002）**：
   - `tests/unit/DamageElementIndexTests.cpp:92`：
     ```cpp
     // 原代码：
     CAPTURE(is_shadow, force_scalar);
     // 改为：
     CAPTURE(is_shadow);
     CAPTURE(force_scalar);
     ```
   - `tests/unit/DamagePipelineP3bTests.cpp:269`：
     ```cpp
     // 原代码：
     CAPTURE(threads, round);
     // 改为：
     CAPTURE(threads);
     CAPTURE(round);
     ```
2. **消除丢弃返回值警告（MSVC C4834）**：
   - 检测命令（命中后需人工排除两类非丢弃点：上一行以 `=` 结尾的赋值续行、链式调用 `.AddOrRefresh(...)` 的使用点）：
     ```powershell
     rg -n "^\s*registry\.get_or_emplace<[^>]+>\([^;]*\);\s*$" src --glob *.cpp
     ```
   - 修复策略：对全部裸丢弃点统一改为 `(void)` 显式静音（零运行时开销、零行为变更），以 `PhantomTrance.cpp:275-276` 为样例：
     ```cpp
     // 原代码：
     registry.get_or_emplace<BarrierComponent>(owner);
     registry.get_or_emplace<StatsDirty>(owner);
     // 改为：
     (void)registry.get_or_emplace<BarrierComponent>(owner);
     (void)registry.get_or_emplace<StatsDirty>(owner);
     ```
   - **禁止**用 `emplace_or_replace` 替代：对 `BarrierComponent` 等携带数据的组件，`emplace_or_replace` 会销毁并重建既有实例、覆写当前状态（如已吸收护盾值），属行为变更，违反非目标约束；`get_or_emplace` 的「存在则复用」语义必须保留。

### 2.3 核心测试确定性与防抖加固

1. **`SkillBehaviorGuardTests.cpp` 伤害递进断言防抖**：
   - 在 `tests/unit/SkillBehaviorGuardTests.cpp:2264-2265` 中，将测试精英怪物的初始血量由 `5000.0f` 提升至 `50000.0f`，防止前置爆发将怪物打入濒死或致死区间；
   - 在断言处补充存活前提校验：
     ```cpp
     CHECK(outcome.elite_health_after_delayed_window > 0.0f);
     CHECK(outcome.elite_health_after_delayed_window < outcome.elite_health_after_first_tick);
     ```
2. **`SingleGpuTimerOwnerRegressionTest.cpp` GPU 查询防抖**：
   - 增加缓冲帧推进与重试逻辑，确保在 GPU 查询未及时 Flush 时给予足够的流水线步进周期；
   - 明确测试关注点是“防崩溃与状态机正确转换”，对重载机器上的瞬态延迟状态进行优雅兜底。

### 2.4 A-02 兜底字面量专项测试补齐（Low-2 闭环）

在 [`tests/functional/HeavenlySwordDescentNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/HeavenlySwordDescentNodes.cpp) 与 [`tests/functional/BloodSeaNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/BloodSeaNodes.cpp) 中各自新增一个专用的缺省降级测试用例。

**前置：兜底常量可测化**。天剑的 5 个 `constexpr` 兜底常量目前位于 `HeavenlySwordDescent.cpp` 匿名命名空间（:33-37），血海的 5 个位于 `BloodSea::DoCast` 函数局部（:302-306），测试无法引用。需将两组常量分别提升为 `HeavenlySwordDescent` / `BloodSea` 的公开 `static constexpr` 成员（头文件中声明），测试直接引用常量断言，消除测试侧字面量重复。

1. **天剑降临兜底测试**：
   - 构造未加载 `skill_mechanics.json` 或传入伪造技能 ID 的环境，直接调用 `DoCast` 生成场；
   - 可由场组件直接断言的兜底落库值（零天赋点场景）：
     - `header.duration == kFieldDurationFallback`（5.0f）
     - `header.radius == kFieldRadiusFallback`（140.0f）
     - `header.tick_interval == 0.5f`（组件默认值；零点数修正后 `clamp(0.18f, 0.75f)` 不改变结果。注意：`0.18f` 是 `kTierDamageBonusFallback` 与 clamp 下限，**不是** tick 兜底值）
     - `resist_reduction == 6.0f`（来自 `GetMech("base_resist_reduction", 6.0f)` 的 GetMech 兜底，非五常量之一，一并锚定）
   - `kImpactRadiusFallback`（90.0f）、`kTierDamageBonusFallback`（0.18f）、`kTierRadiusBonusFallback`（14.0f）不落库于场组件：经 DoCast 行为效果间接断言（冲击命中范围、层数增伤/增径），或引用提升后的类级常量做静态锚定，不在组件断言中虚构字段。
2. **血海兜底测试**：
   - 同理在纯代码默认状态（零血欲消耗）下触发 `DoCast`，断言：
     - `header.duration == kFieldDurationDefault`（4.8f）
     - `header.radius == kFieldRadiusDefault`（120.0f）
     - `header.tick_interval == kFieldTickDefault`（0.25f）
     - `leech_ratio == kLeechRatioDefault`（0.12f）
   - `kBloodthirstDamageBonusDefault`（0.12f）在零消耗时 `bonus_damage_mult == 1.0f` 不可直接观测，经「消耗血欲后 `bonus_damage_mult == 1 + N * kBloodthirstDamageBonusDefault`」的行为断言覆盖。

---

## 3. 验收标准与验证方案

1. **编译器 0 警告**：
   - 执行 `./build.bat RelWithDebInfo`，编译输出必须无 C4002、无 C4834，全仓无任何新增警告。
2. **测试 100% 确定性**：
   - 连续执行 5 轮 `ctest --test-dir build -C RelWithDebInfo -L ci`，100% 通过（0 失败）；
   - 针对性执行 `ctest --test-dir build -C RelWithDebInfo -R "SkillBehaviorGuard|SingleGpuTimer|SkillBehaviors" --repeat until-fail:10`，无任何一次失败。
3. **工程卫生与文档闭环**：
   - A-02 复审报告中的 R2、R3、R4 风险项全量关闭并在跟踪清单中归档销项。
