# 审查报告：持久场组件迁移 + 抖动测试与工具链卫生

- 日期：2026-09-14
- 审查对象：工作区未提交变更（45 个修改文件 + 8 个新增文件，+约 700/-约 660 行）
- 驱动文档：
  - `docs/designs/2026-09-14-flaky-tests-and-toolchain-hygiene-design.md`（下称设计 A）
  - `docs/designs/2026-09-14-persistent-field-component-migration-design.md`（下称设计 B）
  - 对应计划：`docs/plans/2026-09-14-flaky-tests-and-toolchain-hygiene-plan.md`、`docs/plans/2026-09-14-persistent-field-component-migration-plan.md`
  - 既有归档：`docs/reviews/2026-09-14-flaky-tests-toolchain-hygiene-closure.md`（下称 closure 报告）

## 一、结论

**提交**

无 Blocker / High 发现项。两组设计均按计划落地，行为等价性经逐行核查成立，构建零警告，相关测试全绿。发现项均为 Low / 观察级，不阻塞提交，见第五节。

## 二、审查轮次与输入

- 轮次：第 1 轮（单轮通过）
- 输入：
  - `git diff` 全量（src / tests / 配置分三份转存核查）
  - 新增文件全文：`src/game/systems/skill/PersistentFieldSystem.hpp`（27 行）、`PersistentFieldSystem.cpp`（45 行）、`src/game/systems/skill/components/PersistentFieldComponents.hpp`（86 行）
  - 设计 A / B、两份计划、closure 报告
  - 构建日志、CTest 日志、doctest 直跑日志
  - `scripts/check_module_boundaries.py` 输出

## 三、变更文件边界

- **src（15 文件 get_or_emplace 清理）**：SkillSystem、BeamChannelDelivery、PhantomTrance、AreaFieldDelivery、MovementStance、Inventory、BladeWard、FlowingThrust、BoomerangDelivery、Progression、StatsSystem、EffectSystem、BladeBoomerang、InfiniteBlades、SwordArray —— 全部按设计 A §2.2 统一加 `(void)`，未发现使用 `emplace_or_replace` 的错误替代（该替代会重建 BarrierComponent 覆写护盾值，设计明令禁止）。
- **src（设计 B 迁移）**：`SkillDefs.hpp` 删除两组件（-80 行）；新增 `components/PersistentFieldComponents.hpp`、`PersistentFieldSystem.hpp/.cpp`；`SkillSystem.cpp` 硬编码 view 循环替换为单行系统调用；`BladeMasteryService.cpp` 删除本地模板改调共享清理；`HeavenlySwordDescent` / `BloodSea` 常量提升 + HUD 查询门面；`GameUiSnapshotBuilder.cpp` / `PlayerHUD.cpp` 改走只读 DTO；`GameplayRenderAdapter.cpp` 按设计保留直接组件访问（仅加 include）。
- **计划外改动（已核实为正确性修复）**：`ProjectileSystem.cpp:511-515` 拦截概率边界修复、`SkillTreeController.cpp` 5 处 `SetNodeVisible` 加 `(void)`（`UiRuntime.hpp:88` 声明 `[[nodiscard]]`）、`SwordArrayNodes.cpp:124` `EnsureLoaded` 加 `(void)`。closure 报告 §4 已如实记录前两项为「计划外追加修复」，不属于越权触及。
- **tests**：CAPTURE 拆分（DamageElementIndexTests.cpp:92、DamagePipelineP3bTests.cpp:269、SkillBakerFlagConsumerTests 5 处、SpecStateMappingTests 5 处、SpecStateTableTests.cpp:125）；flaky 修复（SkillBehaviorGuardTests.cpp:2264-2265 精英血量 5000→50000 + :2318 存活护栏 `CHECK(impactNodes.elite_health_after_delayed_window > 0.0f)`；SingleGpuTimerOwnerRegressionTest.cpp:220-248 缓冲帧重试）；常量回归测试（BloodSeaNodes.cpp:558-599、HeavenlySwordDescentNodes.cpp:319-345）；迁移夹具同步（9 文件补 include；BladeMasteryTests.cpp:262/275 场实体补 `SkillComponent(12u, player)` + `PersistentFieldTag`）。
- **配置**：`.clang-format:78` `Standard: Cpp20`→`c++20`（clang-format 22.1.3 拒绝旧拼写）；`settings.json` 仅 benchmarkScore/updatedAtUtc 自动刷新。

## 四、范围对齐与质量核查

### 4.1 设计 A 对齐

| 验收项 | 结果 |
| --- | --- |
| `.clang-format` Standard 修复 | ✅ |
| C4002 CAPTURE 拆分（全仓） | ✅ 含计划外发现的 SkillBaker/SpecState 系列 |
| C4834 `(void)` 清理（src/*.cpp 53 处） | ✅ 构建日志 C4834 计数 0 |
| flaky：精英血量 + 存活护栏 | ✅ 数值与设计一致 |
| flaky：GPU 定时器缓冲帧 | ✅ 重试循环含 BindFramebuffer + render + PollReadyQueries |
| 常量提升为类级 `static constexpr` | ✅ 天剑 5 项 / 血海 5 项均迁至行为头文件类级公开成员（BloodSea.hpp:26-30 核实） |
| 常量回归测试 | ✅ doctest 直跑 2/2 通过（21 断言），断言值与实现数值链逐项吻合 |
| 构建 0 警告（RelWithDebInfo） | ✅ build.bat exit=0，warning C 计数 0 |
| CI 测试 | ✅ `ctest -L ci` 1/1、`-L integration` 6/6；专项用例手写 10 次循环 0 失败（doctest 2.5.3 无 `--repeat`，closure §6.3 已如实记录 CTest 聚合注册限制） |

### 4.2 设计 B 对齐

| 验收项 | 结果 |
| --- | --- |
| 组件迁移 + static_assert | ✅ `PersistentFieldComponents.hpp:46-47/83-84`，字段与默认值逐项与旧 SkillDefs.hpp 一致（天剑 header{5.0,140,0.5}/resist=6.0；血海 header{5.0,120,0.25}/bonus=1.0/leech=0.12） |
| `PersistentFieldSystem::Update` | ✅ 签名与设计一致，循环逻辑平移无改动 |
| `DestroyOwnedPersistentFields` | ✅ 与设计伪代码逐字一致：`view<PersistentFieldTag, SkillComponent>` 过滤 `owner` 匹配 + `skill_id==0` 全销毁哨兵，两段式收集后销毁（避免迭代中 destroy） |
| `OnMasterySwitchCleanup` | ✅ 与 BladeMasteryService 旧天剑分支逐行对照无遗漏：formation/SpiritSwordAI(skill_id==3)/BeamChannel(skill_id==5, tick_timer=min clamp)/original_* 基准还原；销毁语义等价旧 DestroyOwnedAreaFields+DestroyOwnedFields（场实体同时带 AreaFieldComponent+PersistentFieldTag+SkillComponent，创建路径 BloodSea.cpp:420-423、HeavenlySwordDescent.cpp:647-650 核实） |
| UI 只读 DTO | ✅ 两个 Query 函数语义与旧 PlayerHUD helper 等价（含多场取 duration 最大、天剑合并 AreaField(11) 双查询）；GameUiSnapshotBuilder bloodSea 分支新增 `remaining_duration > 0.0f` 前置与旧「无场不覆写」语义等价 |
| 模块边界验收 5 条 | ✅ SkillDefs.hpp/BladeMasteryService.cpp/SkillSystem.cpp/UI 均无底层组件引用（rg 0 命中）；`check_module_boundaries.py` PASS（0/0 edges） |
| 非目标约束 | ✅ 未改战斗数值、未合并组件、未触 SpecState |

### 4.3 行为等价性重点核查（防「迁移引入漂移」）

- `QueryBloodSeaHud` 多场并存时 `has_void_keystone`/`miasma_duration_bonus` 与 `remaining_duration` 取自同一场（duration 最大者），与旧 helper 一致。
- 天剑/血海 `header.duration` 均为每帧递减的剩余时长（UpdateField 实现），Query 读值即剩余时长，与旧 HUD 语义一致。
- `attunement` 变化路径（SetHeavenlySwordAttunement → RefreshPlayerState → CleanupSpecializedFields）在迁移后仍完整触发清理，DoCast 重读 GetHeavenlyAttunement，行为与迁移前一致。
- 常量回归测试的 stub 技能注册经 TestSetupScope 析构还原（ResetSkillRegistries 重载 skills.json），无跨用例污染。

## 五、发现项

> 2026-09-14 更新：发现项 1、2、3 已在提交前处理完毕，见各条目「处置」。

| # | 严重度 | 位置 | 描述 |
| --- | --- | --- | --- |
| 1 | Low | `src/game/systems/combat/MonsterAffixSystem.hpp:491`、`:1294`；`src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:153`；`tests/performance/GPUSyncBenchmark.cpp:76` | 残留裸 `get_or_emplace` 丢弃 `[[nodiscard]]` 返回值。设计 A 的检测命令 `rg ... src --glob *.cpp` 不覆盖 `.hpp` 与 tests，属扫描盲区。**处置：4 处已全部补 `(void)`**（其余候选行经核实为 `auto &` 绑定或链式调用，返回值已被使用，不触发 C4834，未改动）。建议后续把检测 glob 扩为 `*.{cpp,hpp}` 并排除「返回值被使用」的形态。 |
| 2 | Low | `src/game/systems/skill/ProjectileSystem.cpp:511-515` | 拦截概率边界修复为计划外改动（closure §4 已记录），`chance==1.0` 分支原先无直测。**处置：新增直测** `tests/integration/SkillSystemTests.cpp` `[Integration] SkillSystem - BladeWard interception_chance 1.0 never misses`（4000 次独立 trial 断言全拦截）；已做回归捕获验证——临时还原旧判定后该测试以 `CHECK(3999 == 4000)` 失败，证明能有效捕获 roll==1000 边界回归，非假测试。 |
| 3 | Low | `settings.json`（仓库根目录） | benchmarkScore/updatedAtUtc 为运行时自动刷新产物，混入本次变更。**处置：已回退至 HEAD 版本，工作区无 diff。** |
| 4 | 观察 | `src/game/application/ui/GameUiSnapshotBuilder.cpp:393-410` | 保留的 AreaFieldComponent 遍历（source_skill_id==11 窗口 + ==12 节点读取）与 `QueryHeavenlyFieldHud` 存在轻微重复查询。低频 UI 路径、行为一致，不构成问题；若后续 UI 全面走 DTO 可一并收敛。 |

## 六、最佳实践建议

1. 设计 A 的检测命令建议固化为脚本并扩展 glob 至 `.hpp` 与 `tests/`，防止 C4834 复发（对应发现项 1）。
2. `DestroyOwnedPersistentFields` 的两段式收集-销毁模式值得作为仓库内销毁遍历的标准范式记录（避免迭代中 mutate registry）。
3. 后续若新增持久场类型，`PersistentFieldSystem::Update` 的 per-type view 循环可考虑收敛为基于 `PersistentFieldHeader` 的统一 tick 入口（当前两类尚可接受，属远期项）。

## 七、剩余风险

- CTest 聚合注册限制导致「单测名级 until-fail」无法通过 ctest 表达，当前以 doctest 直跑循环替代；若未来 CI 需要严格 N 轮证据，需调整 CTest 注册粒度。
- 发现项 1 的检测盲区已在本批次补齐，但检测命令本身尚未固化为脚本，`.hpp`/tests 的新增代码仍可能引入同类裸调用（建议按第六节第 1 条落地）。

## 八、下一步动作

1. ~~按发现项 3 处理 `settings.json`~~（已完成回退）；发现项 1、2 已在本批次处置完毕。
2. 提交拆分为两个 commit：test-hygiene（设计 A）与 component-migration（设计 B）；`SkillSystem.cpp`、`BladeMasteryService.cpp` 因同时承载 (void) 清理与迁移改动，整体归入 component-migration commit，并在 commit message 中注明。
