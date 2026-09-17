# UMR 技能专精改造第三批（技能 7 心剑·无影、技能 8 御剑·回旋、技能 9 绝影绝剑）实施计划

- **文档状态**：已批准待实施（v1.1：响应首轮独立审查报告全面修订）
- **设计依据**：[`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md)
- **计划日期**：2026-09-17
- **系统代号**：`UMR-SKILL-BATCH-3` (Unified Modifier Runtime - Skill Batch 3)
- **修订记录**：
  - v1.0：初稿（11 条 Canonical 记录 + 8 迁移键与 3 死键退役 + 6 阶段原子任务）。
  - v1.1：响应首轮独立审查反馈全面修订：
    - Task 6 显式补齐物理修改 `SkillDefs.hpp` 删除 `PhantomTranceParams::cooldown_flat_reduce` 死字段任务（解 F-01）；
    - 步骤 4 伪代码消除巨阙判定裸数字，改用具名位移 `(1u << 20) /* 854 Giant */`（解 F-02）；
    - 步骤 4 伪代码明确技能 9 冷却保底为“全局硬下限 1.0s”（解 F-03）；
    - Task 7 明确 C++ 单元测试职责划分：纯交付算子在 `DeliveryOpTests`，Baker 综合断言归入 `SkillSpecializationBakerTests.cpp`（采纳 F-04）。
- **计划范围**：
  - Phase 1：技能 7/8/9 Canonical 记录配置（11 条）与紧凑二进制生成
  - Phase 2：离线门禁体系扩展（`validate_skill_spec_modifiers.py`）与独立门禁测试（`SkillSpecBatch3GateTest.py`）
  - Phase 3：机制表单一事实源退役（8 个迁移键）与历史死键清理（3 个死键）
  - Phase 4：`SkillSpecializationBaker` 步骤 1 基准显式化、步骤 2 分支清理、步骤 4 终局覆盖与形态单源同步
  - Phase 5：C++ 单元测试套件实现（`SkillBatch3DeliveryOpTests.cpp`）
  - Phase 6：构建编译、功能回归与单例隔离保障

---

## 1. 实施思路与原理

### 1.1 数据流与架构边界

1. **纯数值全量外置入 UMR Canonical 数据管线（0 新增算子）**：
   - **技能 7（心剑·无影）**：
     - 700 神识凝聚：`SKILL_MANA_COST_MULT`（法耗折扣 10%/点，param_f32: 0.10）；
     - 701 无影无形：`SKILL_MORE_DAMAGE_MULT`（基础物理增伤 10%/点，param_f32: 0.10）；
     - 702 裂空：`SKILL_AREA_MULT`（范围半径放大 10%/点，param_f32: 0.10）；
     - 703 心念映射：`SKILL_RANGE_MULT`（追踪射程放大 10%/点，param_f32: 0.10）；
     - 732 步影随行：`SKILL_MANA_COST_MULT`（Keystone 引导法耗增加 50%，param_f32: -0.50）。
   - **技能 8（御剑·回旋）**：
     - 800 轻巧：`SKILL_MANA_COST_FLAT`（基础法耗平减 -1.0/点，param_f32: -1.0）；
     - 801 疾速（速度）：`SKILL_SPEED_MULT`（飞行速度放大 15%/点，param_f32: 0.15）；
     - 801 疾速（射程）：`SKILL_RANGE_MULT`（最远飞行距离放大 15%/点，param_f32: 0.15）；
     - 810 滞空切割：`SKILL_DURATION_FLAT`（Keystone 滞空时长 0.8s，param_f32: 0.80）。
   - **技能 9（绝影绝剑）**：
     - 975 延命：`SKILL_DURATION_FLAT`（绝影形态时长增加 0.25s/点，param_f32: 0.25）；
     - 986 缩地成寸：`SKILL_COOLDOWN_FLAT`（基础冷却平减 -1.0s/点，param_f32: -1.0）。
   - 共计 11 条纯数值记录录入 [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)，复用既有 OpCodes 30..42，不产生任何底层 Schema 变更。

2. **基准确值化与步骤 1 显式初始化**：
   - 乘算算子（`SKILL_RANGE_MULT`、`SKILL_SPEED_MULT`、`SKILL_AREA_MULT`、`SKILL_MANA_COST_MULT`）与加算算子（`SKILL_DURATION_FLAT`、`SKILL_COOLDOWN_FLAT`）严格依赖确定性初值；
   - 技能 7：步骤 1 显式初始化 `out_profile.area_radius = 60.0f`（取自机制表 `7/0/base_radius`）、`del.range = 350.0f`（取自 `7/0/base_range`）、`out_profile.effective_mana_cost = 15.0f`（取自 `7/0/mana_cost_per_sec`），杜绝 702 在步骤 2 中手写重算；
   - 技能 8：步骤 1 显式赋予 `del.speed = 400.0f`、`del.range = 300.0f`、`del.duration = 0.0f`（未点 810 立即折返，无滞空）；
   - 技能 9：步骤 1 显式赋予 `del.speed = 600.0f`、`del.duration = 3.0f`（取自 `9/0/form_duration`）、`del.trance.duration_sec = 3.0f`、`out_profile.effective_cooldown = 15.0f`。

3. **专精循环清理与通用增量合成**：
   - 清理步骤 2 中 700..703, 732, 800..801, 810, 975, 986 的手写纯数值计算；
   - 步骤 3 通用合成逻辑自动生效，直接消费 11 条增量，实现零堆分配、零运行期分支求值。

4. **步骤 4 确定性终局覆盖与单源同步**：
   - 技能 8：854 巨阙与 830 侧刃互斥，若点亮巨阙强制锁定 `out_profile.delivery.sub_count = 0`，消除遍历顺序依赖；
   - 技能 9：步骤 4 实施单向同步 `out_profile.delivery.trance.duration_sec = out_profile.delivery.duration`，彻底消除双重数据源；施加冷却保底 `out_profile.effective_cooldown = std::max(1.0f, out_profile.effective_cooldown)`；彻底废除死字段 `del.trance.cooldown_flat_reduce`。

5. **单一事实源治理与死键清理**：
   - 机制表 `skill_mechanics.json` 彻底删除 8 项已迁移键；
   - 彻底删除 3 项全仓零引用的历史死键：`8/810/hover_tick_interval`、`9/0/form_move_pct`、`9/0/weaken_duration`；
   - 门禁脚本中登记死键列表，禁止历史死键回潮。

---

## 2. 关键逻辑伪代码引导

### 2.1 烘焙层基准初始化与分支清理 (`SkillSpecializationBaker.cpp`)

```cpp
// ==================== 步骤 1: 基础属性与交付默认推导 ====================
switch (skill_id) {
  case 7: // 心剑·无影
    del.sub_interval = 0.3f;
    del.range = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_range", 350.0f);
    // 显式基准写入：消除 702 隐式魔法数与幂等破坏
    out_profile.area_radius = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_radius", 60.0f);
    // 引导法耗 15.0f 保持由步骤 1 头部从 7/0/mana_cost_per_sec 读取
    break;

  case 8: // 御剑·回旋
    del.speed = skillData->GetParam("speed", 400.0f);
    del.range = skillData->GetParam("max_distance", 300.0f);
    del.duration = 0.0f; // 滞空时长默认 0，由 810 经 UMR 加算
    break;

  case 9: // 绝影绝剑：突进形态
    del.speed = skillData->GetParam("dash_speed", 600.0f);
    del.duration = data::SkillMechanicsRegistry::Get().GetFloat(9, 0, "form_duration", 3.0f);
    del.trance.duration_sec = del.duration;
    break;
}

// ==================== 步骤 2: 清理纯数值分支 ====================
// case 7: 移除 700/701/702/703 分支与 732 的 mana_penalty_pct 计算
// case 8: 移除 800/801/810 手写数值计算（810 仅保留 flag 16u）
// case 9: 移除 975 时长累加与 986 冷却计算（保留 flag 1u 与 65536u）

// ==================== 步骤 3: UMR 增量合成（既有通用管线自动生效） ====================
// out_profile.effective_mana_cost, cooldown, more_damage, area_radius, range, speed, duration

// ==================== 步骤 4: 确定性终局覆盖与单源同步 ====================
if (skill_id == 8) {
  // 854 巨阙覆盖侧刃（消除裸魔法数字）
  if ((out_profile.delivery.feature_flags & (1u << 20) /* 854 Giant */) != 0) {
    out_profile.delivery.sub_count = 0;
  }
} else if (skill_id == 9) {
  // 975 延命单源同步：统一 duration 与 trance.duration_sec
  out_profile.delivery.trance.duration_sec = out_profile.delivery.duration;
  // 技能 9 全局冷却硬下限 1.0s（含 986 平减防穿透）
  out_profile.effective_cooldown = std::max(1.0f, out_profile.effective_cooldown);
}
```

---

## 3. 原子任务拆分（Atomic Task Breakdown）

- [ ] **Task 1：配置 11 条 Canonical 专精修饰器记录**
  - 在 [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json) 中追加 11 条记录：
    - `2007000`: 700 (神识凝聚, `SKILL_MANA_COST_MULT`, 0.10, `mul`, `skill.mana_cost`)
    - `2007010`: 701 (无影无形, `SKILL_MORE_DAMAGE_MULT`, 0.10, `mul`, `skill.more_damage`)
    - `2007020`: 702 (裂空, `SKILL_AREA_MULT`, 0.10, `mul`, `skill.area_mult`)
    - `2007030`: 703 (心念映射, `SKILL_RANGE_MULT`, 0.10, `mul`, `skill.range_mult`)
    - `2007320`: 732 (步影随行, `SKILL_MANA_COST_MULT`, -0.50, `mul`, `skill.mana_cost`)
    - `2008000`: 800 (轻巧, `SKILL_MANA_COST_FLAT`, -1.0, `add`, `skill.mana_cost_flat`)
    - `2008010`: 801 (疾速-速度, `SKILL_SPEED_MULT`, 0.15, `mul`, `skill.speed_mult`)
    - `2008011`: 801 (疾速-射程, `SKILL_RANGE_MULT`, 0.15, `mul`, `skill.range_mult`)
    - `2008100`: 810 (滞空切割, `SKILL_DURATION_FLAT`, 0.80, `add`, `skill.duration_flat`)
    - `2009750`: 975 (延命, `SKILL_DURATION_FLAT`, 0.25, `add`, `skill.duration_flat`)
    - `2009860`: 986 (缩地成寸, `SKILL_COOLDOWN_FLAT`, -1.0, `add`, `skill.cooldown_flat`)
  - 严格填齐 `operation`、`target`、`stacks`、`conditions` 等全量字段。

- [ ] **Task 2：编译紧凑二进制与同步运行时 JSON 资产**
  - 执行 `python scripts/gen_modifier_runtime_v2.py` 生成二进制 blob 与 JSON；
  - 验证 `gen_modifier_runtime_v2.py --check` 返回 0。

- [ ] **Task 3：机制表单一事实源退役与历史死键清理**
  - 在 [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中删除 8 项迁移键：
    - `7/700/mana_reduction_pct_per_point`
    - `7/701/phys_damage_pct_per_point`
    - `7/702/radius_pct_per_point`
    - `7/703/range_pct_per_point`
    - `7/732/mana_penalty_pct`
    - `8/810/hover_duration`
    - `9/975/duration_per_point`
    - `9/986/cd_per_point`
  - 删除 3 项历史死键：
    - `8/810/hover_tick_interval`
    - `9/0/form_move_pct`
    - `9/0/weaken_duration`
  - 运行 `python scripts/gen_skill_mechanics_schema.py` 刷新 schema。

- [ ] **Task 4：离线门禁脚本升级与防回潮保障**
  - 修改 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py)：
    - 在 `MIGRATION_EQUIVALENCE` 中登记 Batch 3 11 条规则；
    - 在 `DEAD_MECHANICS_KEYS` 中追加 3 项历史死键；
    - 在 `KEPT_MECHANICS_KEYS` 中登记基准保留键；
  - 执行 `python scripts/validate_skill_spec_modifiers.py` 确保 6 项门禁全通过。

- [ ] **Task 5：新增独立 Python 门禁测试**
  - 创建 [`tests/python/SkillSpecBatch3GateTest.py`](file:///d:/PRJ/NoMoreDay/tests/python/SkillSpecBatch3GateTest.py)：
    - 校验 11 条记录 ID 解码合规性；
    - 校验 8 项迁移键与 3 项死键从 mechanics 表彻底退役；
    - 校验两生成器 `--check` 一致性。

- [ ] **Task 6：烘焙层重构与形态死字段物理清理**
  - 物理修改 [`src/game/foundation/components/SkillDefs.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp)，从 `PhantomTranceParams` 中删除死字段 `float cooldown_flat_reduce = 0.0f;`（解 F-01）；
  - 修改 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)：
    - 步骤 1：显式初始化技能 7 `area_radius`（60.0f）、技能 8 `duration`（0.0f）；
    - 步骤 2：清理 700..703, 732, 800..801, 810, 975, 986 的手写计算分支；
    - 步骤 4：追加技能 8 巨阙 854 清空 `sub_count`（消除裸数字）；技能 9 终局单源同步 `trance.duration_sec = duration` 与全局冷却硬下限 1.0s。

- [ ] **Task 7：新增 C++ 单元测试与 Baker 综合断言**
  - 创建 [`tests/unit/SkillBatch3DeliveryOpTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillBatch3DeliveryOpTests.cpp)，聚焦交付算子孤立求值：
    - 覆盖技能 7 减耗、增伤、范围、射程、惩罚增耗算子；
    - 覆盖技能 8 法耗平减、弹速、射程、滞空时长；
    - 覆盖技能 9 形态时长累加、冷却平减；
  - 在 [`tests/unit/SkillSpecializationBakerTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationBakerTests.cpp) 追加 Batch 3 综合断言（采纳 F-04）：
    - 技能 7 未点 702 保持 60.0f 基准、点满 702 放大；
    - 技能 8 巨阙 854 覆盖侧刃 `sub_count = 0`；
    - 技能 9 延命双字段同步与冷却保底 1.0s。

- [ ] **Task 8：全量构建、测试回归与状态隔离核验**
  - 检查并在 `MindBladeNodes.cpp`、`BladeBoomerangNodes.cpp`、`PhantomTranceNodes.cpp` 中核验 `ReloadModifierRuntimeFromAsset()`，杜绝随机测试污染；
  - 执行 `build.bat RelWithDebInfo`，确保 0 警告 0 错误；
  - 执行 `ctest -C RelWithDebInfo --output-on-failure`，确保用例 100% 通过。

---

## 4. 测试方法与执行命令

### 4.1 离线门禁测试
```powershell
python scripts/validate_skill_spec_modifiers.py
python tests/python/SkillSpecBatch3GateTest.py
python scripts/gen_modifier_runtime_v2.py --check
python scripts/gen_skill_mechanics_schema.py --check
```

### 4.2 编译构建
```powershell
.\build.bat RelWithDebInfo
```

### 4.3 单元与功能回归测试
```powershell
# 运行 Batch 3 独立交付算子单元测试
.\bin\NoMoreDayTests.exe --test-case=*SkillBatch3Delivery*

# 运行技能 7/8/9 专项功能回归测试
.\bin\NoMoreDayTests.exe --test-case=*MindBlade*
.\bin\NoMoreDayTests.exe --test-case=*BladeBoomerang*
.\bin\NoMoreDayTests.exe --test-case=*PhantomTrance*

# 全量回归验证（排除性能测试）
ctest -C RelWithDebInfo -E Performance --output-on-failure
```

---

## 5. 验证任务完成（DoD & Exit Criteria）

1. **契约闭环**：11 条 Canonical 记录在 `skill_spec_modifiers.canonical.json` 与生成产物中双向对齐；
2. **数据清理**：`skill_mechanics.json` 中 8 个退役键与 3 个死键被物理删除，无悬空引用；
3. **架构单源**：
   - 技能 7 `area_radius` 具备显式 60.0f 基准，702 乘法完全幂等；
   - 技能 8 800/801 彻底消除代码字面量硬编码；
   - 技能 9 `trance.duration_sec` 与 `del.duration` 单源同步，死字段 `cooldown_flat_reduce` 废除；
4. **测试证明**：
   - 离线门禁 6/6 全绿；
   - Python 门禁 9/9 全绿；
   - C++ 单元测试 `SkillBatch3DeliveryOpTests` 全绿；
   - `MindBladeNodes`、`BladeBoomerangNodes`、`PhantomTranceNodes` 100% 通过，断言零衰减。
