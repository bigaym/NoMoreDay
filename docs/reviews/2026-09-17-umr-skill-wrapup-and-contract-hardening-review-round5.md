# UMR 技能专精收尾与契约加固 —— 第 5 轮独立复核（review.md 11 项修复增量）

- 审查目标：对未提交工作区中 `review.md` 静态扫描 11 项发现的**修复增量**做独立只读复核，并检查修复是否引入新缺陷。
- 审查轮次：Round 5（fix-delta verification）
- 审查方式：只读。`git diff` + `rg` + 知识图谱；**未构建、未跑测试、未修改任何文件**。
- 基准：HEAD = `48618225`（`feat(modifier): migrate skill 10/11/12 specialization nodes to UMR delivery ops`）

---

## 1. 输入

| 输入 | 说明 |
|---|---|
| `docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md` | v1.2 设计（含实施期修订） |
| `docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md` | v1.3 计划 |
| `docs/reviews/2026-09-17-...-review{,round2,round3,round4}.md` | 前 4 轮评审 |
| `review.md`（仓库根，gitignore） | 11 项独立静态扫描发现 |
| `git diff`（工作区 vs HEAD） | 本次修复增量与原始实现的唯一事实来源 |

## 2. 变更文件边界（`git status --porcelain` 摘要）

- 本次增量（`git diff` 可见）：`src/game/systems/skill/behaviors/SwordArray.cpp`、`SevenStarSlash.cpp`、`PhantomTrance.cpp`、`BeamChannelDeliverySystem.cpp`、`SevenStarSlashShared.hpp`、`SkillSpecializationBaker.cpp`、`SkillProfileResolve.cpp`、`SkillDefs.hpp`、`BloodSea.cpp`、`HeavenlySwordDescent.cpp`、`GameplayState.cpp`、`tests/TestCommon.hpp`、`tests/functional/SwordArrayNodes.cpp`、`tests/functional/SevenStarSlashNodes.cpp`、`tests/unit/SkillProfileResolveTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`、`assets/data/mastery_skill_trees.json`、`assets/data/skill_mechanics_schema.json`、`build.bat`
- 新增未跟踪文件（本次增量依赖）：`src/game/systems/skill/behaviors/BeamChannelShared.hpp`、`src/game/systems/skill/behaviors/SwordArrayShared.hpp`、`tests/unit/SkillProfileResolveSentinelTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`、design/plan/round1-4 评审文档
- 与本次修复无关的渲染/测试改动（不在复核范围）：`tests/integration/{GPUABIBindingTierIntegrationTest,JFAPassUpsampleMaskTest,MaterialLightingIntegrationTest,RenderSystemPhaseDToggleSmokeTest}.cpp`、`tests/performance/MaterialLightingBenchmark.cpp`、`tests/unit/{RenderSystemInitializeFailureTest,VFXSequencerTest}.cpp`、`设计文档/职业被动和技能设置.md`

> 注：`tests/functional/SevenStarSlashNodes.cpp` 仅注释更新（127 行说明 dodge 修正符已移除），无断言变化。

## 3. 11 项发现逐项裁定

| # | 位置 | 裁定 | 依据 |
|---|---|---|---|
| 1 | `SwordArray.cpp` | **已修复** | `cast_target` 统一钳制（详见 §4a） |
| 2 | `SevenStarSlash.cpp:421` | **已修复** | `skillData` 在 359 行已保证非空，三元退化删除正确 |
| 3 | `TestCommon.hpp` | **部分修复** | 清理生效，但失败路径无诊断；且与 #9 组合产生新缺陷（见 F1/F2） |
| 4 | `TestCommon.hpp:144-158` | **已修复** | `out.is_open()` / `out.good()` 双分支均 `DOCTEST_WARN_MESSAGE` |
| 5 | `SevenStarSlashShared.hpp:29` | **已修复** | `kSevenStarSlashBaseRadius = 96.0f` 单源；Baker:140 与行为:424 共用 |
| 6 | `BeamChannelDeliverySystem.cpp` | **部分修复** | 主体归一正确，`ResolveSkill7Element` 未覆盖（见 F3） |
| 7 | `PhantomTrance.cpp:107` | **已修复** | 契约保持「始终返回有效引用」，回退值与原哨兵默认值等价 |
| 8 | `SwordArrayNodes.cpp` | **部分修复** | 675 钳制子用例是真伪证；603 远端断言与单测重复（见 F4） |
| 9 | `TestCommon.hpp:117-120` | **已修复** | `in.bad()` + 尺寸校验齐备；但与 #3 组合产生新缺陷（见 F1） |
| 10 | `SkillWrapupHardeningTests.cpp` | **已修复** | 断言改为 `tree->nodes[1015].stat_modifiers.empty()`，命中生产数据路径（`StatsSystem.cpp:459-488`） |
| 11 | `SkillProfileResolveSentinelTests.cpp` | **已修复** | step-2/step-3 用例均可证伪旧实现 |

## 4. 逐项技术论证

### (a) `SwordArray.cpp` 的 `cast_target` 提升

- 钳制单源位于 `SwordArray.cpp:86-106`：`is_mobile_aura` 于 86 行由 `nodeActive(SwordArrayNodes::MobileAura, specState.mobileAura)` 求得，语义与旧实现的 `nodeActive(...)` 完全一致（`specState`/`exec.active_nodes` 在提升点之前已解析，且两者之间无任何变更）。
- 作用域正确：`profile` 在 `SwordArray.cpp:50` 由 `ResolveBakedProfile` 取得，`kSwordArrayBaseCastRange` 由 6 行 include 的 `SwordArrayShared.hpp:8`（`NoMoreDay` 命名空间）提供，`<cmath>` 于 31 行引入，`std::sqrt` 可用。
- 在射程内：`distSq > maxRange * maxRange` 为假 → `cast_target` 保持 `exec.target_pos`，与旧路径逐位一致。
- mobile aura：`cast_target = casterPos`，与旧 `spawn_pos` 覆盖一致；owner 无 `Position` 时退回 `exec.target_pos`，与旧行为一致。
- 675 挪阵：`SwordArray.cpp:129-132` 现写 `cast_target`，确已受限；旧实现直写 `exec.target_pos`（review.md 所述 130-132 行）。原始放置路径 `SwordArray.cpp:149` 同样使用 `cast_target`。
- 语义漂移（见 F5）：mobile aura 的覆盖现在**也作用于挪阵分支**。因 `SwordArray::Update`（`SwordArray.cpp:303-310`）每帧把随身剑垒位置强制同步到 owner，实际净效果可忽略；但这是设计 §2.2.2 未明写的次序变化，仅记录、不阻断。

### (b) `PhantomTrance.cpp::ResolveParams` 契约

- 调用者枚举：`ResolveParams` 为匿名命名空间（`PhantomTrance.cpp:80`，`} // namespace` 在 725 行）内的静态函数，**唯一调用点**为 `PhantomTrance::DoCast`（`PhantomTrance.cpp:735`）。无跨 TU 调用者。
- 契约维持：`PhantomTrance.cpp:104-108` 在 `ResolveBakedProfile` 返回 `nullptr` 时返回 `static const PhantomTranceParams kFallbackParams{}`，调用方 735 行后的所有解引用（`params.blink`、`params.duration_sec`，并整体拷贝进 `pt.params`）均安全。
- 函数内 `static const` 安全性：C++11 起局部静态初始化线程安全（magic statics），对象为常量、生命周期至进程结束，返回 `const&` 无悬垂。
- 数值等价：旧实现 step-2 恒返回 `&scratch`，技能表缺失时 `scratch` 为哨兵（`BakedSkillProfile{}`），其 `delivery.trance` 恰为默认构造值；新回退值 `PhantomTranceParams{}` 与之逐字段相同。**该修改不改变哨兵场景的实际形态参数，只消除空指针解引用风险。**

### (c) `BeamChannelDeliverySystem.cpp` 归一化

- `BeamChannelDeliverySystem.cpp:158-161` 在 `GetBakedSkillProfile` 之后将非 `is_baked` 的档案一次性归一为 `nullptr`；后续 `effective_tags`（168）、`radius`（233）、`more_damage_mult`、`bonus_crit`、`bonus_crit_damage` 及捕获了 `profile` 的 lambda 全部消费归一结果。合法烘焙档案 `is_baked == true` → 指针不变，行为零变化（`BakedSkillProfile::operator==` 为 defaulted，`is_baked` 参与相等比较，见 `SkillDefs.hpp:701`）。
- 仍然遗漏：`ResolveSkill7Element`（`BeamChannelDeliverySystem.cpp:72-76`）自行调用 `GetBakedSkillProfile` 且不判 `is_baked`（见 F3）。
- 附带行为变化（符合预期）：哨兵 `BakedDeliveryParams::range` 默认 200.0f，归一后改走机制表 `base_range`（350.0f / 节点 703 缩放），不再被 200.0f 误钳。

### (d) `TestCommon.hpp` 快照/还原

| 场景 | 结果 |
|---|---|
| 快照时不存在 → 用例创建 → 删除 | ✅ `TestCommon.hpp:100-102` 保持 `false`，`:129-133` 删除 |
| 快照时存在 → 用例覆盖 → 还原 | ✅ `:134-141` 比对不等 → `:144-158` 写回快照字节 |
| 快照时存在 → 未修改 → 不重写 | ✅ `:139-141` 相等即 `return`，mtime 不变 |
| 读截断/失败 → 放弃快照 | ❌ **`:105-107`/`:109-112`/`:117-120` 放弃时 `m_settingsFileExisted` 仍为 `false`，析构 `:129-133` 会删除真实 `settings.json`**（F1） |
| 写失败 → 告警 | ✅ `:145-150`、`:154-158` 双分支 `DOCTEST_WARN_MESSAGE` |
| 用例删除文件 | ✅ `:136` 打不开 → 空字节 ≠ 快照 → 写回快照 |
| 目录只读 | ⚠ 写失败走告警，不崩溃，但磁盘上文件已被截断为半截（`trunc` 已打开），属无更好退路的已知限制 |

### (e) 新增/扩展用例的可证伪性（静态推理，未运行）

| 用例 | 能否证伪修复前实现 |
|---|---|
| `SwordArrayNodes.cpp:241-268`「675 Relocate clamps out-of-range target」期望 `x≈400` | ✅ 旧代码写 `exec.target_pos` = 10000，必失败 |
| `SwordArrayNodes.cpp:309-337`「603 cast range parity and clamp」远端 `x≈560` | ❌ 旧创建路径已钳制，属 parity lock；且与单测 `SkillWrapupHardeningTests` 560f 子用例重复（F4） |
| `SwordArrayNodes.cpp:736-750`「653 mobile aura ignores out-of-range target」 | ❌ 旧 mobile aura 亦覆盖为施法者，属回归锁（用例注释已如实说明） |
| `SkillWrapupHardeningTests.cpp` 节点 1015 `stat_modifiers.empty()` | ✅ 修复前 `mastery_skill_trees.json` 该数组非空，必失败 |
| `SkillWrapupHardeningTests.cpp`「ResolveBeamChannelMaxRange branches」哨兵分支 | ✅ 修复前 helper 无 `is_baked` 判据，会返回 420 而非回退 350 |
| `SkillWrapupHardeningTests.cpp` 半径 56/68/100、flat 时长 0.59f | ✅ 独立字面量期望，不与生产常量同源 |
| `SkillProfileResolveSentinelTests.cpp` Rebake 哨兵后 `ResolveBakedProfile == nullptr` | ✅ 旧实现 step-1 按 `skill_id` 命中即返回哨兵 |
| step-2 / step-3 用例 | ✅ 旧实现返回 `&scratch`（非空），必失败 |
| `TestCommon.hpp` 快照/还原 | ⚠ **无任何测试覆盖**，F1 因而未被发现 |

### (f) `SevenStarSlashShared.hpp` 引入 Baker 的 ODR/命名空间

- 无头文件循环：`SkillSystem.hpp` **不**包含 `SkillSpecializationBaker.hpp`（仅 `SkillSystem.cpp:58` 包含），且 Baker `.cpp` 在 6 行早已包含 `SkillSystem.hpp`，新 include 只是传递性重复，`#pragma once` 处理。
- 无 ODR 问题：`inline constexpr float kSevenStarSlashBaseRadius`（`SevenStarSlashShared.hpp:29`）跨 TU 唯一定义；Baker 在 `namespace NoMoreDay` 内以 `skills::seven_star_shared::...` 限定访问（`SkillSpecializationBaker.cpp:140`），解析正确。
- 唯一代价是编译期 TU 膨胀（见 F6），非正确性问题。

### (g) 未做/部分完成项

- 发现 3、6、8 为部分修复（F1/F2/F3/F4）。
- 发现 6 的归一化与发现 7 的回退路径**均无直接回归测试**（仅间接覆盖），列为剩余风险。

## 5. 发现项

### F1（High，阻断提交）快照放弃路径会删除真实的 `settings.json`

- 位置：`tests/TestCommon.hpp:98-133`（快照/还原新增块全部为本次增量，见 `git diff`）。
- 问题：`SnapshotSettingsFile` 的三条「保守放弃」分支——`file_size` 失败（`:105-107`）、`ifstream` 打不开（`:109-112`）、读截断/短读（`:117-120`）——都只是 `return`，`m_settingsFileExisted` 保持 `false`。而 `RestoreSettingsFileIfChanged` 把 `false` 解释为「快照时文件本就不存在」，在 `:129-133` 执行 `std::filesystem::remove(kSettingsFilePath, ec)`。于是：**只要快照阶段发生任何 I/O 异常，析构就会静默删除磁盘上真实存在的 `settings.json`**（此时快照甚至没有成功，用例可能根本没碰过该文件）。三条分支的注释（`:106`「避免析构时误判内容已变」、`:110-111`「避免用残缺内容覆盖原文件」、`:115-117`）明确表达了相反意图，代码与注释自相矛盾。这也是发现 #3 与 #9 各自正确、组合后互相破坏的典型回归。
- 触发可达性：需要 I/O 失败或读竞争（例如文件被占用、权限受限、异常盘）。概率不高但后果为静默数据丢失，且发生在仓库受版本控制的文件上。
- 具体修正：引入三态，例如

  ```cpp
  enum class SettingsSnapshot { Absent, Present, Unavailable };
  SettingsSnapshot m_settingsSnapshot = SettingsSnapshot::Absent;
  ```

  - `exists()` 为假 → `Absent`；
  - `file_size` 失败 / 打不开 / 读截断 → `Unavailable`；
  - 读成功 → `Present` + 字节。
  - `RestoreSettingsFileIfChanged()`：`Present` 走现有还原逻辑；`Absent` 才 `remove`；`Unavailable` **直接 return，绝不触碰文件**（可选：`DOCTEST_WARN_MESSAGE(false, "settings.json snapshot unavailable; left untouched")`）。

### F2（Medium，部分修复 #3）清理失败被静默吞掉

- 位置：`tests/TestCommon.hpp:130-131`。
- 问题：`std::filesystem::remove(kSettingsFilePath, ec)` 的 `std::error_code ec` 被丢弃，删除失败（目录只读、文件被占用）时既不告警也不留痕，仓库根会静默残留未跟踪的 `settings.json`。发现 #3 只补了「删除动作」，未补「失败可观测性」，与 #4 对写入路径的处理标准不一致。
- 具体修正：`if (ec) { DOCTEST_WARN_MESSAGE(false, "settings.json cleanup failed: " << ec.message()); }`。

### F3（Medium，部分修复 #6）`ResolveSkill7Element` 仍消费未过滤哨兵档案

- 位置：`src/game/systems/skill/BeamChannelDeliverySystem.cpp:72-76`。
- 问题：该辅助函数自行调用 `SkillSystem::GetBakedSkillProfile(registry, caster, 7u)` 并直接读 `profile->effective_tags`，不判 `is_baked`；`UpdateMindBladeBeam` 在 164 行调用它，因此哨兵档案仍可经此路径参与元素判定。当前**恰好无害**，因为 `BakedSkillProfile::effective_tags` 默认值 `Tag::None`（`SkillDefs.hpp:682`），`HasTag(Tag::None, ...)` 恒假、会落入 `beam->conversion_tag` 回退——但这与 `:154-158` 注释宣称的「哨兵默认值不会渗入结算」并不成立为不变量；一旦后续哨兵填充任何标签即成为真实漏洞。review.md 建议的「统一走 `ResolveBakedProfile`」也未被采用。
- 具体修正：把 `ResolveSkill7Element` 改为接收已归一的 `const BakedSkillProfile *profile` 参数（`UpdateMindBladeBeam` 内已持有），或在 `:73` 追加 `profile->is_baked` 判据。

### F4（Low，部分修复 #8）603 远端断言与单测重复，非独立证据

- 位置：`tests/functional/SwordArrayNodes.cpp:327`（`CHECK(clampedPos.x == doctest::Approx(560.0f))`）。
- 问题：发现 #8 要求消除与单测重复的钳制断言；本次删除了旧重复后，又在「603 cast range parity and clamp behavior」子用例中新增了等价的 560f 远端断言，而 `tests/unit/SkillWrapupHardeningTests.cpp` 的「Skill 6 cast offset clamps to delivery range」已断言同一 560f。该断言对修复前实现也不会失败（创建路径本就钳制），因此不是发现 #1 的独立证据，重复问题只是换位而非消除。
- 具体修正：功能测试保留「默认 400 / 未点 603」的 parity 断言即可，删除 560f 远端断言（或明确注释其为双保险）；发现 #1 的唯一独立证据是 `:241-268` 的 675 挪阵子用例。

### F5（Low，偏差披露）mobile aura 覆盖顺序扩展到挪阵分支

- 位置：`src/game/systems/skill/behaviors/SwordArray.cpp:88-105`。
- 问题：新代码在挪阵分支之前统一计算 `cast_target`，使 `is_mobile_aura` 的「落点恒为施法者」覆盖**也作用于 675 挪阵**；旧实现挪阵直写原始目标点（等价于覆盖不生效）。设计 §2.2.2 只描述施法落点钳制，未声明挪阵的分支次序变化。
- 影响评估：随身剑垒在 `SwordArray.cpp:303-310` 每帧强制同步到 owner 位置，实际净效果可忽略，判为可接受偏差；但应按项目规则显式记录。
- 具体修正：在 `:88` 附近补一句中文注释说明「挪阵分支同样采用单源落点，mobile aura 下等于回到施法者（与每帧跟随一致）」，或新增一个「mobile aura + 675 挪阵」子用例固化该语义。

### F6（Low）为一个浮点常量把重型头引入 Baker TU

- 位置：`src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:29` → `src/game/systems/skill/SkillSpecializationBaker.cpp:11`。
- 问题：`SevenStarSlashShared.hpp` 会带入 `SkillSystem.hpp`、`SkillRegistry.hpp`、`Buff.hpp`、`SevenStarSlashSpecState.gen.hpp` 等；Baker 仅为读取一个 `constexpr float` 而承担这些依赖。无正确性/ODR 缺陷（§4f），属可维护性与增量编译成本。
- 具体修正：比照本次新建的 `BeamChannelShared.hpp` / `SwordArrayShared.hpp`，把常量放入一个只含 `<cstdint>` 的轻量常量头，行为与 Baker 同时包含该头。

### F7（BestPractice）`build.bat` 新增注释冗余

- 位置：`build.bat:313-317`。
- 问题：5 行 REM 中「Skill gates must run before the runtime binary is generated...」与「Both steps below are read-only gates over canonical / skill_mechanics source data.」语义重叠，可读性下降。
- 具体修正：合并为 2 行（一条说明门禁位置原因，一条说明只读语义）。

### F8（Low）新增文件未纳入版本控制

- 位置：`src/game/systems/skill/behaviors/BeamChannelShared.hpp`、`src/game/systems/skill/behaviors/SwordArrayShared.hpp`、`tests/unit/SkillProfileResolveSentinelTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`（`git status` 显示 `??`）。
- 问题：这 4 个文件是本次实现与修复的必要组成，但处于 untracked 状态；若提交时使用 `git add -u` 会整体遗漏，导致构建/测试失败。
- 具体修正：提交时显式 `git add` 这些路径（连同 design/plan/review 文档）。

## 6. 最佳实践建议

1. 修复 F1 后，为 `TestSetupScope` 补一个最小单元测试（在临时目录注入 I/O 失败或直接构造 `Unavailable` 状态），否则该类夹具缺陷仍会静默复发。
2. `BakedSkillProfile::is_baked` 已成为消费契约，建议在 `ResolveBakedProfile.hpp` 文档注释中把「唯一合法生产者 = `SkillSpecializationBaker::Bake`（`SkillSpecializationBaker.cpp:345`）」写明，并标注 `RebakeSkillProfiles`（`SkillSystem.cpp:2708-2740`）写入的哨兵为非法消费样本。
3. `GetBakedSkillProfile` 直连点（设计 §2.1.2 记录的约 30 处）建议增加静态扫描清单，防止新增直连点绕过 `is_baked` 判定（F3 即为实例）。

## 7. 剩余风险

- F1 未修前，任何一次测试进程内的 I/O 异常都可能导致开发者本地 `settings.json` 被删除；远端 CI（快照时通常不存在该文件）不受影响。
- 发现 #6 的入口归一与发现 #7 的回退分支无直接回归测试，属「有防御、无证据」。
- `build.bat:318-321` 新增的两个 `--check` 门禁（`scripts/validate_skill_spec_modifiers.py --check`、`scripts/gen_skill_mechanics_schema.py --check`）与手工改写的 `assets/data/skill_mechanics_schema.json:2193`（`dynamic_keys` 新增 `base_range`）**本次未执行验证**（只读审查禁止构建）。静态核对：脚本均支持 `--check`（`gen_skill_mechanics_schema.py:488`、`validate_skill_spec_modifiers.py:497`），且 `dynamic_keys` 按 `sorted()` 输出（`gen_skill_mechanics_schema.py:474`），`base_range` 排在 `haste_pct_per_point` 之前符合字典序，新增动态键来源为 `BeamChannelShared.hpp:26` 的参数化 `beamSkillId`；但仍需在提交前本地跑一次门禁确认。
- `BakedSkillProfile` 哨兵仍被 UI 展示路径遍历（`GameUiSnapshotBuilder.cpp:406`、`PlayerHUD.cpp:144`），本次未加 `is_baked` 过滤；因哨兵仅在技能表缺失时出现，列为既有残余。

## 8. 下一步动作

1. 按 F1 引入三态快照状态，修正 `TestCommon.hpp:98-133`；同时修 F2。
2. 按 F3 收敛 `ResolveSkill7Element` 的档案读取。
3. 删除/降级 `SwordArrayNodes.cpp:327` 的重复断言（F4），补 F5 的注释或子用例。
4. 提交前执行 `validate_skill_spec_modifiers.py --check` 与 `gen_skill_mechanics_schema.py --check`，并 `git add` F8 的未跟踪文件。
5. 其余为建议项，不阻断。

结论：修改

---

## 附：整改回应（2026-09-17，非审查结论）

> 本节由实施方追加，记录对 F1–F8 的处理与验证证据；上文第 5–8 节的审查结论保持不变，作为第五轮的历史记录。

| 项 | 处置 | 落点 |
|---|---|---|
| F1（High） | **已修**。`TestCommon.hpp` 引入三态：`m_settingsFileExisted` 在确认文件存在后立即置位，三条放弃分支（`file_size` 失败 / 打开失败 / `in.bad()` 或长度不符）统一置 `m_settingsSnapshotAbandoned`；析构先判 `!m_settingsFileExisted`（删产物）再判 `m_settingsSnapshotAbandoned`（仅告警、绝不删改），真实文件不再被当作用例产物删除 | `tests/TestCommon.hpp:97-146` |
| F2（Medium） | **已修**。`remove(kSettingsFilePath, ec)` 补 `if (ec) DOCTEST_WARN_MESSAGE(...)`，清理失败不再静默 | `tests/TestCommon.hpp:129-137` |
| F3（Medium） | **已修**。`ResolveSkill7Element` 的档案读取补 `profile->is_baked` 过滤（与主路径归一化语义一致），并注明哨兵 `effective_tags` 为默认空集、语义上不可消费 | `src/game/systems/skill/BeamChannelDeliverySystem.cpp:71-82` |
| F4（Low） | **已修**。删除 `SwordArrayNodes.cpp` 中与单测重复的 560 行为钳制断言（该断言对修复前实现不失败，非独立证据），子用例更名 `603 cast range parity`，注释显式指向单测与 675 子用例 | `tests/functional/SwordArrayNodes.cpp:305-330` |
| F5（Low） | **已修**。design §2.2.2 补「落点单源（v1.3 修正）」与 mobile-aura 语义扩展到挪阵分支的说明及其可忽略净影响的理由；测例覆盖由三种扩展为四种（新增 675 挪阵超距钳制） | `docs/designs/2026-09-17-...-design.md:169-171` |
| F6（Low） | **已修**。半径常量析出为零依赖头 `SevenStarSlashConstants.hpp`，`SevenStarSlashShared.hpp` 与 `SkillSpecializationBaker.cpp` 均改为包含该头，烘焙 TU 不再拖入行为层重型头 | `src/game/systems/skill/behaviors/SevenStarSlashConstants.hpp`（新增） |
| F7（BestPractice） | **已修**。5 行语义重叠 REM 精简为 3 行 | `build.bat:313-315` |
| F8（Low） | **已登记**。8 个未跟踪文件（含本轮新增的 `SevenStarSlashConstants.hpp`）须显式 `git add`，禁止 `git add -u`；见 `docs/plans/2026-09-17-...-plan.md` 文末登记 | 计划文档登记 |

### 验证证据（本次全部实跑）

- `build.bat RelWithDebInfo` → `EXIT=0`，`warning C` / `error C` / `error LNK` 均 0 命中；新增两道门禁在日志中显示 `[Build] OK: Validating skill spec modifiers.`、`[Build] OK: Checking skill mechanics schema.`，且位于 `[Build] OK: Generating modifier runtime binary.` 之前。
- 定向用例（`*SkillWrapup*`、`*SkillProfileResolve*`、`*SkillBatch4*`、`*Skill 6*`、`*Skill 10*`、`*PhantomTrance*`）→ **80 用例 / 1184 断言全过**。
- `ctest --test-dir build -C RelWithDebInfo -L unit` → 8/8；`-L integration` → 6/6；`-R nmd.tests.ci.nonperf` → 1/1。
- 第 7 节列出的未执行门禁已实跑：`validate_skill_spec_modifiers.py --check`（`6/6 passed`）、`gen_skill_mechanics_schema.py --check`（`437 entries, 62 unreferenced`）、`gen_skill_contracts.py --check`，均 `EXIT=0`。
- `settings.json` SHA256 = `6C446ED35D44A0F432F4072686E11DD74E1FB736D37D25685C23BB6074298485`，与整改前一致 → 夹具未污染工作区。

### 未采纳/未处理（保留为残余）

- 第 6 节最佳实践 1（为 `TestSetupScope` 注入 I/O 失败的最小单测）：当前三态逻辑依赖文件系统故障才有分支差异，构造该单测需可注入的 I/O 抽象层，属超出本包范围的独立课题，登记为跟催项。
- 第 6 节最佳实践 2/3（`is_baked` 生产者契约文档、`GetBakedSkillProfile` 直连点静态清单）：与 F-02 同属独立整改，未在本轮展开。
- 第 7 节残余（UI 展示路径 `GameUiSnapshotBuilder.cpp:406`、`PlayerHUD.cpp:144` 仍遍历哨兵档案）：维持既有残余判定，不在本包边界内。
