# UMR Batch 4（技能 10/11/12）第 4 轮审查修复验证报告 — 独立 C++ 复核

- **审查轮次**：第 4 轮（对 `review.md` 10 项发现修复后的**只读验证**）
- **审查角色**：独立审查者（未参与实现，未修改任何文件）
- **HEAD**：`3f770c0c`（工作区未提交）
- **结论**：**提交**（代码缺陷 0；F11 文档同步已完成，全部发现关闭）
- **审查日期**：2026-09-17（第 4 轮验证 + 第 5/6 轮 F11 文档修订复审）

## 1. 审查输入与方法

| 类别 | 文件 |
| --- | --- |
| 规则 | `AGENTS.md`、`docs/workflows/review.md` |
| 设计 | `docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md` |
| 计划 | `docs/plans/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-plan.md` |
| 代码 | `src/game/systems/skill/SkillSpecializationBaker.cpp`、`behaviors/{SevenStarSlash,HeavenlySwordDescent,BloodSea}.cpp`、`SkillProfileResolve.{hpp,cpp}`、`SkillSystem.cpp`、`foundation/components/SkillDefs.hpp`、`foundation/data/SkillRegistry.hpp`、`SpecStateTable.hpp` |
| 测试 | `tests/unit/SkillSpecializationBakerTests.cpp`、`tests/functional/{HeavenlySwordDescentNodes,BloodSeaNodes}.cpp`、`tests/TestCommon.hpp` |

**方法**：逐点阅读当前工作区源码，与 10 项发现、设计 §1.3/§3.2/§3.3/§6、计划 §2.1/§2.2/Task 4.2 交叉比对；对 JSON 键、字段消费者、符号存在性执行只读检索。

**未复跑项（承认为未由本审查独立验证的外部证据）**：`build.bat RelWithDebInfo`、`bin\NoMoreDayTests.exe`（19/19 与 1761/1761）、`ctest -L unit` 8/8、`validate_skill_spec_modifiers.py` 6/6、`SkillSpecBatch4GateTest.py` 20/20。本审查结论全部来自静态代码核对；上述执行结果作为用户提供的证据引用，标记为“未独立复跑”。

## 2. 逐项复核结论

| 编号 | 严重度 | 结论 | 一句话结论 |
| --- | --- | --- | --- |
| F1 | 中 | **关闭** | `ApplyNodeModifiersToProfile()` 内已无 `case 10/11/12`，三者落入 `default: break;`，行为不变 |
| F2 | 中 | **关闭** | 技能 10 `area_radius` 死写已按设计 §1.3 非目标 7 显式登记为“统一基准”，且文件内注释与代码一致 |
| F3 | 中 | **关闭** | 4 键存在性守卫齐全、键名正确、位于被保护断言之前，且 JSON 中确实存在 |
| F4 | 高 | **关闭** | BloodSea 时长/半径均以 `profile && skill != nullptr` 排除哨兵；回退表达式不空解引用 |
| F5 | 低 | **关闭** | `pressureTideRisePoints` / `lingeringBloodMistPoints` 已登记为刻意保留（设计 §1.3 非目标 6） |
| F6 | 高 | **关闭** | HeavenlySword 半径守卫含 `skill != nullptr`（比值不再坍缩），时长守卫同源 |
| F7 | 低 | **关闭** | `celestialDomainPoints` / `enduringHeavenPoints` 已登记为保留 |
| F8 | 低 | **关闭** | `critChancePoints` / `voidTreadPoints` 已登记为保留 |
| F9 | 中 | **关闭** | `HeavenlySwordDescentNodes.cpp` 的 `EnsureSkillMechanics()` 已 `REQUIRE(ReloadModifierRuntimeFromAsset())` |
| F10 | 中 | **关闭** | `BloodSeaNodes.cpp` 同上 |
| F11 | 中 | **关闭** | **文档**：计划 §2.2 / Task 5.2 / Task 5.3 与设计 §3.3 第 1~4 条、§6 R-01 均已同步哨兵门与内层 `skill ?` 空守卫；全文件 `rg "skill->"` 无未守卫残留；技能 10 例外有代码不变量支撑 |
| F12 | 低 | 登记（非阻断） | 哨兵真值分支下 HeavenlySword 每点增量项被丢弃；与设计 R-01 的“基准回退”意图一致，但严格不等于迁移前整式 |
| F13 | 信息 | 登记（非阻断） | 陈旧缓存哨兵 + 技能数据事后可见时守卫仍会放行；静态资源下不可达 |

## 3. 详细证据

### F1（关闭）`ApplyNodeModifiersToProfile` 空分支清理

- `src/game/systems/skill/SkillSpecializationBaker.cpp:338` 为 `ApplyNodeModifiersToProfile` 定义起点。
- 该 switch 现有分支为 `case 1..9`，`case 9` 于 `:1063 break; :1064 }`，随后仅：

  ```cpp
  default:      // :1066
    break;      // :1067
  ```

- 全文件 `case 10/11/12` 仅出现在 `:130/:137/:142`，均属 `Bake()` 步骤 1 的基础基准写入 switch（非 `ApplyNodeModifiersToProfile`）。
- 原“空分支”与 `default` 行为等价，删除后语义不变；与 `case 5` 同 switch 内“不留空分支”惯例一致。设计 §3.2 步骤 2（`design:214`）与计划 Task 4.2（`plan:214`）、计划 §2.1 步骤 2 伪代码（`plan:89-95`）均已写明“不新增分支，落入既有 `default: break;`”。

### F2（关闭）技能 10 `area_radius` 死写的文档化

- `SkillSpecializationBaker.cpp:130-136`：`case 10` 写入 `out_profile.area_radius = skillData->GetParam("radius", 96.0f)`，注释 `:131-133` 说明“技能 10 行为层不消费 `area_radius`，此处写入仅为交付基准一致性，非死写数值权威”。
- 设计 §1.3 非目标 7（`design:120`）明确：“…对技能 10 无行为层读者，仅为与技能 11/12 保持…一致基准，**不得据此认为它是七星斩的判定半径**（实际命中半径由行为层以 `baseRadius * 0.28/0.34/0.50` 派生）”。
- 代码核实该派生属实：`SevenStarSlash.cpp:417` 直读 `skillData->GetParam("radius", 96.0f)`（未读 `profile->area_radius`）；`:456` `0.50f/0.28f`、`:463` `0.34f`。三处比值与设计描述一致。**文档存在且准确**。

### F3（关闭）快照来源守卫

- `tests/unit/SkillSpecializationBakerTests.cpp:2295` 起为 Batch4 快照用例；`:2307-2317` 依次：

  | 行 | 守卫 |
  | --- | --- |
  | `:2308` | `skill10->params.count("radius") == 1` |
  | `:2309` | `skill10->params.count("invulnerable_duration") == 1` |
  | `:2312` | `skill11->params.count("field_radius") == 1` |
  | `:2313` | `skill11->params.count("field_duration") == 1` |
  | `:2316` | `skill12->params.count("field_radius") == 1` |
  | `:2317` | `skill12->params.count("field_duration") == 1` |

- `SkillData::params` 为 `std::unordered_map<std::string,float>`（`SkillRegistry.hpp:25`），`count()` 缺键返回 0 → 守卫非空转（非 vacuous）。守卫位于所有 `Bake()` 断言之前。
- JSON 核实（脚本读取 `assets/data/skills.json`）：技能 10 params = `{flow_bonus_per_stack, invulnerable_duration, radius, single_target_execute_bonus}`；技能 11 = `{field_duration, field_radius, impact_radius, tier_damage_bonus, tier_radius_bonus}`；技能 12 = `{bloodthirst_damage_bonus, field_duration, field_radius, field_tick, leech_ratio}`。6 个受守卫键全部存在 → 守卫不会误报。

### F4（关闭）`BloodSea::DoCast` 哨兵守卫

- `BloodSea.cpp:305-306` 取 `profile`；`:409` 取 `skill`（在守卫之前，作用域内）。
- 时长（`:430-435`）：`((profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationDefault) : kFieldDurationDefault)) + field_duration_per_bloodthirst * effective_consumed`。
- 半径（`:439-445`）：同样的 `profile && skill != nullptr` 守卫，else 分支为 `skill ? skill->GetParam("field_radius", …) : kFieldRadiusDefault`，随后保留 `consumed * field_radius_per_bloodthirst + bloodCurtainOpeningPoints * radius_per_point`。
- **无空解引用**：`skill->GetParam` 被内层 `skill ?` 包裹。
- **skill == nullptr 分支等价迁移前**：迁移前该点即 `skill ? GetParam : default` 再叠加行为层项，现回退表达式的数值构成与之一致。
- 哨兵成因核实：`SkillSpecializationBaker.cpp:26-31`（`!skillData` 时仅写 `skill_id` 并早退）、`SkillDefs.hpp:631`（`delivery.duration = 1.0f`）、`:678`（`area_radius = 1.0f`）、`SkillSystem.cpp:2736-2742`（同一哨兵写入 `baked_profiles`）、`:2773-2777`（按 `skill_id` 命中即返回，不校验有效性）。哨兵 `more_damage_mult` 默认 1.0（`:680`），故 `:494` 的 `profile ? profile->more_damage_mult : 1.0f` 无需额外守卫即可等价。
- `Radius_per_point`（`:315-317`）保留并仍被使用（`:445`），删除的 `damage_per_point` / `duration_per_point` / `bottomless_damage_mult` 在 `BloodSea.cpp` 中已无残留引用。

### F5（关闭）1201 / 1219 保留项登记

- 设计 §1.3 非目标 6（`design:119`）显式列出 `BloodSeaCastSpecGen::pressureTideRisePoints` / `lingeringBloodMistPoints` 为“本批刻意保留”。全仓检索确认二者仅剩生成绑定（`BloodSeaSpecState.gen.hpp:48,62,76,90`）与测试引用（`BloodSeaNodes.cpp:112-113`、`GeneratedSpecStateTests.cpp:221,235`），生产读者已删除。

### F6（关闭）`HeavenlySwordDescent::DoCast` 哨兵守卫

- `HeavenlySwordDescent.cpp:490` 取 `profile`；`:552` 取 `skill`；`:556-558` 取 `base_field_radius`；`:603-609`：

  ```cpp
  float field_radius = base_field_radius + spent_tiers * tier_radius_bonus;
  const float areaMult =
      (skill != nullptr && profile && base_field_radius > 0.0f)
          ? profile->area_radius / base_field_radius : 1.0f;
  field_radius *= areaMult;
  ```

  守卫含 `skill != nullptr`，故 `skillData` 缺失时 `areaMult == 1.0f`，**不会**把半径压到 ~1（这正是第 3 轮 F2' 的失效点，现已修正）。
- 时长（`:655-659`）：`(profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationFallback) : kFieldDurationFallback)`。else 分支有空守卫。
- `1107` 半径惩罚改为经 UMR `2011070` 承接；`sky_piercing_damage_mult`（`:511`）在 `:612` 恰被使用一次（与 HEAD 一致，无重复乘算）：`git show HEAD` 原块同样为 `*= range_mult; impact_damage_mult *= 1.0f + damage_mult;`，迁移只删去半径行。
- 删除变量 `celestial_domain_per_point` / `sky_piercing_range_mult` / `enduring_heaven_per_point` / `base_field_duration` 无残留引用。

### F7（关闭）/ F8（关闭）天剑与七星保留项登记

- 设计 §1.3 非目标 6（`design:119`）同条列出 `HeavenlySwordDescentCastSpecGen::celestialDomainPoints`/`enduringHeavenPoints` 与 `SevenStarSlashSpecStateGen::critChancePoints`/`voidTreadPoints`。
- 生产侧删除核实：`SevenStarSlash.cpp` 已移除 `critChancePerPoint`（原 `:368`）与 `voidTreadDurationPerPoint`（原 `:380`），改为 `:424-427` 消费 `profile->delivery.duration`、`:555` 消费 `profile->delivery.bonus_crit`。检索确认 6 个字段现仅有生成绑定与测试引用，无生产读者。

### F9（关闭）`HeavenlySwordDescentNodes.cpp` 隔离

- `tests/functional/HeavenlySwordDescentNodes.cpp:12` `#include "TestCommon.hpp"`；`:36-46` `EnsureSkillMechanics()` 在 `SkillRegistry::Get().LoadFromJson(...)` 之后、`SkillBehaviorRegistry::Initialize()` 之前插入 `:44 REQUIRE(ReloadModifierRuntimeFromAsset());`。
- `ReloadModifierRuntimeFromAsset()` 定义于 `tests/TestCommon.hpp:25-45`：返回 `bool`，缺失资产/解析失败时返回 `false`（`REQUIRE` 会失败），内部执行 `ModifierRuntimeRegistry::Reload("assets/generated/modifier_runtime_v2.bin")`，正是“从真实生成资产强制重载”的语义。返回值为检查式调用（非忽略）。

### F10（关闭）`BloodSeaNodes.cpp` 隔离

- `tests/functional/BloodSeaNodes.cpp:12` include、`:38-48` `EnsureSkillMechanics()`，`:46 REQUIRE(ReloadModifierRuntimeFromAsset());`，位置与 F9 完全一致。

### 文档一致性专项核对

| 要求 | 结果 | 证据 |
| --- | --- | --- |
| 设计 §3.2 步骤 2 写明不新增显式分支 | ✅ | `design:214`“**不**为技能 10/11/12 新增分支，三者继续落入既有 `default: break;`” |
| 设计 §3.3 第 3/4 条展示 `skill != nullptr` 守卫 | ✅ | `design:246,260,261,266,268` |
| 设计 §6 R-01 提及哨兵档案 | ✅ | `design:384`“默认值为正（`duration=1.0f` / `area_radius=1.0f`）的**哨兵档案**…无法用数值判据区分” |
| 计划 Task 4.2 写明“不新增 `case 10/11/12`” | ✅ | `plan:214` |
| 计划 §2.1 步骤 2 伪代码块不新增分支 | ✅ | `plan:89-95` |
| 计划 §2.2 哨兵守卫通则段落 | ✅ 第 5 轮新增 | `plan:101` |
| 计划 §2.2 天剑伪代码与代码一致 | ✅ 第 5 轮修正 | `plan:131-140` ≡ `HeavenlySwordDescent.cpp:605-608,655-659` |
| 计划 §2.2 血海伪代码与代码一致 | ✅ 第 5 轮修正 | `plan:158-171` ≡ `BloodSea.cpp:430-435,439-445` |
| 计划 Task 5.2 / 5.3 单源消费行 | ✅ 第 6 轮修正 | `plan:232`、`plan:242` |
| 设计 §3.3 第 3/4 条主表达式 `skill != nullptr` 守卫 | ✅ | `design:260,261,266,268` |
| 设计 §3.3 空档案回退基准子项 | ✅ 第 6 轮修正 | `design:251,252` |
| 设计 §3.3 第 1 条技能 10 例外 | ✅ 第 6 轮新增 | `design:247` |
| 设计 §6 R-01 与 §3.3 第 2/3/4 条一致性 | ✅ | R-01（`design:385`）引用 §3.3 第 1 条，技能 10 例外已在该节内声明 |
| 全文件未守卫 `skill->` 残留 | ✅ 无（`rg` 复核） | `plan`/`design` 每处 `skill->` 均处于 `skill ?` 或 `skill != nullptr` 内 |

## 4. 新发现问题

### F11（中，文档一致性）哨兵守卫文档同步 —— **关闭（第 6 轮完成）**

#### (a) 计划 §2.2 主伪代码现已与实现逐字一致 —— **通过**

| 站点 | 计划伪代码 | 实现代码 | 结果 |
| --- | --- | --- | --- |
| 天剑半径 | `plan:131-134` `(skill != nullptr && profile && base_field_radius > 0.0f) ? (profile->area_radius / base_field_radius) : 1.0f` | `HeavenlySwordDescent.cpp:605-608` | ✅ 逐字等价 |
| 天剑时长 | `plan:137-140` `(profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationFallback) : kFieldDurationFallback)` | `HeavenlySwordDescent.cpp:655-659` | ✅ 逐字等价 |
| 血海时长 | `plan:158-162` 同构（含 `+ field_duration_per_bloodthirst * effective_consumed`） | `BloodSea.cpp:430-435` | ✅ 逐字等价 |
| 血海半径 | `plan:166-171` 同构（含 `+ consumed*field_radius_per_bloodthirst + bloodCurtainOpeningPoints*radius_per_point`） | `BloodSea.cpp:439-445` | ✅ 逐字等价 |
| 哨兵守卫通则 | `plan:101` 新增段落，给出规范式与“回退分支的 `skill->GetParam` 必须包在 `skill ?` 内” | — | ✅ |

#### (b) 未守卫 `skill->` 残留 —— **已清空（通过）**

- 全文件 `rg "skill->"` 复核：`plan` / `design` 中每一处 `skill->` 均处于 `skill ?` 或 `skill != nullptr` 之内，或属于守卫通则段落本身的示例文本。
- `plan:232`（Task 5.2）：已改为 `(skill != nullptr && profile && base_field_radius > 0.0f) ? … : 1.0f`，时长改为 `(profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationFallback) : kFieldDurationFallback);` ✅
- `plan:242`（Task 5.3）：duration/radius 基础部分均为 `(profile && skill != nullptr) ? profile->X : (skill ? skill->GetParam(...) : kDefault)`；并附注 `more_damage_mult` 在任何分支都是有效数值（哨兵 `1.0f` 与回退基准相同），无需 `skill` 门 ✅
- `design:251`（技能 11 基准）与 `design:252`（技能 12 基准）：均改为 `skill ? skill->GetParam(...) : <default>` 并注明「`skill ?` 守卫避免空解引用」✅
- `design:260,261,266,268`（§3.3 第 3/4 条主表达式）：else 分支均含内层 `skill ?` ✅

#### (c) 设计 §3.3 第 2/3/4 条 与 §6 R-01 一致性 —— **一致（通过）**

- 第 3 条（`design:260-261`）、第 4 条（`design:266,268`）与 R-01（`design:385`）的「`profile && skill != nullptr` 排除哨兵 + 回退分支 `skill ?`」口径**一致** ✅。
- 技能 10 例外已显式声明：`design:247`「技能 10 以 `skillData` 取参、且入口已对空 `skillData` 早退，不受此条约束」。R-01（`design:385`）引用「§3.3 第 1 条哨兵守卫」，而例外即含于该条内，故 R-01 与 §3.3 第 1/2 条自洽 ✅。
- **例外有代码支撑**：`src/game/systems/skill/behaviors/SevenStarSlash.cpp:356`

  ```cpp
  const auto *skillData = SkillRegistry::Get().GetSkill(kSkillId);
  ```

  与 `:359-361`：

  ```cpp
  if (skillData == nullptr || ownerPos == nullptr || ownerStats == nullptr) {
    return;
  }
  ```

  即 `DoCast` 在任何消费点之前对空 `skillData` 早退 → 后续 `profile ? … : skillData->GetParam(...)`（`design:255`，实现于 `SevenStarSlash.cpp:424-427`）不会空解引用，且哨兵档案在该路径不可达。第 1 条子项（`design:250`）技能 10 基准写作 `skillData->GetParam(...)`，与第 2 条自洽 ✅。

#### 第 5/6 轮修订记录（已核对）

1. `plan:232`（Task 5.2）：已改为 `(skill != nullptr && profile && base_field_radius > 0.0f) ? … : 1.0f` 与含内层 `skill ?` 的时长三元式 ✅
2. `plan:242`（Task 5.3）：duration/radius 基础部分已改为 `(profile && skill != nullptr) ? … : (skill ? skill->GetParam(...) : kDefault)`，并补充 `more_damage_mult` 无需 `skill` 门的说明（与实现 `BloodSea.cpp:494` 及哨兵默认值 `1.0f` 一致）✅
3. `design:251`、`design:252`：回退基准项已加内层 `skill ?` 与「避免空解引用」注记 ✅
4. `design:247`：技能 10 例外限定已补入，并由 `SevenStarSlash.cpp:356,359-361` 的空 `skillData` 早退支撑 ✅

### F12（低，行为边界，登记非阻断）哨兵真值分支下天剑每点增量被丢弃

- **文件**：`src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:605-609, 655-659`
- **说明**：当 `skill == nullptr`（`skillData` 缺失）且专精槽仍有点数时，`areaMult` 回退为 1.0、时长回退为 `kFieldDurationFallback`。迁移前同一分支仍会叠加 `celestial_domain_per_point * points`、`sky_piercing_range_mult` 与 `enduring_heaven_per_point * points` 三项。`ResolveSpecState`（`SpecStateTable.hpp:50-74`）只读专精槽、不读 `skillData`，故“点数 > 0 而技能数据缺失”在构造上可达（但资源静态加载下极不可能）。
- **判定**：设计 §3.3 第 1 条与 R-01 明确采用“哨兵永不生效、回退基准=原基准”的口径，故这是**有意的基准回退**而非实现错误；仅需知晓“严格等于迁移前整式”在该病态分支不成立。不阻断。
- （七星的 `skillData` 由 `SevenStarSlash.cpp:359-361` 早退保证非空，哨兵不可达，故 `profile ?` 单守卫足够；血海回退表达式与迁移前一致。）

### F13（信息，残留风险，登记非阻断）陈旧缓存哨兵

- **文件**：`SkillSystem.cpp:2766-2779`、`SkillDefs.hpp:671-696`
- **说明**：`GetBakedSkillProfile` 仅按 `skill_id` 命中返回，不校验档案有效性。若某槽位曾在 `skillData` 缺失时烘焙出哨兵并被缓存，之后技能数据变为可见但未触发再次 `RebakeSkillProfiles`，则 `skill != nullptr && profile` 守卫会放行哨兵（`area_radius == 1.0f`）→ 天剑半径仍会坍缩。当前资源为启动期静态加载、`skillData` 可见性稳定，实践不可达；登记为后续加固点（可在 `ResolveBakedProfile` 或守卫处增加 `profile->skill_id != 0`/哨兵指纹判别）。

## 5. 复审范围与风险声明

- 本审查为只读静态核对，**未**独立复跑构建与测试；所有“通过”类结论援引用户提供的执行证据，已在上文标记。若需强保证，建议提交前由维护者复跑 `bin\NoMoreDayTests.exe` 全量与 `ctest -C RelWithDebInfo -L unit`。
- F12/F13 为边界/残留登记，不影响正常路径行为等价性；本轮新增测试（`SkillBatch4DeliveryOpTests`、`SkillBatch4NullProfileFallbackTests`、`SkillBatch4BloodSeaOrderingTests`）与设计快照一致，且 R-05 顺序偏差模型已按 0.10·K（非 0.364·K）修正（`tests/unit/SkillBatch4BloodSeaOrderingTests.cpp:27-29,157-159`），与本轮代码一致。
- 第 5/6 轮针对 F11 的修订仅触及 `docs/plans/...-plan.md` 与 `docs/designs/...-design.md`，**未改动任何 C++/测试文件**，故第 1 节所列构建/测试证据仍然适用；本轮对 F11 的复验为纯文档只读比对（`plan:101,131-140,158-171,232,242`、`design:243-268,385` 对 `HeavenlySwordDescent.cpp:605-608,655-659`、`BloodSea.cpp:430-435,439-445`、`SevenStarSlash.cpp:356,359-361,424-427`）。

## 6. 结论

10 项发现（F1–F10）在**当前工作区代码**中均已正确、完整关闭，且修复未引入新的代码缺陷：内存安全/空解引用/数值坍缩/测试隔离四类风险点均已闭合，守卫在三个行为层之间语义一致（哨兵默认值 ≠ 回退基准的字段一律加 `skill != nullptr`；哨兵默认值 == 回退基准的字段仅需 `profile ?`）。

F11 经第 5/6 轮文档修订后**关闭**：
- **(a) 通过**：计划 §2.2 天剑/血海主伪代码（`plan:131-140`、`plan:158-171`）与 `HeavenlySwordDescent.cpp:605-608,655-659`、`BloodSea.cpp:430-435,439-445` 逐字一致；`plan:101` 哨兵守卫通则段落完整。
- **(b) 通过**：`plan:232`、`plan:242` 已同步哨兵门与内层 `skill ?`；`design:251,252` 回退基准已加 `skill ?`；全文件 `rg "skill->"` 确认无未守卫解引用。
- **(c) 通过**：设计 §3.3 第 3/4 条、第 1 条技能 10 例外（`design:247`）与 §6 R-01（`design:385`）互相自洽；例外由 `SevenStarSlash.cpp:359-361` 的空 `skillData` 早退保证，成立。

F12/F13 为已登记的边界/残留项（非阻断：静态资源下不可达，或不改变正常路径行为）。所有发现关闭，代码与文档一致。

**提交**
