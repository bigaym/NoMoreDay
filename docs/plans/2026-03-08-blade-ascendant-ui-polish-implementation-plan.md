# Blade Ascendant UI Polish Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a mastery-first visual system for the Blade Ascendant specialization UI so Sword Saint, Demon Blade, and Heavenly Sword read as distinct identities across the hub, tree, nodes, links, tooltip, and lightweight interaction feedback.

**Architecture:** Introduce a shared mastery theme profile layer and make the existing UI entry points consume it instead of deriving presentation directly from skill tags alone. Keep the current interaction flow, contract/runtime data model, and scissor-safe rendering structure intact while adding only lightweight, deterministic visual polish.

**Tech Stack:** C++20, EnTT, raylib/rlgl UI rendering, doctest, CMake/MSVC, existing Blade Mastery and Skill specialization runtime systems.

---

### Task 1: Expose Mastery Identity to the Specialization Tree

**Files:**
- Modify: `src/game/components/SkillDefs.hpp`
- Modify: `src/game/data/SkillRegistry.hpp`
- Modify: `src/game/data/SkillRegistry.cpp`
- Test: `tests/unit/SkillRegistryMasteryTreeTests.cpp`

**Step 1: Write the failing test**

Add a unit test that loads the mastery skill tree data and verifies a known Blade skill tree carries a stable `BladeMasteryId` for UI lookup.

```cpp
TEST_CASE("[Unit] SkillRegistry - mastery skill trees preserve mastery identity") {
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

    const SkillTreeDefinition* tree = SkillRegistry::Get().GetSkillTree(7);
    REQUIRE(tree != nullptr);
    CHECK(tree->mastery_id == BladeMasteryId::HeavenlySword);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] SkillRegistry - mastery skill trees preserve mastery identity"`

Expected: FAIL or compile failure because `SkillTreeDefinition` does not expose mastery identity yet.

**Step 3: Write minimal implementation**

Thread `mastery_id` from `assets/data/mastery_skill_trees.json` into the loaded tree definition and make it accessible to UI code.

```cpp
struct SkillTreeDefinition {
    uint32_t skill_id = 0;
    BladeMasteryId mastery_id = BladeMasteryId::None;
    std::unordered_map<uint32_t, TalentNode> nodes;
};
```

Update serialization and registry loading so the field survives load/save paths.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] SkillRegistry - mastery skill trees preserve mastery identity"`

Expected: PASS.

### Task 2: Add a Shared Blade Mastery UI Theme Profile

**Files:**
- Create: `src/game/systems/ui/BladeMasteryUITheme.hpp`
- Create: `src/game/systems/ui/BladeMasteryUITheme.cpp`
- Test: `tests/unit/BladeMasteryUIThemeTests.cpp`

**Step 1: Write the failing test**

Add a unit test that verifies all three Blade masteries resolve distinct, lightweight theme profiles.

```cpp
TEST_CASE("[Unit] BladeMasteryUITheme - all blade masteries expose distinct profiles") {
    const auto swordSaint = BladeMasteryUITheme::For(BladeMasteryId::SwordSaint);
    const auto heavenlySword = BladeMasteryUITheme::For(BladeMasteryId::HeavenlySword);
    const auto demonBlade = BladeMasteryUITheme::For(BladeMasteryId::DemonBlade);

    CHECK(swordSaint.background_pattern != heavenlySword.background_pattern);
    CHECK(demonBlade.node_shell != swordSaint.node_shell);
    CHECK(swordSaint.primary.r != demonBlade.primary.r);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] BladeMasteryUITheme - all blade masteries expose distinct profiles"`

Expected: FAIL or compile failure because the theme helper does not exist yet.

**Step 3: Write minimal implementation**

Create a shared, intentionally small theme profile used by hub, tree, and tooltip chrome.

```cpp
enum class BladeUiBackgroundPattern { DiagonalCuts, ErosionPlate, ArrayOrbit };
enum class BladeUiNodeShell { SwordSaint, DemonBlade, HeavenlySword };
enum class BladeUiLinkStyle { Linear, InwardPulse, ArcFlow };

struct BladeMasteryUIThemeProfile {
    Color primary;
    Color secondary;
    Color highlight;
    Color danger;
    BladeUiBackgroundPattern background_pattern;
    BladeUiNodeShell node_shell;
    BladeUiLinkStyle link_style;
    float idle_pulse_seconds;
};
```

Keep Demon Blade restrained: broken plate + inward pulse + limited corrosion edge, not dense crack noise.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] BladeMasteryUITheme - all blade masteries expose distinct profiles"`

Expected: PASS.

### Task 3: Introduce Shared Node Visual Classification and Apply Themes to Hub/Tree Chrome

**Files:**
- Modify: `src/game/systems/ui/UISkillHub.cpp`
- Modify: `src/game/systems/ui/UISkillHub.hpp`
- Modify: `src/game/systems/ui/UISkillSpecRenderer.hpp`
- Modify: `src/game/systems/ui/UISkillSpecRenderer.cpp`
- Modify: `src/game/systems/ui/UISkillTalentTree.cpp`
- Modify: `tests/tech/UITests.cpp`

**Step 1: Write the failing test**

Extend `tests/tech/UITests.cpp` using the same source-file loading pattern already used by the scissor test, and assert that the hub and tree renderer consume the shared mastery theme and shared node visual classification helper.

```cpp
TEST_CASE("[Tech] SkillUI - hub and tree use shared blade mastery theme plumbing") {
    const std::string hubSource = LoadUiSource("UISkillHub.cpp");
    const std::string treeSource = LoadUiSource("UISkillSpecRenderer.cpp");

    CHECK(hubSource.find("BladeMasteryUITheme::For") != std::string::npos);
    CHECK(treeSource.find("BladeMasteryUITheme::For") != std::string::npos);
    CHECK(treeSource.find("ResolveNodeVisualKind") != std::string::npos);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - hub and tree use shared blade mastery theme plumbing"`

Expected: FAIL because the shared theme/classification plumbing is not present yet.

**Step 3: Write minimal implementation**

Do three things together:

1. Theme the mastery cards in `UISkillHub` with the shared profile.
2. Add a shared node-visual classification helper used by both renderer and hit-testing.
3. Replace current generic background/link chrome in `UISkillSpecRenderer` with mastery-first styling instead of layering more effects on top.

```cpp
enum class NodeVisualKind { Passive, Modifier, Synergy, Trigger, Transmuter, Keystone };

NodeVisualKind ResolveNodeVisualKind(const TalentNode& node,
                                     const NodeContractData* contract);

float GetNodeRadius(const TalentNode& node,
                    const NodeContractData* contract,
                    const SkillSpecView& view);
```

Rules:

- mastery identity is always the dominant color/story;
- element/transmuter color stays local to inner rings, corner ticks, or tiny badges;
- current generic glows/grid/particles are reduced or replaced when mastery chrome arrives;
- hover and click bounds must use the same node-visual classification as rendering.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - hub and tree use shared blade mastery theme plumbing"`

Expected: PASS.

### Task 4: Polish Tooltip Hierarchy and Lightweight Tree Feedback

**Files:**
- Modify: `src/game/systems/ui/UISkillTalentTree.cpp`
- Modify: `src/game/systems/ui/UISkillTalentTree.hpp`
- Modify: `tests/tech/UITests.cpp`

**Step 1: Write the failing test**

Add a tech/source guard that verifies the tooltip path now uses dedicated helpers for badge rendering and keyword-aware layout, while preserving the existing scissor-balance guarantee.

```cpp
TEST_CASE("[Tech] SkillUI - specialization tooltip uses badge and keyword helpers") {
    const std::string source = LoadUiSource("UISkillTalentTree.cpp");
    CHECK(source.find("DrawRoleBadge") != std::string::npos);
    CHECK(source.find("DrawScopeBadge") != std::string::npos);
    CHECK(source.find("DrawKeywordHighlights") != std::string::npos);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - specialization tooltip uses badge and keyword helpers"`

Expected: FAIL because the tooltip is still plain-text-first.

**Step 3: Write minimal implementation**

Refactor the tooltip into three layers:

1. mastery-colored header chrome;
2. badge row for role/scope/exclusion;
3. wrapped description with keyword-aware emphasis.

```cpp
DrawTooltipHeader(bounds, theme, node.name);
DrawRoleBadge(role_bounds, contract.role, theme);
DrawScopeBadge(scope_bounds, contract.scope_policy, theme);
DrawKeywordHighlights(text_bounds, description,
                      {"剑意", "御剑步", "触发", "仅限本技能", "互斥"});
```

Implement keyword emphasis with a small text-layout helper; do not use naive string substitution that would break wrapping/cropping.

Keep exactly one `BeginScissorMode` / `EndScissorMode` pair around the tree draw path.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - specialization tooltip uses badge and keyword helpers"`

Expected: PASS.

### Task 5: Run Full Verification for the UI Polish Slice

**Files:**
- Verify: `src/game/components/SkillDefs.hpp`
- Verify: `src/game/data/SkillRegistry.hpp`
- Verify: `src/game/data/SkillRegistry.cpp`
- Verify: `src/game/systems/ui/BladeMasteryUITheme.hpp`
- Verify: `src/game/systems/ui/BladeMasteryUITheme.cpp`
- Verify: `src/game/systems/ui/UISkillHub.cpp`
- Verify: `src/game/systems/ui/UISkillSpecRenderer.cpp`
- Verify: `src/game/systems/ui/UISkillTalentTree.cpp`
- Verify: `tests/unit/SkillRegistryMasteryTreeTests.cpp`
- Verify: `tests/unit/BladeMasteryUIThemeTests.cpp`
- Verify: `tests/tech/UITests.cpp`

**Step 1: Run the focused doctest cases**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] SkillRegistry - mastery skill trees preserve mastery identity,[Unit] BladeMasteryUITheme - all blade masteries expose distinct profiles,[Tech] SkillUI*"`

Expected: PASS.

**Step 2: Build the project**

Run: `./build.bat`

Expected: Build completes successfully.

**Step 3: Run the unit label set**

Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`

Expected: PASS.

**Step 4: Run the CI label set**

Run: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

Expected: PASS.

### Notes

- Keep the current specialization allocation, pan, zoom, drag, reset, and unspecialize flow untouched.
- Do not add a new RenderGraph pass for this UI work.
- Let mastery identity dominate; let element/transmuter accents remain secondary and local.
- Replace generic chrome as mastery chrome lands; do not stack new effects on top of the old background noise.
- Commit steps are intentionally omitted because the user has not requested any git commits.
