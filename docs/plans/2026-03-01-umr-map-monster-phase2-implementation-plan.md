# UMR Map/Monster Phase-2 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extend UMR to cover map-affix and monster-affix stat modifiers through the same runtime evaluator path, while keeping monster behavior affix mechanics unchanged.

**Architecture:** Add two new UMR adapters (`MapModifierAdapter`, `MonsterModifierAdapter`) that convert active map/monster affix state into runtime record IDs + evaluation context and return `ModifierDelta` for `AttributePipeline`. Keep `MonsterAffixSystem` behavior logic in place for non-stat mechanics (on-hit/on-death/update), and migrate only stat effects in this phase. Use `modifier_v2` data files for map/monster domains and compile them into the existing runtime blob.

**Tech Stack:** C++20, nlohmann/json, Python 3.10, doctest, CMake/MSVC, existing `build.bat` precheck pipeline.

---

### Task 1: Add Map/Monster Domain Data Files

**Files:**
- Create: `assets/data/modifier_v2/map_modifiers.json`
- Create: `assets/data/modifier_v2/monster_modifiers.json`
- Modify: `assets/data/modifier_v2/modifier_catalog.json`
- Test: `tests/unit/ModifierSchemaV2ValidationTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierSchemaV2 - Catalog includes map and monster domains") {
  const auto root = nlohmann::json::parse(ReadTextFile("assets/data/modifier_v2/modifier_catalog.json"));
  const auto entries = root.at("entries");
  CHECK(HasCatalogEntry(entries, "map", "map_modifiers.json"));
  CHECK(HasCatalogEntry(entries, "monster", "monster_modifiers.json"));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*Catalog includes map and monster domains*"`
Expected: FAIL because entries/files do not exist.

**Step 3: Write minimal implementation**

Add both JSON files with valid `schema_version=2`, `domain`, and `records`.
Add catalog entries for both files.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*Catalog includes map and monster domains*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add assets/data/modifier_v2 tests/unit/ModifierSchemaV2ValidationTests.cpp
git commit -m "feat: add map and monster modifier_v2 domain data"
```

### Task 2: Add MapModifierAdapter and Map Runtime Test

**Files:**
- Create: `src/game/systems/modifier/MapModifierAdapter.hpp`
- Create: `src/game/systems/modifier/MapModifierAdapter.cpp`
- Test: `tests/unit/MapModifierAdapterTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] MapModifierAdapter - active map affix produces runtime stat delta") {
  const auto blob = BuildMapModifierRuntimeFixture();
  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(blob));

  ActiveDimensionalState state;
  state.explicitAffixes.push_back(MapAffix{MapAffixType::Enemy_ExtraHealth, MapAffixCategory::Debuff, 0.30f, 5, "test"});

  auto delta = MapModifierAdapter::EvaluateEnemyAffixDelta(state);
  CHECK(delta.percent_add.contains(StatType::MaxHealth));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*MapModifierAdapter*"`
Expected: FAIL (adapter missing).

**Step 3: Write minimal implementation**

Implement adapter with:
- map affix -> node id encoding helper
- runtime record resolution by node whitelist intersection
- evaluator call returning `ModifierDelta`

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*MapModifierAdapter*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/MapModifierAdapter.* tests/unit/MapModifierAdapterTests.cpp
git commit -m "feat: add runtime-driven map modifier adapter"
```

### Task 3: Add MonsterModifierAdapter and Runtime Test

**Files:**
- Create: `src/game/systems/modifier/MonsterModifierAdapter.hpp`
- Create: `src/game/systems/modifier/MonsterModifierAdapter.cpp`
- Test: `tests/unit/MonsterModifierAdapterTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] MonsterModifierAdapter - affix list resolves runtime stat delta") {
  const auto blob = BuildMonsterModifierRuntimeFixture();
  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(blob));

  MonsterAffixComponent c;
  c.affixes.push_back(MonsterAffixType::Fast);

  auto delta = MonsterModifierAdapter::EvaluateAffixDelta(c, false);
  CHECK(delta.percent_add.contains(StatType::MoveSpeed));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*MonsterModifierAdapter*"`
Expected: FAIL (adapter missing).

**Step 3: Write minimal implementation**

Implement adapter with:
- monster affix -> node id encoding helper
- runtime record resolution by node whitelist intersection
- optional berserk branch as explicit runtime delta

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*MonsterModifierAdapter*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/MonsterModifierAdapter.* tests/unit/MonsterModifierAdapterTests.cpp
git commit -m "feat: add runtime-driven monster modifier adapter"
```

### Task 4: Migrate AttributePipeline to UMR Map/Monster Adapters

**Files:**
- Modify: `src/game/systems/stats/AttributePipeline.cpp`
- Test: `tests/unit/MonsterAffixTests.cpp`
- Test: `tests/unit/AttributePipelineTest.cpp`

**Step 1: Write the failing regression test**

```cpp
TEST_CASE("[Unit] AttributePipeline - enemy map/monster stats come from UMR delta path") {
  // Build deterministic fixture for map+monster modifiers and assert expected result.
  CHECK(CalculateFixtureEnemyHealth() == doctest::Approx(195.0f));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*enemy map/monster stats come from UMR delta path*"`
Expected: FAIL on current hardcoded path.

**Step 3: Write minimal implementation**

Replace map/monster hardcoded stat modification blocks in `AttributePipeline` with adapter-driven `ModifierDelta` application through `ModifierEvaluator::ApplyStat`.

**Step 4: Run tests to verify pass and no regressions**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*MonsterAffix*" && ./bin/NoMoreDayTests.exe --test-case="*AttributePipeline*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/stats/AttributePipeline.cpp tests/unit/MonsterAffixTests.cpp tests/unit/AttributePipelineTest.cpp
git commit -m "refactor: route enemy map and monster stat mods through UMR adapters"
```

### Task 5: Wire Domain Coverage in Compiler and Validation

**Files:**
- Modify: `scripts/gen_modifier_runtime_v2.py`
- Modify: `scripts/validate_json.py`
- Test: `tests/unit/ModifierCompilerDeterminismTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierCompiler - map and monster records included in runtime") {
  auto debug = CompileAndReadDebugJson();
  CHECK(HasDomain(debug, "map"));
  CHECK(HasDomain(debug, "monster"));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="*map and monster records included in runtime*"`
Expected: FAIL if domains are ignored by pipeline.

**Step 3: Write minimal implementation**

Ensure catalog entries compile as-is and debug output can assert domain coverage.

**Step 4: Run test to verify it passes**

Run: `python scripts/gen_modifier_runtime_v2.py --check && python scripts/gen_modifier_runtime_v2.py --check-determinism`
Expected: `compiled records` includes map/monster counts and determinism passes.

**Step 5: Commit**

```bash
git add scripts/gen_modifier_runtime_v2.py scripts/validate_json.py tests/unit/ModifierCompilerDeterminismTests.cpp
git commit -m "chore: enforce map and monster domain coverage in UMR compiler"
```

### Task 6: Update UMR Design Docs + Final Verification

**Files:**
- Modify: `设计文档/统一修饰器运行时系统_UMR.md`
- Modify: `设计文档/技术架构与实现路线.md`
- Modify: `设计文档/怪物词缀设计.md`
- Modify: `设计文档/局外成长与终局玩法.md`

**Step 1: Write failing doc checklist test (or CI check step)**

```text
Checklist:
- UMR doc has phase-2 status updated.
- Monster/map docs explicitly state "stat effects via UMR; behavior affixes remain MonsterAffixSystem".
```

**Step 2: Run check and verify it fails**

Run: `python scripts/validate_json.py`
Expected: pass; then manual checklist fails before doc updates.

**Step 3: Write minimal implementation**

Update docs with scope boundaries, migration status, and extension path.

**Step 4: Final verification**

Run:
- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`

Expected: all PASS.

**Step 5: Commit**

```bash
git add 设计文档/统一修饰器运行时系统_UMR.md 设计文档/技术架构与实现路线.md 设计文档/怪物词缀设计.md 设计文档/局外成长与终局玩法.md
git commit -m "docs: record UMR phase-2 map and monster migration status"
```
