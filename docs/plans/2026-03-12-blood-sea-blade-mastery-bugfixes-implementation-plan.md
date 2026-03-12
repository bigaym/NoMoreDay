# Blood Sea And Blade Mastery Bugfixes Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Fix the five 2026-03-12 Blade Ascendant regressions without expanding scope, covering empowerment gating, Blood Sea readability, HUD clarity, missing glyph-safe text, and specialization tooltip contrast.

**Architecture:** Keep the runtime model intact and repair the narrow failing edges around it. Reuse existing Blade resource HUD and existing world render primitives instead of inventing a new player buff bar or a new shader path; Blood Sea field readability should use a low-risk world-space ring/gradient pulse render first, while tooltip readability should be fixed in the current tooltip header/badge path.

**Tech Stack:** C++20, EnTT, raylib UI/render primitives, existing GPUSkillEffect/RenderSystem plumbing, doctest/CTest.

---

### Task 1: Fix converted-resource auto empowerment gating

**Files:**
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Modify: `tests/integration/SkillSystemTests.cpp`
- Verify context: `src/game/systems/skill/SkillSystem.cpp`

**Step 1: Write the failing regression expectation**

Update the existing converted-resource auto-empower integration coverage so Sword Saint / converted full resource now expects a base skill cast to consume full resource and mark the cast as empowered.

**Step 2: Run the narrow test to verify it fails**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SkillSystem - Sword Flow" --output-on-failure`

Expected: the updated converted-resource empowerment assertion fails on current code.

**Step 3: Write the minimal implementation**

Change `BladeResourceService::ShouldAutoEmpowerOnCast()` so Blade Ascendant converted resources that still mirror the Sword Intent cap can trigger the pre-cast empowerment path instead of only raw `SwordIntent` kind.

**Step 4: Run the narrow test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SkillSystem - Sword Flow" --output-on-failure`

Expected: the converted-resource empowerment case passes.

### Task 2: Fix `???` threshold text above the Blade resource widget

**Files:**
- Modify: `src/game/systems/ui/SwordIntentWidget.cpp`
- Modify: `tests/tech/UITests.cpp`
- Verify context: `src/game/systems/ui/UISystem.cpp`

**Step 1: Write a failing tech guard**

Add a source-level tech test that requires the widget text rows to use the UI font path instead of raw default-font `DrawText` for the label / threshold / detail text above the icon row.

**Step 2: Run the narrow test to verify it fails**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SwordIntentWidget" --output-on-failure`

Expected: the new guard fails because `SwordIntentWidget.cpp` still uses raw `DrawText` / `MeasureText`.

**Step 3: Write the minimal implementation**

Switch those text rows to `UISystem::DrawTextUI` plus width measurement through the active UI font so Chinese threshold text such as Bloodthirst status labels renders with the loaded glyph-capable font.

**Step 4: Run the narrow test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SwordIntentWidget" --output-on-failure`

Expected: the tech guard passes.

### Task 3: Make Blood Sea duration and active capability readable in the current HUD

**Files:**
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Modify: `tests/tech/UITests.cpp`

**Step 1: Write the failing HUD expectation**

Extend `PlayerHUD` tech coverage so active Blood Sea detail text must include remaining duration plus current form / key active capability wording sourced from the live `BloodSeaFieldComponent`.

**Step 2: Run the narrow test to verify it fails**

Run: `ctest --test-dir build -C RelWithDebInfo -R "PlayerHUD - Render Logic" --output-on-failure`

Expected: the new Blood Sea HUD string expectation fails on current wording.

**Step 3: Write the minimal implementation**

Keep the existing resource HUD surface and enrich the Blood Sea runtime detail text so it communicates duration and the highest-value active field state (form and key ability) without inventing a new player buff-bar system.

**Step 4: Run the narrow test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -R "PlayerHUD - Render Logic" --output-on-failure`

Expected: the updated Blood Sea HUD expectation passes.

### Task 4: Improve specialization tooltip title / badge contrast

**Files:**
- Modify: `src/game/systems/ui/UISkillTalentTree.cpp`
- Modify: `tests/tech/UITests.cpp`
- Verify context: `src/game/systems/ui/BladeMasteryUITheme.cpp`

**Step 1: Write the failing contrast guard**

Add a tech test that checks the tooltip title and badge text path no longer render text in near-identical theme/tag colors; require a dedicated readable text treatment instead of reusing the badge outline color for chip text.

**Step 2: Run the narrow test to verify it fails**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SkillUI - tooltip" --output-on-failure`

Expected: the new contrast guard fails on the current tooltip implementation.

**Step 3: Write the minimal implementation**

Adjust tooltip title and badge text colors to a neutral high-contrast treatment while keeping the mastery/tag color language in the fill, stripe, and outline layers.

**Step 4: Run the narrow test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SkillUI - tooltip" --output-on-failure`

Expected: tooltip tech coverage passes.

### Task 5: Add a readable Blood Sea range / pulse render path

**Files:**
- Modify: `src/engine/render/RenderSystem.cpp`
- Modify: `tests/tech/UITests.cpp`
- Verify context: `src/game/systems/skill/behaviors/BloodSea.cpp`

**Step 1: Write the failing structural guard**

Add a tech/source guard that requires a dedicated `BloodSeaFieldComponent` render branch instead of the generic tiny `ColorComponent` fallback only.

**Step 2: Run the narrow test to verify it fails**

Run: `ctest --test-dir build -C RelWithDebInfo -R "Blood Sea" --output-on-failure`

Expected: the new render-path guard fails because only the generic pixel-circle path exists.

**Step 3: Write the minimal implementation**

Render Blood Sea with world primitives: suppress the generic tiny circle for Blood Sea field entities, draw an outer boundary ring at `field.radius`, a low-alpha interior gradient, and a pulse/intensity variation keyed off the field cadence / form so cast success and area coverage are readable without introducing a new shader.

**Step 4: Run the narrow test to verify it passes**

Run: `ctest --test-dir build -C RelWithDebInfo -R "Blood Sea" --output-on-failure`

Expected: the structural render-path guard passes.

### Task 6: Update the bug registry and run focused verification

**Files:**
- Modify: `conductor/bug_registry.md`

**Step 1: Update bug entries after code is verified**

Record root cause, fix summary, and exact verification commands for `BUG-20260312-001` through `BUG-20260312-005`.

**Step 2: Run focused verification**

Run: `ctest --test-dir build -C RelWithDebInfo -R "SkillSystem - Sword Flow|PlayerHUD - Render Logic|SkillUI - tooltip|Blood Sea|SwordIntentWidget" --output-on-failure`

Expected: all touched focused tests pass.

**Step 3: Run repo-required broader verification for touched C++ files**

Run: `./build.bat` and then `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`

Expected: build succeeds and relevant unit suite remains green.
