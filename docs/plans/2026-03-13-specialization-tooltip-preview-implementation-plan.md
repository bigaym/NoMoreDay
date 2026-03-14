# Specialization Tooltip Preview Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a reusable static display-preview pipeline for specialization skill tooltips, surface the global duration stat in the character panel, and upgrade specialization node tooltips with quantitative lines for the current Blood Sea-driven first slice.

**Architecture:** Keep combat truth unchanged and add a separate build-preview layer that reads static player stats plus active specialization/passive allocations. Reuse existing `CombatStats::duration_scale`, `UISkillTalentTree.cpp`, and `UIRenderer::DrawSkillTooltip`, but stop computing tooltip damage inline from a synthetic range and stop leaving specialization node value output as a generic qualitative-only footer.

**Tech Stack:** C++20, EnTT, raylib UI rendering, nlohmann JSON, doctest, CTest, existing `SkillSpecModifierAdapter` / stat pipeline plumbing.

---

### Task 1: Add specialization tooltip display metadata contract

**Files:**
- Modify: `src/game/components/SkillDefs.hpp`
- Modify: `tests/unit/SkillPrerequisiteRequiredPointsTests.cpp`
- Verify context: `assets/data/mastery_skill_trees.json`

**Step 1: Write the failing JSON contract test**

Add a new doctest that proves `TalentNode` can parse and round-trip explicit quantitative tooltip metadata for behavior-heavy nodes.

```cpp
TEST_CASE("[Unit] TalentNode JSON - display_lines parse quantitative tooltip metadata") {
  const nlohmann::json nodeJson = {
      {"id", 900201},
      {"name_key", "display_node"},
      {"desc_key", "display_node_desc"},
      {"max_points", 3},
      {"display_lines", nlohmann::json::array({
          {{"label", "持续时间"}, {"per_point", 10.0f}, {"is_percent", true}},
          {{"label", "脉冲频率"}, {"per_point", 8.0f}, {"is_percent", true}}
      })}
  };

  const TalentNode node = nodeJson.get<TalentNode>();
  REQUIRE(node.display_lines.size() == 2);
  CHECK(node.display_lines[0].label == "持续时间");
  CHECK(node.display_lines[0].per_point == doctest::Approx(10.0f));
  CHECK(node.display_lines[0].is_percent);
}
```

**Step 2: Run the narrow test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] TalentNode JSON - display_lines parse quantitative tooltip metadata"`

Expected: compile or test failure because `TalentNode` does not yet expose `display_lines`.

**Step 3: Write the minimal implementation**

Extend `TalentNode` with a lightweight display metadata record that supports per-point scaling without teaching UI code to parse freeform strings.

```cpp
struct TalentDisplayLine {
  std::string label;
  float base_value = 0.0f;
  float per_point = 0.0f;
  bool is_percent = false;
  std::string suffix;
};

struct TalentNode {
  ...
  std::vector<TalentDisplayLine> display_lines;
};
```

Add `to_json` / `from_json` coverage in `SkillDefs.hpp` so the metadata stays data-driven.

**Step 4: Run the narrow test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] TalentNode JSON - display_lines parse quantitative tooltip metadata"`

Expected: the new JSON contract test passes.

**Step 5: Commit**

```bash
git add src/game/components/SkillDefs.hpp tests/unit/SkillPrerequisiteRequiredPointsTests.cpp
git commit -m "feat: add specialization tooltip display metadata"
```

### Task 2: Surface the global duration stat in the character panel

**Files:**
- Modify: `src/game/systems/ui/UICharacter.cpp`
- Modify: `tests/tech/UITests.cpp`
- Verify context: `src/game/components/Stats.hpp`
- Verify context: `src/game/systems/combat/StatsSystem.cpp`

**Step 1: Write the failing tech guard**

Add a source-level UI tech test that requires `UICharacter.cpp` to display a duration row sourced from `combatStats->duration_scale` and place it in the skill-scaling cluster near cooldown reduction and area scale.

```cpp
TEST_CASE("[Tech] CharacterPanel - duration scale row is visible") {
  const std::string source = ReadSource("src/game/systems/ui/UICharacter.cpp");
  REQUIRE(!source.empty());
  CHECK(source.find("技能持续时间") != std::string::npos);
  CHECK(source.find("combatStats->duration_scale") != std::string::npos);
  CHECK(source.find("冷却缩减") < source.find("技能持续时间"));
  CHECK(source.find("技能持续时间") < source.find("技能范围"));
}
```

**Step 2: Run the narrow test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] CharacterPanel - duration scale row is visible"`

Expected: the tech guard fails because `UICharacter.cpp` does not yet render the duration stat.

**Step 3: Write the minimal implementation**

Add the row in the existing `回复与辅助 / 综合属性` block directly after `冷却缩减` and before `技能范围` so the skill-affecting rows stay grouped.

```cpp
DrawStatRow("技能持续时间",
            TextFormat("%.0f%%", (combatStats->duration_scale - 1.0f) * 100.0f),
            rowX, y, rowW, 20.0f, alpha);
```

Keep the row purely build-facing; do not introduce runtime buff inspection into the character panel. Use the same row primitive, alignment, and spacing as neighboring stats, and format it as a signed build modifier (`+X%`, `-X%`, `0%`) rather than a duration-in-seconds explanation.

**Step 4: Run the narrow test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] CharacterPanel - duration scale row is visible"`

Expected: the new tech guard passes.

**Step 5: Commit**

```bash
git add src/game/systems/ui/UICharacter.cpp tests/tech/UITests.cpp
git commit -m "feat: surface duration scale in character panel"
```

### Task 3: Introduce the static skill display-preview service

**Files:**
- Create: `src/game/systems/skill/SkillDisplayPreviewService.hpp`
- Create: `src/game/systems/skill/SkillDisplayPreviewService.cpp`
- Create: `tests/unit/SkillDisplayPreviewTests.cpp`
- Verify context: `src/game/data/SkillRegistry.hpp`
- Verify context: `src/game/systems/modifier/SkillSpecModifierAdapter.hpp`
- Verify context: `tests/unit/TalentModifierTest.cpp`

**Step 1: Write the failing unit tests**

Create focused tests for the preview rules:

- duration preview multiplies `field_duration` by static `CombatStats::duration_scale` and explicit specialization metadata only;
- estimated damage produces a single value plus mode, never a min/max range;
- live temporary state is not required as input.

```cpp
TEST_CASE("[Unit] SkillDisplayPreview - duration preview uses static sources only") {
  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& combat = registry.emplace<CombatStats>(player);
  combat.duration_scale = 1.25f;

  SkillData skill{.id = 990100, .name_key = "preview_skill", .desc_key = "desc",
                  .mana_cost = 0.0f, .cooldown = 0.0f, .base_damage = 40.0f,
                  .weapon_damage_mult = 1.0f};
  skill.params["field_duration"] = 4.8f;
  SkillRegistry::Get().RegisterSkill(skill);

  const auto preview = SkillDisplayPreviewService::Build(registry, player, 990100);
  CHECK(preview.has_duration);
  CHECK(preview.display_duration_seconds == doctest::Approx(6.0f));
}
```

**Step 2: Run the narrow tests to verify they fail**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] SkillDisplayPreview -*"`

Expected: compile failure because the new service does not exist yet.

**Step 3: Write the minimal implementation**

Create a dedicated service and payload.

```cpp
enum class SkillDisplayDamageMode { Hit, PerSecond, Total, ChannelWindow };

struct SkillDisplayPreview {
  bool has_duration = false;
  float display_duration_seconds = 0.0f;
  bool has_estimated_damage = false;
  float estimated_damage_value = 0.0f;
  SkillDisplayDamageMode estimated_damage_mode = SkillDisplayDamageMode::Hit;
};

class SkillDisplayPreviewService {
public:
  static SkillDisplayPreview Build(entt::registry& registry,
                                   entt::entity player,
                                   uint32_t skillId);
};
```

Implementation rules:

- read baseline duration from standardized skill params such as `field_duration`;
- use `CombatStats` for static global stats only;
- overlay specialization-specific preview modifiers locally instead of assuming skill-tree modifiers are already baked into `CombatStats` (`TalentModifierTest` shows the legacy skill-tree stat path is disabled);
- reuse `SkillSpecModifierAdapter` where it cleanly maps runtime-record-driven specialization multipliers to preview damage.

**Step 4: Run the narrow tests to verify they pass**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] SkillDisplayPreview -*"`

Expected: the preview unit tests pass and the service exposes single-value damage preview data.

**Step 5: Commit**

```bash
git add src/game/systems/skill/SkillDisplayPreviewService.hpp src/game/systems/skill/SkillDisplayPreviewService.cpp tests/unit/SkillDisplayPreviewTests.cpp
git commit -m "feat: add static specialization skill preview service"
```

### Task 4: Wire skill tooltips to the static preview payload

**Files:**
- Modify: `src/engine/render/UIRenderer.cpp`
- Modify: `src/engine/render/UIRenderer.hpp`
- Modify: `tests/tech/UITests.cpp`
- Verify context: `src/game/systems/ui/UISystem.cpp`
- Verify context: `src/game/systems/skill/SkillDisplayPreviewService.hpp`

**Step 1: Write the failing tech guard**

Add source-level coverage that requires `DrawSkillTooltip` to consume the preview service, render a duration row, keep the fixed stat-row order, resolve damage labels centrally, and stop building the old `0.9x ~ 1.1x` synthetic damage range.

```cpp
TEST_CASE("[Tech] SkillUI - tooltip uses static preview payload") {
  const std::string source = ReadSource("src/engine/render/UIRenderer.cpp");
  REQUIRE(!source.empty());
  CHECK(source.find("SkillDisplayPreviewService::Build") != std::string::npos);
  CHECK(source.find("持续时间") != std::string::npos);
  CHECK(source.find("ResolveDamageLabel(") != std::string::npos);
  CHECK(source.find("0.9f") == std::string::npos);
  CHECK(source.find("1.1f") == std::string::npos);
}
```

**Step 2: Run the narrow test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - tooltip uses static preview payload"`

Expected: the tech guard fails because `UIRenderer.cpp` still formats a min/max damage range inline.

**Step 3: Write the minimal implementation**

Replace the inline preview logic in `DrawSkillTooltip`.

```cpp
const SkillDisplayPreview preview =
    SkillDisplayPreviewService::Build(registry, playerView.front(), skillId);

if (preview.has_duration) {
  utils::FormatToBuffer(buf, "{:.1f}s", preview.display_duration_seconds);
  coreStats.push_back({"持续时间", buf, WHITE});
}

if (preview.has_estimated_damage) {
  utils::FormatToBuffer(buf, "{:.0f}", preview.estimated_damage_value);
  coreStats.push_back({ResolveDamageLabel(preview.estimated_damage_mode), buf,
                       {255, 150, 50, 255}});
}
```

Implementation rules:

- keep stat-row order fixed as `法力消耗 -> 冷却时间 -> 持续时间 -> damage preview row last`;
- centralize label mapping in a helper such as `ResolveDamageLabel(...)` so `Hit` / `PerSecond` / `Total` / `ChannelWindow` wording cannot drift by callsite;
- omit duration or damage rows cleanly when preview data is unavailable;
- do not keep the old `avgWeapon -> DamagePipeline -> +/-10%` display range fallback;
- keep duration visually neutral and damage-value emphasis restrained; do not add new glow, badge, or animation treatment.

**Step 4: Run the narrow test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - tooltip uses static preview payload"`

Expected: the tech guard passes and the tooltip path is preview-service-driven.

**Step 5: Commit**

```bash
git add src/engine/render/UIRenderer.cpp src/engine/render/UIRenderer.hpp tests/tech/UITests.cpp
git commit -m "feat: route skill tooltip stats through static preview"
```

### Task 5: Render quantitative specialization node lines

**Files:**
- Modify: `src/game/systems/ui/UISkillTalentTree.cpp`
- Modify: `tests/tech/UITests.cpp`
- Verify context: `src/game/components/SkillDefs.hpp`
- Verify context: `src/game/systems/modifier/SkillSpecModifierAdapter.hpp`

**Step 1: Write the failing tooltip tech guard**

Add a source-level guard that requires a dedicated quantitative-line builder, a dedicated quantitative section/layout budget, and removal of the generic `数值加成已启用` footer placeholder.

```cpp
TEST_CASE("[Tech] SkillUI - specialization tooltip renders quantitative lines") {
  const std::string source = ReadSource("src/game/systems/ui/UISkillTalentTree.cpp");
  REQUIRE(!source.empty());
  CHECK(source.find("BuildNodeQuantitativeLines(") != std::string::npos);
  CHECK(source.find("quantitative") != std::string::npos);
  CHECK(source.find("display_lines") != std::string::npos);
  CHECK(source.find("数值加成已启用") == std::string::npos);
}
```

**Step 2: Run the narrow test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - specialization tooltip renders quantitative lines"`

Expected: the tech guard fails because the tooltip footer still emits the generic placeholder line.

**Step 3: Write the minimal implementation**

Inside `UISkillTalentTree.cpp`, add a helper that merges:

- auto-generated lines from stable `stat_modifiers` / `damage_modifiers` mappings;
- explicit `display_lines` for behavior-heavy nodes;
- scaling by current invested points when applicable.

```cpp
std::vector<std::pair<std::string, Color>> BuildNodeQuantitativeLines(
    const TalentNode& node,
    const SpecializedSkill& specialized,
    uint32_t hoveredNodeId) {
  // 1. format stable stat/damage modifiers
  // 2. append explicit node.display_lines fallback entries
  // 3. dedupe equivalent lines
  // 4. scale values using the chosen invested-state rule
}
```

Implementation rules:

- render a distinct quantitative payoff block between the qualitative description and the warning/footer section;
- update tooltip layout metrics so the new block gets explicit height budget instead of stealing unreadably from the description/footer area;
- keep warning, exclusion, and cost lines in the final footer section only;
- order quantitative lines by decision value: damage/output, duration, frequency/count, area/range, resource/cooldown, then others;
- default to showing the 2-3 most useful quantitative lines for dense nodes;
- if uninvested nodes show preview values, treat them consistently as next-point preview values everywhere rather than as current active totals;
- if `display_lines` and auto-generated lines describe the same payoff, keep one canonical visible line.

**Step 4: Run the narrow test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - specialization tooltip renders quantitative lines"`

Expected: the tooltip source guard passes.

**Step 5: Commit**

```bash
git add src/game/systems/ui/UISkillTalentTree.cpp tests/tech/UITests.cpp
git commit -m "feat: add quantitative specialization tooltip lines"
```

### Task 6: Populate the first Blood Sea preview data slice

**Files:**
- Modify: `assets/data/skills.json`
- Modify: `assets/data/mastery_skill_trees.json`
- Modify: `tests/unit/SkillRegistryMasteryTreeTests.cpp`
- Verify context: `assets/data/blade_masteries.json`

**Step 1: Write the failing data coverage test**

Add a unit test that loads real data and checks the first specialization slice has the required preview metadata.

```cpp
TEST_CASE("[Unit] Skill Registry - Blood Sea specialization preview data is present") {
  auto& registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  const SkillData* bloodSea = registry.GetSkill(12);
  REQUIRE(bloodSea != nullptr);
  CHECK(bloodSea->GetParam("field_duration", 0.0f) > 0.0f);

  const SkillTreeDefinition* tree = registry.GetSkillTree(12);
  REQUIRE(tree != nullptr);
  CHECK(tree->nodes.at(1200).display_lines.size() > 0);
  CHECK(tree->nodes.at(1219).display_lines.size() > 0);
}
```

**Step 2: Run the narrow test to verify it fails**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] Skill Registry - Blood Sea specialization preview data is present"`

Expected: the test fails because the current mastery tree nodes do not yet define `display_lines`.

**Step 3: Write the minimal data update**

Populate the first slice only.

- keep Blood Sea skill `id = 12` on standardized `field_duration` preview data;
- add quantitative `display_lines` for the current high-value Blood Sea nodes, especially nodes that are still behavior-heavy in pure text form such as `1200`, `1216`, `1219`, `1221`, `1222`, and `1223`;
- use per-point metadata where the node scales with invested points.

Example JSON shape:

```json
{
  "id": 1219,
  "name_key": "久驻血雾",
  "display_lines": [
    { "label": "持续时间", "per_point": 10.0, "is_percent": true }
  ]
}
```

**Step 4: Run the narrow test to verify it passes**

Run: `./build.bat && ./bin/NoMoreDayTests.exe --test-case="[Unit] Skill Registry - Blood Sea specialization preview data is present"`

Expected: the real-data preview coverage test passes.

**Step 5: Commit**

```bash
git add assets/data/skills.json assets/data/mastery_skill_trees.json tests/unit/SkillRegistryMasteryTreeTests.cpp
git commit -m "feat: add Blood Sea specialization preview data"
```

### Task 7: Run focused verification and update the bug registry

**Files:**
- Modify: `conductor/bug_registry.md`

**Step 1: Run focused verification for touched preview work**

Run these commands in order:

```bash
./build.bat
./bin/NoMoreDayTests.exe --test-case="[Unit] TalentNode JSON - display_lines parse quantitative tooltip metadata"
./bin/NoMoreDayTests.exe --test-case="[Tech] CharacterPanel - duration scale row is visible"
./bin/NoMoreDayTests.exe --test-case="[Unit] SkillDisplayPreview -*"
./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - tooltip uses static preview payload"
./bin/NoMoreDayTests.exe --test-case="[Tech] SkillUI - specialization tooltip renders quantitative lines"
./bin/NoMoreDayTests.exe --test-case="[Unit] Skill Registry - Blood Sea specialization preview data is present"
```

Expected: build succeeds and every touched focused test passes.

**Step 2: Run repo-required broader verification for touched C++ files**

Run:

```bash
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
```

Expected: the unit suite remains green after the preview/UI changes.

**Step 2.5: Run manual UI verification for hierarchy and clipping**

Verify at least these cases in-game or in the closest available UI harness:

- one duration-bearing skill tooltip shows `持续时间` in the correct stat order;
- one long-description specialization node still preserves readable description space;
- one behavior-heavy node with `display_lines` shows a distinct quantitative block;
- one excluded node shows warnings after quantitative payoff lines;
- one uninvested, one invested, and one maxed node all follow the same point-value interpretation rule.

Expected: no clipping, duplicated quantitative lines, or section-order regressions are visible.

**Step 3: Update the duration-field bug entry with root cause and verification**

Record the final outcome for `BUG-20260313-001` in `conductor/bug_registry.md`, including:

- whether the broken link was the character-panel row, tooltip preview wiring, or both;
- the final fixed file paths;
- the exact verification commands from Steps 1-2.

**Step 4: Commit**

```bash
git add conductor/bug_registry.md
git commit -m "docs: close duration preview wiring bug"
```
