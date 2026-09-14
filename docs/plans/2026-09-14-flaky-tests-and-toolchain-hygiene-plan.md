# 测试稳定性治理与工程护栏加固实施计划

- 日期：2026-09-14
- 依据设计：`docs/designs/2026-09-14-flaky-tests-and-toolchain-hygiene-design.md`
- 依据标准：`docs/workflows/planning.md`
- 基线 commit：`0401c406`
- 目标：修复格式化工具链配置不兼容、消除 MSVC 历史编译警告基线、加固间歇性 Flaky 测试用例，并补齐技能 11/12 机制兜底字面量测试，实现测试套件高确定性与工程规范闭环。

---

## 1. 实施思路与原理

1. **工具链枚举兼容（Toolchain Standard Fix）**：
   - 现状：`.clang-format:78` 声明 `Standard: Cpp20`。项目固化工具链 clang-format 22.1.3 已移除旧版 CamelCase 标准枚举拼写，解析该行报 `unknown enumerated scalar` 错误退出（A-02 复审 R3 实测记录）。
   - 方案：统一改为现行规范拼写 `Standard: c++20`，恢复工具链兼容性。
2. **Doctest 宏单参规范化（Warning C4002 Elimination）**：
   - 现状：[`tests/unit/DamageElementIndexTests.cpp:92`](file:///d:/PRJ/NoMoreDay/tests/unit/DamageElementIndexTests.cpp#L92) 与 [`tests/unit/DamagePipelineP3bTests.cpp:269`](file:///d:/PRJ/NoMoreDay/tests/unit/DamagePipelineP3bTests.cpp#L269) 使用了 `CAPTURE(a, b)`，向单参数宏传入多个参数，触发 MSVC C4002 警告且丢弃第二个实参。
   - 方案：拆分为连续的单参数调用 `CAPTURE(a); CAPTURE(b);`。
3. **消除丢弃返回值告警（Warning C4834 Elimination）**：
   - 现状：全仓共 53 处裸调用 EnTT `registry.get_or_emplace<...>(...)` 且丢弃 `[[nodiscard]]` 返回引用的语句，分布于 15 个文件（以 [`src/game/systems/skill/behaviors/PhantomTrance.cpp:275-276`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/PhantomTrance.cpp#L275) 为代表，该文件自身即有 8 处），每处均触发 MSVC C4834。完整清单与检测命令见设计 §2.2。
   - 方案：对全部裸丢弃点统一以 `(void)` 显式静音，保持无新堆分配且零运行时开销、零行为变更。
4. **测试靶标生命值护栏（Prevent `0 < 0` False Failures）**：
   - 现状：[`tests/unit/SkillBehaviorGuardTests.cpp:2317`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillBehaviorGuardTests.cpp#L2317) 断言 `outcome.elite_health_after_delayed_window < outcome.elite_health_after_first_tick`。当测试配置的精英怪物在前期遭受过量伤害致死时，两个变量均为 `0.0f`，导致断言假失败。
   - 方案：将靶标初始生命值从 `5000.0f` 提升至 `50000.0f`，并在断言前增加生命值大于 0 的存活前提守卫。
5. **A-02 兜底字面量测试补齐（Close Low-2 Residual）**：
   - 现状：天剑降临与血海在退役手写绑定后，`DoCast` 包含了各 5 个 `constexpr` 兜底常量，缺少未载入 JSON 时的回退单测。
   - 方案：在功能测试文件中补充专用用例，断言在键缺失场景下准确回退至兜底常量。

---

## 2. 伪代码引导与接口骨架

### 2.1 工具链修复（`.clang-format`）

```yaml
# .clang-format:78
Standard: c++20
```

### 2.2 宏参数与编译警告修正

```cpp
// tests/unit/DamageElementIndexTests.cpp:92
- CAPTURE(is_shadow, force_scalar);
+ CAPTURE(is_shadow);
+ CAPTURE(force_scalar);

// tests/unit/DamagePipelineP3bTests.cpp:269
- CAPTURE(threads, round);
+ CAPTURE(threads);
+ CAPTURE(round);

// src/game/systems/skill/behaviors/PhantomTrance.cpp:275-276（示例，代表全仓 53 处裸丢弃点）
- registry.get_or_emplace<BarrierComponent>(owner);
- registry.get_or_emplace<StatsDirty>(owner);
+ (void)registry.get_or_emplace<BarrierComponent>(owner);
+ (void)registry.get_or_emplace<StatsDirty>(owner);

// 其余 51 处同型修改，逐点定位命令（在仓库根执行）：
//   rg -n "^\s*registry\.get_or_emplace<[^>]+>\([^;]*\);\s*$" src --glob *.cpp
// 完整 15 文件分布清单见设计 §2.2。
```

### 2.3 `SkillBehaviorGuardTests.cpp` 护栏加固

```cpp
// tests/unit/SkillBehaviorGuardTests.cpp:2264-2265
- registry.get<HealthComponent>(eliteTarget).max = 5000.0f;
- registry.get<HealthComponent>(eliteTarget).current = 5000.0f;
+ registry.get<HealthComponent>(eliteTarget).max = 50000.0f;
+ registry.get<HealthComponent>(eliteTarget).current = 50000.0f;

// tests/unit/SkillBehaviorGuardTests.cpp:2317-2318
+ CHECK(impactNodes.elite_health_after_delayed_window > 0.0f);
  CHECK(impactNodes.elite_health_after_delayed_window <
        impactNodes.elite_health_after_first_tick);
```

### 2.4 机制兜底字面量测试补齐（`HeavenlySwordDescentNodes.cpp` / `BloodSeaNodes.cpp`）

前置：将天剑匿名命名空间常量（`HeavenlySwordDescent.cpp:33-37`）与血海函数局部常量（`BloodSea.cpp:302-306`）提升为行为类公开 `static constexpr` 成员，测试直接引用常量断言，避免字面量重复。

```cpp
// tests/functional/HeavenlySwordDescentNodes.cpp（新增用例，骨架）
TEST_CASE("[Functional] HeavenlySwordDescent - DoCast constexpr fallback constants") {
  TestSetupScope scope;
  // 未加载 skill_mechanics.json（或伪造技能 ID），触发 GetMech 兜底路径
  entt::registry registry;
  // 调用 DoCast（零天赋点场景）生成场实体
  // ...
  const auto &field = registry.get<HeavenlySwordFieldComponent>(field_entity);
  // 组件可直断的兜底落库值：
  CHECK(field.header.duration == doctest::Approx(HeavenlySwordDescent::kFieldDurationFallback)); // 5.0f
  CHECK(field.header.radius == doctest::Approx(HeavenlySwordDescent::kFieldRadiusFallback));     // 140.0f
  CHECK(field.header.tick_interval == doctest::Approx(0.5f)); // 组件默认值；零点数修正后 clamp(0.18f, 0.75f) 不改变结果
  CHECK(field.resist_reduction == doctest::Approx(6.0f));     // GetMech 兜底，非五常量之一，一并锚定
  // kImpactRadiusFallback(90)/kTierDamageBonusFallback(0.18)/kTierRadiusBonusFallback(14) 不落库于场组件，
  // 经 DoCast 行为效果间接断言（冲击命中范围、层数增伤/增径），不在组件断言中虚构字段。
}

// tests/functional/BloodSeaNodes.cpp（新增用例，骨架）
TEST_CASE("[Functional] BloodSea - DoCast constexpr fallback constants") {
  TestSetupScope scope;
  entt::registry registry;
  // 纯代码默认状态（零血欲消耗）触发 DoCast
  // ...
  const auto &field = registry.get<BloodSeaFieldComponent>(field_entity);
  CHECK(field.header.duration == doctest::Approx(BloodSea::kFieldDurationDefault));       // 4.8f
  CHECK(field.header.radius == doctest::Approx(BloodSea::kFieldRadiusDefault));           // 120.0f
  CHECK(field.header.tick_interval == doctest::Approx(BloodSea::kFieldTickDefault));      // 0.25f
  CHECK(field.leech_ratio == doctest::Approx(BloodSea::kLeechRatioDefault));              // 0.12f
  // kBloodthirstDamageBonusDefault(0.12) 零消耗时 bonus_damage_mult == 1.0f 不可直接观测，
  // 经「消耗血欲后 bonus_damage_mult == 1 + N * kBloodthirstDamageBonusDefault」的行为断言覆盖。
}
```

---

## 3. 原子任务拆分

- [x] **Task 1: 工具链规范修复**
  - [x] T1.1 修改 `.clang-format:78` 为 `Standard: c++20`。
  - [x] T1.2 确认配置文件能够被项目固化工具链 clang-format 22.1.3 无错误加载（复跑 A-02 复审 R3 的报错命令验证）。

- [x] **Task 2: 编译警告基线清零（C4002 / C4834）**
  - [x] T2.1 修复 `tests/unit/DamageElementIndexTests.cpp:92` 的 `CAPTURE` 宏。
  - [x] T2.2 修复 `tests/unit/DamagePipelineP3bTests.cpp:269` 的 `CAPTURE` 宏。
  - [x] T2.3 按设计 §2.2 的 15 文件分布清单，对全仓 53 处裸丢弃的 `registry.get_or_emplace` 语句统一加 `(void)` 显式静音（含 `src/game/systems/skill/behaviors/PhantomTrance.cpp:275-276` 代表点），保持零行为变更。
  - [x] T2.4 执行 `build.bat RelWithDebInfo`，确认 C4002 与 C4834 彻底清零。

- [x] **Task 3: 核心 Flaky 测试防抖与确定性加固**
  - [x] T3.1 增加 `tests/unit/SkillBehaviorGuardTests.cpp` 中精英怪物的血量预算并添加存活断言守卫。
  - [x] T3.2 优化 `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:235`，引入缓冲步进与非崩溃断言守卫。

- [x] **Task 4: 技能 11/12 兜底字面量回归测试补齐（闭环 Low-2）**
  - [x] T4.0 将天剑 5 个兜底常量（`HeavenlySwordDescent.cpp:33-37`）与血海 5 个兜底常量（`BloodSea.cpp:302-306`）提升为 `HeavenlySwordDescent` / `BloodSea` 的公开 `static constexpr` 成员，行为不变。
  - [x] T4.1 在 `tests/functional/HeavenlySwordDescentNodes.cpp` 中添加 `DoCast constexpr fallback constants` 测试用例。
  - [x] T4.2 在 `tests/functional/BloodSeaNodes.cpp` 中添加 `DoCast constexpr fallback constants` 测试用例。

- [x] **Task 5: 全量回归与多轮稳定性验证**
  - [x] T5.1 运行全量 `ctest --test-dir build -C RelWithDebInfo -L ci` 连续 5 轮，确认零偶发失败。
  - [x] T5.2 专项运行 `ctest --test-dir build -C RelWithDebInfo -R "SkillBehaviorGuard|SingleGpuTimer" --repeat until-fail:10` 验证零抖动。
  - [x] T5.3 归档 A-02 复审遗留 Low-2、R2、R3、R4 销项证据（见 `docs/reviews/2026-09-14-flaky-tests-toolchain-hygiene-closure.md`）。

---

## 4. 测试方法与测试用例

1. **受影响测试套件**：
   - `tests/unit/DamageElementIndexTests.cpp`
   - `tests/unit/DamagePipelineP3bTests.cpp`
   - `tests/unit/SkillBehaviorGuardTests.cpp`
   - `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp`
   - `tests/functional/HeavenlySwordDescentNodes.cpp`
   - `tests/functional/BloodSeaNodes.cpp`
2. **多轮稳定性验证命令**：
   ```powershell
   # 1. 验证编译警告清零
   .\build.bat RelWithDebInfo
   # 2. 专项高频防抖验证
   ctest --test-dir build -C RelWithDebInfo -R "SkillBehaviorGuard|SingleGpuTimer" --repeat until-fail:10 --output-on-failure
   # 3. 连续 5 轮 CI 门禁回归
   1..5 | ForEach-Object { ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure }
   ```

---

## 5. 验证任务完成标准（DoD）

1. **配置有效（DoD 1）**：`.clang-format` 使用小写 `c++20`，clang-format 22.1.3 无任何语法或枚举解析报错。
2. **编译零警告（DoD 2）**：`build.bat RelWithDebInfo` 构建输出中 C4002 与 C4834 警告数为 0（全仓 53 处 C4834 裸丢弃点全部消除），达成真 0 警告。
3. **测试零抖动（DoD 3）**：CI 套件与时序测试套件连续运行 5 轮，通过率为 100%（0 失败）。
4. **兜底有护栏（DoD 4）**：天剑与血海的全部兜底常量均在无配置情境下纳入回归覆盖（组件直断或经行为效果断言），Track A-02 Low-2 正式闭环。
