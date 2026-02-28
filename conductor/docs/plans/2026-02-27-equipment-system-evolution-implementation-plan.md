# Equipment System Evolution Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Deliver a modular, self-found-first equipment progression loop by splitting drop generation into extensible modules, adding target-drop routing, and introducing one gated crafting strategy hook with measurable outcomes.

**Architecture:** Keep current gameplay behavior stable first (P1), then layer targeted drop routing (P2), then season-ready extension points (P3). All new behavior is controlled by feature flags and observed via structured drop trace metrics. The plan uses TDD and small vertical slices so each step can be validated and rolled back independently.

**Tech Stack:** C++20, EnTT ECS, CMake/MSVC, doctest, JSON data assets (`loot_tables.json`, `affixes.json`).

---

### Task 1: Introduce Drop Contracts (P1 Foundation)

**Files:**
- Create: `src/game/systems/item/DropContracts.hpp`
- Modify: `src/game/systems/item/DropSystem.cpp`
- Test: `tests/unit/DropContractsTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] DropContracts - DropContext defaults are stable") {
    DropContext ctx{};
    CHECK(ctx.areaLevel == 1);
    CHECK(ctx.magicFind == doctest::Approx(0.0f));
    CHECK(ctx.targetPoolId.empty());
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] DropContracts - DropContext defaults are stable"`
Expected: FAIL with missing `DropContext`/`DropResult` symbols.

**Step 3: Write minimal implementation**

```cpp
struct DropContext { int areaLevel = 1; float magicFind = 0.0f; std::string targetPoolId; /* ... */ };
struct DropResult { /* base, rarity, affixes, flags, debugTraceId */ };
```

**Step 4: Run test to verify it passes**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] DropContracts - DropContext defaults are stable"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/item/DropContracts.hpp tests/unit/DropContractsTest.cpp src/game/systems/item/DropSystem.cpp
git commit -m "refactor(item): add drop contracts for modular pipeline"
```

### Task 2: Extract Pipeline Stages Without Behavior Changes

**Files:**
- Create: `src/game/systems/item/DropSourceResolver.hpp`
- Create: `src/game/systems/item/BaseSelector.hpp`
- Create: `src/game/systems/item/RarityAffixRoller.hpp`
- Create: `src/game/systems/item/DropPostProcessor.hpp`
- Modify: `src/game/systems/item/DropSystem.cpp`
- Test: `tests/unit/DropPipelineParityTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] DropPipeline parity - legacy and modular outputs are equivalent") {
    CHECK(GenerateDropLegacy(seed) == GenerateDropModular(seed));
}
```

**Step 2: Run test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] DropPipeline parity - legacy and modular outputs are equivalent"`
Expected: FAIL because modular pipeline path is not wired.

**Step 3: Write minimal implementation**

```cpp
// DropSystem.cpp
DropResult result = DropSourceResolver::Resolve(ctx);
result = BaseSelector::Select(ctx, result);
result = RarityAffixRoller::Roll(ctx, result);
result = DropPostProcessor::Finalize(ctx, result);
```

**Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -R "^nmd.tests.unit$" --output-on-failure`
Expected: PASS with no parity regressions.

**Step 5: Commit**

```bash
git add src/game/systems/item/Drop*.hpp src/game/systems/item/DropSystem.cpp tests/unit/DropPipelineParityTest.cpp
git commit -m "refactor(item): split drop pipeline into resolver/selector/roller/post stages"
```

### Task 3: Add Target Drop Schema and Parsing

**Files:**
- Modify: `assets/data/loot_tables.json`
- Modify: `src/game/systems/item/LootTable.hpp`
- Modify: `src/game/systems/item/DropSystem.cpp`
- Test: `tests/unit/LootTableTargetPoolParseTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] LootTable parse - target pool fields") {
    auto table = LoadLootTable("assets/data/loot_tables.json");
    REQUIRE(table.HasEntry("boss_target_route_01"));
    CHECK(table.Entry("boss_target_route_01").targetPoolId == "blade_route_a");
}
```

**Step 2: Run test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] LootTable parse - target pool fields"`
Expected: FAIL because new JSON fields are unknown.

**Step 3: Write minimal implementation**

```cpp
// LootTable.hpp
std::string targetPoolId;
float targetWeight = 0.0f;
std::vector<std::string> sourceTags;
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] LootTable parse - target pool fields"`
Expected: PASS.

**Step 5: Commit**

```bash
git add assets/data/loot_tables.json src/game/systems/item/LootTable.hpp src/game/systems/item/DropSystem.cpp tests/unit/LootTableTargetPoolParseTest.cpp
git commit -m "feat(item): add target pool fields to loot table schema"
```

### Task 4: Implement TargetDropModule (P2)

**Files:**
- Create: `src/game/systems/item/TargetDropModule.hpp`
- Create: `src/game/systems/item/TargetDropModule.cpp`
- Modify: `src/game/systems/item/BaseSelector.hpp`
- Modify: `src/game/systems/item/DropSystem.cpp`
- Test: `tests/unit/TargetDropModuleTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] TargetDropModule routes boss-tagged context to target pool") {
    DropContext ctx{};
    ctx.sourceId = "boss.blade.ascendant";
    ctx.targetPoolId = "blade_route_a";
    auto out = ResolveTargetBase(ctx);
    CHECK(out.baseId == "dragon_tooth_sword");
}
```

**Step 2: Run test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] TargetDropModule routes boss-tagged context to target pool"`
Expected: FAIL because module does not exist.

**Step 3: Write minimal implementation**

```cpp
if (flags.targetPoolEnabled && !ctx.targetPoolId.empty()) {
    return TargetDropModule::Resolve(ctx, lootTable);
}
return BaseSelector::SelectDefault(ctx, lootTable);
```

**Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/item/TargetDropModule.* src/game/systems/item/BaseSelector.hpp src/game/systems/item/DropSystem.cpp tests/unit/TargetDropModuleTest.cpp
git commit -m "feat(item): add target drop routing module"
```

### Task 5: Add Affix Metadata for Craft Constraints

**Files:**
- Modify: `assets/data/affixes.json`
- Modify: `src/game/systems/item/ItemFactory.cpp`
- Modify: `src/game/systems/item/CraftingSystem.cpp`
- Test: `tests/unit/AffixCraftConstraintTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] Craft constraints honor mutual exclusion groups") {
    auto item = MakeRareWeaponWithAffix("crit_multi_group_a");
    auto ok = TryAddAffix(item, "crit_multi_group_b");
    CHECK_FALSE(ok);
}
```

**Step 2: Run test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Craft constraints honor mutual exclusion groups"`
Expected: FAIL because group checks are missing.

**Step 3: Write minimal implementation**

```cpp
if (HasMutualExclusionConflict(existingAffixes, candidateAffix)) {
    return false;
}
```

**Step 4: Run test to verify it passes**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Craft constraints honor mutual exclusion groups"`
Expected: PASS.

**Step 5: Commit**

```bash
git add assets/data/affixes.json src/game/systems/item/ItemFactory.cpp src/game/systems/item/CraftingSystem.cpp tests/unit/AffixCraftConstraintTest.cpp
git commit -m "feat(item): add affix craft groups and exclusion constraints"
```

### Task 6: Add One Gated Crafting Strategy Hook

**Files:**
- Modify: `src/game/systems/item/CraftingSystem.hpp`
- Modify: `src/game/systems/item/CraftingSystem.cpp`
- Test: `tests/unit/CraftingStrategyHookTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] Crafting strategy hook - guaranteed-safe-upgrade consumes configured budget") {
    auto item = MakeCraftableItemWithFP(20);
    auto ok = ApplyStrategy(item, "safe_upgrade");
    CHECK(ok);
    CHECK(item.forgingPotential <= 20);
}
```

**Step 2: Run test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Crafting strategy hook - guaranteed-safe-upgrade consumes configured budget"`
Expected: FAIL because strategy hook path is absent.

**Step 3: Write minimal implementation**

```cpp
if (!flags.craftingStrategyHooksEnabled) return ApplyLegacyCraft(...);
return ApplySafeUpgradeStrategy(...);
```

**Step 4: Run test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/item/CraftingSystem.* tests/unit/CraftingStrategyHookTest.cpp
git commit -m "feat(item): add gated crafting strategy hook"
```

### Task 7: Add Drop Trace Observability + Feature Flags

**Files:**
- Modify: `src/game/systems/item/DropSystem.cpp`
- Modify: `src/game/systems/item/LootFilter.cpp`
- Modify: `settings.json`
- Test: `tests/unit/DropTraceFeatureFlagTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] Drop trace emits trace id when enabled") {
    EnableFlag("drop.target_pool.enabled", true);
    EnableFlag("drop.trace.enabled", true);
    auto drop = GenerateDropForTest();
    CHECK_FALSE(drop.debugTraceId.empty());
}
```

**Step 2: Run test to verify it fails**

Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] Drop trace emits trace id when enabled"`
Expected: FAIL because trace id emission is not implemented.

**Step 3: Write minimal implementation**

```cpp
if (flags.dropTraceEnabled) {
    result.debugTraceId = BuildTraceId(ctx, result);
    LogDropTrace(result.debugTraceId, ctx, result);
}
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/item/DropSystem.cpp src/game/systems/item/LootFilter.cpp settings.json tests/unit/DropTraceFeatureFlagTest.cpp
git commit -m "chore(item): add drop trace observability and feature flags"
```

### Task 8: Final Validation + Docs Sync

**Files:**
- Modify: `设计文档/装备和存储设计.md`
- Modify: `conductor/bug_registry.md` (only if a new risk item is discovered)

**Step 1: Write the failing documentation checklist**

```text
- Missing target pool field docs
- Missing feature flag docs
- Missing rollout/rollback notes
```

**Step 2: Run verification commands before doc updates**

Run: `./build.bat`
Expected: PASS.

Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
Expected: PASS.

**Step 3: Write minimal documentation updates**

```markdown
- Add target-drop module and fields
- Add crafting strategy hook behavior and flag
- Add rollout and rollback checklist
```

**Step 4: Run final regression suite**

Run: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add 设计文档/装备和存储设计.md
git commit -m "docs(item): document modular drop pipeline and rollout flags"
```

## Suggested Execution Order

1. Task 1 -> Task 2 (stabilize contracts and parity)
2. Task 3 -> Task 4 (enable targeted progression)
3. Task 5 -> Task 6 (constrain and deepen crafting)
4. Task 7 -> Task 8 (observe, validate, and document)

## Definition of Done

- P1/P2 scopes are behind flags and can be toggled independently.
- Unit and CI suites pass in RelWithDebInfo.
- Drop distribution parity (legacy vs modular, default flags off) remains within 3% drift.
- At least two targeted routes are playable and produce usable base items in expected time windows.
- Documentation is synchronized with implementation and rollout procedures.
