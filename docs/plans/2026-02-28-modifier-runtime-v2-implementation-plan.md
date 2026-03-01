# Modifier Runtime V2 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a standalone Modifier Runtime V2 that unifies player-side equipment affixes, skill specialization effects, and talent effects under one deterministic, binary-backed evaluator.

**Architecture:** We introduce a new `game/systems/modifier` subsystem with three layers: (1) content schema (JSON V2), (2) offline compiler (`.json -> .bin + .debug.json`), and (3) runtime loader/evaluator that works only with compact integer IDs, masks, and fixed opcodes. Existing player pipelines (`ItemFactory`, `SkillSystem`, `Astrolabe/AttributePipeline`) are migrated to adapter entry points so gameplay behavior is driven by one execution path.

**Tech Stack:** C++20, nlohmann/json, Python 3.10, doctest, CMake/MSVC, existing `build.bat` + `NoMoreDayTests.exe` workflow.

---

## Skill references and working rules

- Use `@superpowers/test-driven-development` for every code task: test first, then minimum implementation.
- Use `@superpowers/verification-before-completion` before claiming each milestone done.
- Use `@superpowers/requesting-code-review` after Task 8 and Task 10.
- Keep commits small and frequent: one commit per task.

## Zero-context onboarding checklist (read first)

- `AGENTS.md`
- `build.bat`
- `scripts/validate_json.py`
- `src/game/components/ItemStats.hpp`
- `src/game/systems/item/ItemFactory.cpp`
- `src/game/systems/stats/AttributePipeline.cpp`
- `src/game/data/SkillRegistry.cpp`
- `src/game/data/TalentData.hpp`
- `tests/CMakeLists.txt`

## Record samples (put into docs and seed files)

Use these exact sample records during Task 1 and Task 2.

```json
{
  "schema_version": 2,
  "domain": "equipment",
  "records": [
    {
      "id": 1001001,
      "domain": "equipment",
      "priority": 100,
      "filters": {
        "profession_mask": 63,
        "skill_id_whitelist": [1],
        "required_skill_tags_all": 4294967296,
        "forbidden_skill_tags_any": 0,
        "weapon_class_mask": 65535,
        "equip_slot_mask": 2,
        "node_id_whitelist": []
      },
      "constraints": {
        "exclusive_group": 0,
        "max_active": 0
      },
      "ops": [
        {
          "opcode": "ADD_SKILL_LEVEL",
          "target": "skill",
          "param_u32": 1,
          "param_f32": 2.0
        },
        {
          "opcode": "MANA_COST_MULT",
          "target": "skill",
          "param_u32": 1,
          "param_f32": 0.9
        }
      ],
      "debug": {
        "name": "FlowingThrust_AscendedEdge",
        "source": "equipment_affix"
      }
    }
  ]
}
```

```json
{
  "schema_version": 2,
  "domain": "skill_spec",
  "records": [
    {
      "id": 2002103,
      "domain": "skill_spec",
      "priority": 200,
      "filters": {
        "profession_mask": 1,
        "skill_id_whitelist": [2],
        "required_skill_tags_all": 1,
        "forbidden_skill_tags_any": 0,
        "weapon_class_mask": 65535,
        "equip_slot_mask": 0,
        "node_id_whitelist": [213]
      },
      "constraints": {
        "exclusive_group": 0,
        "max_active": 0
      },
      "ops": [
        {
          "opcode": "ADD_STAT_PERCENT_MULT",
          "target": "damage",
          "param_u32": 9,
          "param_f32": 0.22
        }
      ],
      "debug": {
        "name": "HeavyMomentum_Node213",
        "source": "skill_spec_node"
      }
    }
  ]
}
```

```json
{
  "schema_version": 2,
  "domain": "talent",
  "records": [
    {
      "id": 3001100,
      "domain": "talent",
      "priority": 300,
      "filters": {
        "profession_mask": 1,
        "skill_id_whitelist": [],
        "required_skill_tags_all": 0,
        "forbidden_skill_tags_any": 0,
        "weapon_class_mask": 65535,
        "equip_slot_mask": 0,
        "node_id_whitelist": [1100]
      },
      "constraints": {
        "exclusive_group": 0,
        "max_active": 0
      },
      "ops": [
        {
          "opcode": "ADD_STAT_FLAT",
          "target": "character",
          "param_u32": 4,
          "param_f32": 50.0
        }
      ],
      "debug": {
        "name": "Astrolabe_Origin_Health",
        "source": "talent_node"
      }
    }
  ]
}
```

### Task 1: Scaffold schema V2 and sample content

**Files:**
- Create: `assets/data/modifier_v2/modifier_catalog.json`
- Create: `assets/data/modifier_v2/equipment_modifiers.json`
- Create: `assets/data/modifier_v2/skill_spec_modifiers.json`
- Create: `assets/data/modifier_v2/talent_modifiers.json`
- Modify: `scripts/validate_json.py`
- Test: `tests/unit/ModifierSchemaV2ValidationTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierSchemaV2 - Required top-level fields") {
  const std::string bad = R"({"domain":"equipment"})";
  CHECK_THROWS_WITH_AS(
      ValidateModifierSchemaV2Json(bad),
      doctest::Contains("schema_version"),
      std::runtime_error);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierSchemaV2 - Required top-level fields"`
Expected: FAIL with unresolved symbol or missing validator function.

**Step 3: Write minimal implementation**

```cpp
void ValidateModifierSchemaV2Json(std::string_view text) {
  const auto root = nlohmann::json::parse(text);
  if (!root.contains("schema_version")) {
    throw std::runtime_error("modifier schema missing schema_version");
  }
}
```

Also add the three sample records above into the three domain files and add them to catalog.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierSchemaV2 - Required top-level fields"`
Expected: PASS.

**Step 5: Commit**

```bash
git add assets/data/modifier_v2 scripts/validate_json.py tests/unit/ModifierSchemaV2ValidationTests.cpp
git commit -m "feat: add modifier schema v2 seed files and validation"
```

### Task 2: Add binary contract types and header invariants

**Files:**
- Create: `src/game/systems/modifier/ModifierRuntimeTypes.hpp`
- Test: `tests/unit/ModifierRuntimeTypesTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierRuntimeTypes - Header layout is stable") {
  CHECK(sizeof(ModifierRuntimeHeader) == 64);
  CHECK(ModifierRuntimeHeader::kMagic == 0x4D444D4Eu);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierRuntimeTypes - Header layout is stable"`
Expected: FAIL because header/types are missing.

**Step 3: Write minimal implementation**

```cpp
struct ModifierRuntimeHeader {
  static constexpr uint32_t kMagic = 0x4D444D4Eu; // "NMDM"
  uint32_t magic = kMagic;
  uint16_t format_version = 2;
  uint16_t endian = 1;
  uint32_t record_count = 0;
  uint32_t filter_count = 0;
  uint32_t op_count = 0;
  uint32_t index_count = 0;
  uint32_t records_offset = 0;
  uint32_t filters_offset = 0;
  uint32_t ops_offset = 0;
  uint32_t index_offset = 0;
  uint32_t crc32 = 0;
  uint8_t reserved[20] = {};
};
static_assert(sizeof(ModifierRuntimeHeader) == 64);
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierRuntimeTypes - Header layout is stable"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/ModifierRuntimeTypes.hpp tests/unit/ModifierRuntimeTypesTests.cpp
git commit -m "feat: add modifier runtime binary type contracts"
```

### Task 3: Build compiler script with deterministic binary output

**Files:**
- Create: `scripts/gen_modifier_runtime_v2.py`
- Create: `assets/generated/.gitkeep`
- Test: `tests/unit/ModifierCompilerDeterminismTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierCompiler - Deterministic bytes for same input") {
  auto a = CompileModifierFixtureToBytes("tests/fixtures/modifier_v2/basic");
  auto b = CompileModifierFixtureToBytes("tests/fixtures/modifier_v2/basic");
  CHECK(a == b);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierCompiler - Deterministic bytes for same input"`
Expected: FAIL (compiler function/script missing).

**Step 3: Write minimal implementation**

```python
def compile_runtime_blob(records):
    records = sorted(records, key=lambda r: (r["priority"], r["id"]))
    # pack with struct '<IHHIIIIIIIII20s'
    return header_bytes + records_bytes + filters_bytes + ops_bytes + index_bytes
```

Include commands in script:
- `--check`
- `--check-determinism`

**Step 4: Run test to verify it passes**

Run: `python scripts/gen_modifier_runtime_v2.py --check && python scripts/gen_modifier_runtime_v2.py --check-determinism`
Expected: exit code `0`, log includes `determinism: ok`.

**Step 5: Commit**

```bash
git add scripts/gen_modifier_runtime_v2.py assets/generated/.gitkeep tests/unit/ModifierCompilerDeterminismTests.cpp
git commit -m "feat: add modifier runtime compiler with deterministic output"
```

### Task 4: Add runtime registry loader and integrity checks

**Files:**
- Create: `src/game/systems/modifier/ModifierRuntimeRegistry.hpp`
- Create: `src/game/systems/modifier/ModifierRuntimeRegistry.cpp`
- Test: `tests/unit/ModifierRuntimeRegistryTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierRuntimeRegistry - Rejects bad magic") {
  auto blob = BuildBlobWithMagic(0x12345678u);
  ModifierRuntimeRegistry reg;
  CHECK_FALSE(reg.LoadFromBytes(blob));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierRuntimeRegistry - Rejects bad magic"`
Expected: FAIL (registry missing).

**Step 3: Write minimal implementation**

```cpp
bool ModifierRuntimeRegistry::LoadFromBytes(std::span<const uint8_t> bytes) {
  if (bytes.size() < sizeof(ModifierRuntimeHeader)) return false;
  const auto* h = reinterpret_cast<const ModifierRuntimeHeader*>(bytes.data());
  if (h->magic != ModifierRuntimeHeader::kMagic) return false;
  if (h->format_version != 2) return false;
  return ValidateCrc32(bytes, *h);
}
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierRuntimeRegistry - Rejects bad magic"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/ModifierRuntimeRegistry.hpp src/game/systems/modifier/ModifierRuntimeRegistry.cpp tests/unit/ModifierRuntimeRegistryTests.cpp
git commit -m "feat: add modifier runtime registry loader and integrity checks"
```

### Task 5: Implement evaluator core (filters + stat ops)

**Files:**
- Create: `src/game/systems/modifier/ModifierContext.hpp`
- Create: `src/game/systems/modifier/ModifierEvaluator.hpp`
- Create: `src/game/systems/modifier/ModifierEvaluator.cpp`
- Test: `tests/unit/ModifierEvaluatorTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] ModifierEvaluator - Applies ADD_STAT_FLAT when filters match") {
  ModifierEvalContext ctx;
  ctx.profession_id = 0;
  ctx.skill_id = 1;
  ctx.skill_tags = Tag::Hit;
  const float before = 100.0f;
  const float after = EvalFixtureAddFlat(before, ctx);
  CHECK(after == doctest::Approx(150.0f));
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierEvaluator - Applies ADD_STAT_FLAT when filters match"`
Expected: FAIL (evaluator missing).

**Step 3: Write minimal implementation**

```cpp
if (!MatchesFilters(record.filter, ctx)) {
  continue;
}
switch (op.opcode) {
case OpCode::ADD_STAT_FLAT:
  out.AddFlat(static_cast<StatType>(op.param_u32), op.param_f32);
  break;
case OpCode::ADD_STAT_PERCENT_ADD:
  out.AddPercentAdd(static_cast<StatType>(op.param_u32), op.param_f32);
  break;
default:
  break;
}
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] ModifierEvaluator*"`
Expected: PASS for new evaluator tests.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/ModifierContext.hpp src/game/systems/modifier/ModifierEvaluator.hpp src/game/systems/modifier/ModifierEvaluator.cpp tests/unit/ModifierEvaluatorTests.cpp
git commit -m "feat: add modifier evaluator core with filter matching"
```

### Task 6: Integrate equipment adapter and remove hardcoded affix behavior path

**Files:**
- Create: `src/game/systems/modifier/EquipmentModifierAdapter.hpp`
- Create: `src/game/systems/modifier/EquipmentModifierAdapter.cpp`
- Modify: `src/game/systems/item/ItemFactory.cpp`
- Modify: `src/game/systems/stats/AttributePipeline.cpp`
- Test: `tests/unit/EquipmentModifierAdapterTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] EquipmentModifierAdapter - requiredSkillTags gates skill-level bonus") {
  auto result = RunEquipmentFixture(/*skill tags without Hit*/);
  CHECK(result.plus_skill_levels == 0);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] EquipmentModifierAdapter*"`
Expected: FAIL, old path does not use V2 filtering.

**Step 3: Write minimal implementation**

```cpp
ModifierEvalContext ctx = BuildContextFromCharacter(registry, entity, skillId, skillTags);
const auto view = EquipmentModifierAdapter::CollectEquippedRecordIds(registry, entity);
const auto delta = ModifierEvaluator::Evaluate(runtimeRegistry, view, ctx);
ApplyModifierDeltaToStats(delta, statsAccumulator);
```

In `ItemFactory.cpp`, map `requiredSkillTags` to V2 `required_skill_tags_all` during content migration.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] EquipmentModifierAdapter*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/EquipmentModifierAdapter.hpp src/game/systems/modifier/EquipmentModifierAdapter.cpp src/game/systems/item/ItemFactory.cpp src/game/systems/stats/AttributePipeline.cpp tests/unit/EquipmentModifierAdapterTests.cpp
git commit -m "feat: migrate equipment affix effects to modifier runtime v2"
```

### Task 7: Integrate skill specialization adapter

**Files:**
- Create: `src/game/systems/modifier/SkillSpecModifierAdapter.hpp`
- Create: `src/game/systems/modifier/SkillSpecModifierAdapter.cpp`
- Modify: `src/game/systems/skill/SkillSystem.cpp`
- Modify: `src/game/data/SkillRegistry.cpp`
- Test: `tests/unit/SkillSpecModifierAdapterTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] SkillSpecModifierAdapter - node 213 applies HeavyMomentum op") {
  const float baseline = RunSpecFixture({});
  const float boosted = RunSpecFixture({213});
  CHECK(boosted > baseline);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] SkillSpecModifierAdapter*"`
Expected: FAIL (adapter missing).

**Step 3: Write minimal implementation**

```cpp
auto ids = SkillSpecModifierAdapter::CollectAllocatedNodeRecordIds(activeSkillSlot);
auto delta = ModifierEvaluator::Evaluate(runtimeRegistry, ids, ctx);
ApplySkillDelta(delta, skillRuntimeState);
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] SkillSpecModifierAdapter*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/SkillSpecModifierAdapter.hpp src/game/systems/modifier/SkillSpecModifierAdapter.cpp src/game/systems/skill/SkillSystem.cpp src/game/data/SkillRegistry.cpp tests/unit/SkillSpecModifierAdapterTests.cpp
git commit -m "feat: migrate skill specialization effects to modifier runtime v2"
```

### Task 8: Integrate talent adapter

**Files:**
- Create: `src/game/systems/modifier/TalentModifierAdapter.hpp`
- Create: `src/game/systems/modifier/TalentModifierAdapter.cpp`
- Modify: `src/game/systems/skill/AstrolabeSystem.cpp`
- Modify: `src/game/systems/stats/AttributePipeline.cpp`
- Test: `tests/unit/TalentModifierAdapterTests.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Unit] TalentModifierAdapter - active node 1100 grants flat health") {
  const float base = RunTalentFixture(false);
  const float withNode = RunTalentFixture(true);
  CHECK(withNode > base);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] TalentModifierAdapter*"`
Expected: FAIL (adapter missing).

**Step 3: Write minimal implementation**

```cpp
auto ids = TalentModifierAdapter::CollectActiveNodeRecordIds(astrolabeComp);
auto delta = ModifierEvaluator::Evaluate(runtimeRegistry, ids, ctx);
ApplyModifierDeltaToStats(delta, statsAccumulator);
```

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] TalentModifierAdapter*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/game/systems/modifier/TalentModifierAdapter.hpp src/game/systems/modifier/TalentModifierAdapter.cpp src/game/systems/skill/AstrolabeSystem.cpp src/game/systems/stats/AttributePipeline.cpp tests/unit/TalentModifierAdapterTests.cpp
git commit -m "feat: migrate astrolabe talent effects to modifier runtime v2"
```

### Task 9: Wire compiler into precheck and runtime bootstrap

**Files:**
- Modify: `scripts/validate_json.py`
- Modify: `build.bat`
- Modify: `src/game/core/GameInit.cpp`
- Test: `tests/integration/ModifierRuntimeBootstrapIntegrationTest.cpp`

**Step 1: Write the failing test**

```cpp
TEST_CASE("[Integration] ModifierRuntimeV2 - boot loads binary and evaluates sample") {
  CHECK(ModifierRuntimeRegistry::Get().EnsureLoaded());
  CHECK(ModifierRuntimeRegistry::Get().RecordCount() > 0);
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Integration] ModifierRuntimeV2*"`
Expected: FAIL (bootstrap not wired).

**Step 3: Write minimal implementation**

```cpp
if (!ModifierRuntimeRegistry::Get().EnsureLoaded("assets/generated/modifier_runtime_v2.bin")) {
  LOG_FATAL("ModifierRuntimeV2 load failed");
  return false;
}
```

Also invoke:
- `python scripts/gen_modifier_runtime_v2.py --check` from `build.bat` precheck path.

**Step 4: Run test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Integration] ModifierRuntimeV2*"`
Expected: PASS.

**Step 5: Commit**

```bash
git add scripts/validate_json.py build.bat src/game/core/GameInit.cpp tests/integration/ModifierRuntimeBootstrapIntegrationTest.cpp
git commit -m "chore: wire modifier runtime compiler and bootstrap checks"
```

### Task 10: Remove legacy execution branches and finish verification

**Files:**
- Modify: `src/game/components/ItemStats.hpp`
- Modify: `src/game/systems/stats/AttributePipeline.cpp`
- Modify: `src/game/systems/skill/SkillSystem.cpp`
- Modify: `src/game/systems/skill/AstrolabeSystem.cpp`
- Test: `tests/integration/GameplaySystems.cpp`
- Test: `tests/unit/AttributePipelineTest.cpp`

**Step 1: Write the failing regression test**

```cpp
TEST_CASE("[Integration] ModifierRuntimeV2 - legacy and v2 paths are not both active") {
  CHECK_FALSE(IsLegacyAffixPathEnabled());
}
```

**Step 2: Run test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Integration] ModifierRuntimeV2 - legacy and v2 paths are not both active"`
Expected: FAIL because old path is still enabled.

**Step 3: Write minimal implementation**

```cpp
constexpr bool kEnableLegacyModifierPath = false;
// Remove old switch/case execution and route all modifier logic through V2 bridge.
```

**Step 4: Run full relevant verification**

Run: `./build.bat`
Expected: build success.

Run: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
Expected: unit suite PASS.

Run: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
Expected: integration suite PASS.

**Step 5: Commit**

```bash
git add src/game/components/ItemStats.hpp src/game/systems/stats/AttributePipeline.cpp src/game/systems/skill/SkillSystem.cpp src/game/systems/skill/AstrolabeSystem.cpp tests/integration/GameplaySystems.cpp tests/unit/AttributePipelineTest.cpp
git commit -m "refactor: remove legacy modifier branches and finalize v2 runtime path"
```

## Manual test matrix (must run before merge)

1. Equip/unequip loop: repeatedly swap 10 items; verify panel stats and combat DPS update immediately.
2. Skill-specialization toggle: respec node 213 on/off; verify expected damage delta and no stale state.
3. Talent point allocation/refund: apply node 1100 and refund; verify max health round-trips.
4. Tag-gated affix check: skill without required tag gets no bonus; adding tag activates bonus.
5. Save/load regression: save in town, reload, verify all V2 effects preserved and deterministic.

## Final verification evidence to collect

- `./build.bat` output showing success.
- Unit + integration CTest summaries.
- `python scripts/gen_modifier_runtime_v2.py --check-determinism` output.
- One debug dump snippet from `assets/generated/modifier_runtime_v2.debug.json`.

## Definition of done

- All player-side modifier effects are evaluated by Modifier Runtime V2.
- No legacy dual execution path remains.
- Binary compiler is deterministic and checked by precheck.
- Unit/integration tests pass and manual matrix is signed off.
