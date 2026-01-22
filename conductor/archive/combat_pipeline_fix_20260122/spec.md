# Specification: Combat Pipeline Robustness

## 1. 背景与目标

### 1.1 问题描述
在 `rendering_performance_sync_opt` 轨道的回归测试中，`CombatBalanceTest` 报告所有伤害计算结果为 0。
初步分析表明，`DamagePipeline::Calculate` 严重依赖 `SkillRegistry` 的数据 (`weapon_damage_mult`) 来计算武器伤害。如果 `skill_id` (如默认值 0) 未在注册表中正确定义，管线将忽略武器伤害，导致输出为 0。

### 1.2 目标
1.  **修复测试**: 确保 `CombatBalanceTest` 正确模拟战斗环境并通过验证。
2.  **增强鲁棒性**: 确保 `DamagePipeline` 在面对未定义技能 ID 时有明确的行为（报错或回退），而不是默默失败。
3.  **标准化默认行为**: 定义 `SkillID::BasicAttack (0)` 的标准属性，使其作为系统的兜底逻辑。

---

## 2. 技术方案

### 2.1 依赖分析
`DamagePipeline.cpp`:
```cpp
// 2. Add Skill Base Damage
const auto *skill_data = SkillRegistry::Get().GetSkill(skill_id);
if (skill_data) {
    // ... Calculate Base Damage using skill_data ...
}
// Else: Do nothing -> Base Damage = 0 if Base Pool is empty.
```

### 2.2 解决方案：默认技能注册 (Default Skill Registration)
在 `SkillRegistry` 初始化时，或者在 `TestCommon` 的测试环境中，必须强制注册 ID 为 0 的 `Basic Attack`。

**数据定义 (Draft)**:
```cpp
struct SkillData {
    uint32_t id = 0;
    std::string name = "Basic Attack";
    float weapon_damage_mult = 1.0f; // 100% Weapon Damage
    float base_damage = 0.0f;
    Tag tags = Tag::Melee | Tag::Physical | Tag::Attack;
    // ...
};
```

### 2.3 解决方案：管线安全网 (Pipeline Safety Net)
在 `DamagePipeline::Calculate` 中添加对 `skill_data` 为空的检查。

**逻辑变更**:
```cpp
if (!skill_data) {
    if (skill_id == 0) {
        // Fallback: Construct temporary "Basic Attack" data
        // warning: "Skill 0 not registered, using fallback"
    } else {
        // error: "Invalid Skill ID"
        return result; // 0 damage is correct for invalid skill
    }
}
```

### 2.4 测试修正
更新 `CombatBalanceTest.hpp`，在 `TestSetupScope` 中显式注册测试用的 Dummy Skill 或确保系统默认 Skill 已加载。

---

## 3. 验收标准

1.  **编译**: 无新警告。
2.  **测试**: `CombatBalanceTest` 的 3 个 Subcase 全部通过 (100, 50, 150 damage)。
3.  **日志**: 运行测试时，不应出现 "Skill not found" 的错误日志（除非预期测试错误情况）。

