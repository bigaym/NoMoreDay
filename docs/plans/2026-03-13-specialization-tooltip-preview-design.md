# Specialization Tooltip Preview Design

> Date: 2026-03-13
> Status: approved
> Scope: unified static display-preview rules for specialization skill tooltips, specialization node quantitative tooltip lines, and the character-panel duration field wiring needed to support them.

---

## 1. Background

The current specialization presentation has three gaps that undermine build planning readability.

- Skill tooltips do not show a duration row for duration-bearing skills such as summons, formations, domains, and channelled skills.
- Skill tooltip estimated damage is currently produced from the runtime damage pipeline and a synthetic display range, which makes it too coupled to full combat state and no longer aligned with the product direction now that base-damage ranges have been removed.
- Specialization node tooltips are mostly qualitative. They explain intent, but they do not expose short quantitative payoff lines in the style players expect from ARPG specialization trees.

The codebase already contains enough low-level pieces to support a cleaner solution.

- `CombatStats::duration_scale` already exists and is a natural source for a global duration display stat.
- `UIRenderer::DrawSkillTooltip` already centralizes skill-tooltip drawing.
- `UISkillTalentTree.cpp` already owns specialization-node tooltip rendering and already understands node contracts, badges, and per-node state.
- `TalentNode` already stores `stat_modifiers` and `damage_modifiers`, which creates a path for auto-generated quantitative lines when the data is sufficiently structured.

The design should therefore add a dedicated static display-preview layer rather than continuing to let UI code infer preview values from full runtime behavior.

## 2. Goals

### 2.1 Goals

- Add a `持续时间` row to skill tooltips for skills with duration semantics.
- Rework skill tooltip `预估伤害` so it uses a stable static preview rule and displays a single value instead of an artificial range.
- Limit both duration and estimated-damage preview calculations to static build inputs only: character-panel global values, active specialization-node allocations, and passive tree bonuses.
- Keep runtime-only factors out of tooltip previews.
- Upgrade specialization-node tooltips from purely qualitative descriptions to a mixed format: qualitative explanation first, quantitative short lines second.
- Make the solution reusable across professions, while only requiring first-pass data coverage for the current specialization flow.

### 2.2 Non-goals

- Do not change real combat resolution or runtime damage behavior.
- Do not turn tooltip preview into a live combat snapshot.
- Do not require every historical skill and specialization node in the game to be fully quantified in the first slice.
- Do not invent fake values for behavior-heavy nodes that cannot yet be expressed stably.

## 3. Display Boundary Rules

Tooltip preview values are build-preview values, not combat-snapshot values.

Included sources:

- character-panel static global values;
- current skill base parameters;
- active specialization allocations for the hovered skill;
- passive tree bonuses that are always-on for the character build.

Excluded sources:

- temporary buffs and auras;
- enemy state, resistance, vulnerability, or debuffs;
- hit-confirm stacks and on-hit/on-kill state;
- transient summoned-entity inheritance or scene-state-dependent scaling;
- target-count variance, chain-count variance, and map-specific temporary modifiers;
- random display wobble or synthetic min/max damage ranges.

This boundary keeps tooltips explainable and stable. The value shown should answer "what does this build do by default?" rather than "what happened in the last combat frame?"

### 3.1 Presentation Contract

The specialization-facing UI must also share one presentation contract so the character sheet, skill tooltip, and specialization node tooltip read as one system.

- reading order always follows: identity -> core numbers -> behavior explanation -> warnings/limits;
- emphasis comes from position, spacing, and restrained color semantics rather than extra ornament;
- most rows use neutral or subdued text treatment;
- warm emphasis is reserved for build-output rows such as estimated damage;
- cool system colors are reserved for cost, cooldown, duration, and range-related rows;
- warning red is reserved for exclusion, penalty, and conflict messaging.

The purpose of the new data is not to make the tooltip busier. The purpose is to make the build read faster. If new duration / damage / quantitative lines do not improve first-scan comparison, they should not be added in a visually equivalent way to existing prose.

## 4. Recommended Architecture

### 4.1 Unified Static Preview Layer

Introduce a dedicated display-preview model for specialization-facing skill tooltips.

- `UIRenderer::DrawSkillTooltip` should consume preview data rather than calculating duration and estimated damage inline.
- Preview generation should live behind a small, explicit interface that accepts only static build inputs.
- The same preview rules should be usable anywhere else that needs specialization-friendly skill summaries later.

This is preferred over continuing to filter the full runtime damage pipeline in UI code, because the runtime pipeline is oriented around combat truth, not stable player-facing explanation.

### 4.2 Preview Data Structures

Add a dedicated preview payload, for example `SkillDisplayPreview`, with fields along these lines:

- `has_duration`
- `display_duration_seconds`
- `has_estimated_damage`
- `estimated_damage_value`
- `estimated_damage_mode`
- optional source/debug metadata for tracing which display sources contributed

`estimated_damage_mode` should express the semantic meaning of the single value, for example:

- `Hit`
- `PerSecond`
- `Total`
- `ChannelWindow`

This avoids bringing back min/max fields now that base-damage ranges are no longer part of the product model.

### 4.3 Preview Input Context

Add a narrow input object, for example `SkillDisplayContext`, that explicitly limits what preview code can see.

Allowed inputs:

- static player combat-panel values;
- skill definition data;
- currently allocated specialization-node state;
- passive-tree bonuses.

Disallowed inputs:

- live buff containers;
- target entities;
- runtime combat logs;
- transient state caches.

Restricting the interface is the main guardrail that prevents accidental drift back toward runtime-coupled previews.

## 5. Duration Design

### 5.1 Global Duration Source

- Keep `CombatStats::duration_scale` as the single low-level source of truth for global duration scaling.
- Add an explicit character-panel field such as `技能持续时间 +X%` so the stat is visible, testable, and user-readable.
- In the character panel, place the row in the existing skill-scaling cluster: `冷却缩减 -> 技能持续时间 -> 技能范围`.
- Render it as a normal stat row, not a highlighted callout. It is a build-planning stat, not a primary alert.
- Skill tooltip preview should read the global duration contribution through the same surfaced stat path, not by inventing its own separate interpretation.

This also aligns with the open tracking item `BUG-20260313-001`, which exists specifically to verify the `CombatStats -> character panel -> tooltip preview` association chain.

### 5.2 Duration Formula

For a duration-bearing skill:

- display base duration = skill-configured display baseline duration
- displayed duration = `base * global_duration_scale * specialization_duration_scale * passive_tree_duration_scale`

Only currently active, always-on allocations count. Dynamic runtime state does not.

### 5.3 Baseline Duration Data

Each duration-bearing skill must expose a display baseline duration through data.

- Prefer reusing `SkillData::params` with standardized keys such as summon lifetime, field duration, channel window, or equivalent existing parameters.
- If a skill currently hides duration only inside behavior code, add a standardized display-oriented param key rather than teaching UI code to inspect behavior internals.

If a skill does not have a reliable display baseline yet, the tooltip should omit the duration row until the data gap is filled.

## 6. Estimated Damage Design

### 6.1 Output Rule

Estimated damage should display a single value, not a range.

- direct-hit skills: one hit / one cast value
- damage-over-time skills: a clearly labeled `PerSecond` or `Total` value
- channel skills: a clearly labeled standard channel window value such as a 1-second window or a skill-defined baseline window

The skill tooltip stat block should keep a fixed order:

- `法力消耗`
- `冷却时间`
- `持续时间`
- damage preview row last

The player-facing damage labels should also be fixed rather than chosen ad hoc during implementation:

- `Hit` -> `预估伤害`
- `PerSecond` -> `每秒伤害`
- `Total` -> `总伤害`
- `ChannelWindow` -> `引导伤害(1秒)` unless a later global wording pass replaces this with one different canonical channel-window label everywhere

The UI should make the meaning visible through the row label or mode-specific suffix, not by displaying a synthetic min/max interval.

### 6.2 Damage Inputs

Estimated damage may use:

- static player attack/stat panel values;
- skill base damage and scalar data;
- active specialization-node modifiers;
- passive-tree modifiers.

Estimated damage may not use:

- live enemy resistance/vulnerability state;
- crit randomness or proc variance;
- dynamic stacking windows;
- temporary buffs;
- scene-dependent target count or chain behavior.

### 6.3 Interpretation Rule

Tooltip estimated damage is a build-comparison number. It is not a promise of real encounter DPS, and it should remain stable enough that node tooltip lines such as `预估伤害 +18%` stay interpretable.

If a skill cannot yet provide a stable interpretation for a single preview number, the tooltip should skip the estimated-damage row rather than display a misleading value.

## 7. Specialization Node Quantitative Tooltips

### 7.1 Presentation Format

Node tooltips should become a two-part presentation.

- first: the existing qualitative description explaining behavior and fantasy
- second: one or more short quantitative lines summarizing concrete payoff

In layout terms, the tooltip should read as five stable regions:

- title
- badges
- qualitative description
- quantitative payoff block
- warnings / exclusion / cost notes

This preserves expressive design language while making point investment decisions faster to evaluate.

### 7.2 Auto-Generated Quantitative Lines

When `TalentNode` data is already structured enough, quantitative lines should be generated automatically from:

- `stat_modifiers`
- `damage_modifiers`

Examples:

- `持续时间 +20%`
- `范围 +15%`
- `护甲 +80`
- `预估伤害 +12%`

Auto-generation should only occur when the mapping from structured modifier to player-facing line is stable and unambiguous.

The default quantitative block should show the 2-3 most decision-relevant lines first. Recommended stable ordering:

- damage / output
- duration
- frequency / count
- area / range
- resource / cooldown
- other secondary lines

### 7.3 Display Metadata for Behavior Nodes

Some nodes describe behavior changes that cannot be translated safely from generic modifiers alone. For those cases, add optional display metadata to node data, for example `display_lines`.

This metadata exists only to drive tooltip clarity for effects such as:

- stability changes;
- cadence or pulse-frequency changes;
- alternate behavior windows;
- special conversion wording that does not map cleanly to generic stat keys.

Rule: preview code should not guess at behavior-driven quantitative output. If a value cannot be inferred safely, data must declare it explicitly.

### 7.4 Ordering and Fallbacks

- keep qualitative description first;
- render quantitative lines after it as a distinct block with its own spacing break;
- preserve both when a node has both behavior and numeric payoff;
- render warnings and penalties after the quantitative block, not mixed into it;
- deduplicate equivalent output when `display_lines` and auto-generated modifier lines would describe the same payoff;
- if the UI shows uninvested-node preview values, treat them consistently as next-point preview values everywhere rather than as if they were already active total values;
- if the UI shows invested-node values, prefer the current invested total rather than fragmented per-point prose;
- if a node cannot yet provide safe quantitative output, keep the qualitative-only tooltip instead of fabricating numbers.

## 8. Data and File Impact

Expected first-pass touchpoints:

- `src/game/data/SkillRegistry.hpp`
  - standardized access to display-baseline duration parameters
- `src/game/components/SkillDefs.hpp`
  - optional node display metadata structure for quantitative lines
- `src/engine/render/UIRenderer.cpp`
  - consume `SkillDisplayPreview`
  - add tooltip duration row
  - switch estimated damage from synthetic range to single-value display
- `src/game/systems/ui/UISkillTalentTree.cpp`
  - append quantitative lines beneath qualitative node descriptions
- character-panel UI source files
  - expose the global duration stat visibly as `技能持续时间 +X%`
- `assets/data/mastery_skill_trees.json`
  - first-pass quantitative display data for the current specialization flow

## 9. Delivery Strategy

Recommended minimum implementation sequence:

1. surface the global duration field in the character panel and verify the stat chain;
2. introduce the static skill display-preview model and use it in specialization skill tooltips;
3. add baseline duration data for the affected duration-bearing skills;
4. enable specialization-node quantitative rendering from structured modifiers;
5. add explicit display metadata for behavior-heavy nodes that cannot auto-generate cleanly;
6. limit first-pass content/data coverage to the current specialization flow while keeping the protocol reusable.

This sequence front-loads the stat wiring and preview contract before adding wide data coverage.

## 10. Testing and Validation Strategy

### 10.1 UI / Tech Validation

Add or extend UI-oriented coverage for:

- skill tooltips showing or hiding the duration row correctly;
- skill tooltips showing single-value estimated damage with the correct semantic mode;
- skill tooltip stat rows keeping the fixed build-facing order;
- specialization node tooltips rendering qualitative and quantitative sections in stable order;
- character-panel duration field visibility and formatting.

`tests/tech/UITests.cpp` is the natural place for the presentation plumbing checks when source-level UI tests are appropriate.

Add a lightweight manual UI checklist for at least:

- one duration-bearing skill tooltip;
- one long-description specialization node;
- one behavior-heavy node using explicit `display_lines`;
- one mutually exclusive node showing warning text;
- one uninvested, one invested, and one maxed node state.

### 10.2 Data Validation

Add lightweight validation for:

- duration-bearing skills missing a display baseline duration;
- behavior-heavy nodes that neither auto-generate quantitative lines nor provide explicit display metadata.

The goal is not to block every legacy data gap immediately, but to make omissions visible and trackable.

### 10.3 Verification Principle

Verification should prove that display-preview values are sourced from the static build model and remain insulated from runtime-only state. The work succeeds when a build can be inspected consistently outside combat and the tooltip values still match the build-facing sources.

## 11. Acceptance Criteria

This design is satisfied when:

- the character panel visibly exposes the global skill-duration stat;
- the character panel places `技能持续时间` beside other skill-scaling rows rather than burying it among unrelated utility stats;
- specialization skill tooltips can show a duration row for duration-bearing skills using the static preview rule;
- specialization skill tooltip estimated damage is shown as a single build-preview value with an explicit semantic mode when needed;
- specialization skill tooltip stat rows stay in a stable order and never fall back to a synthetic min/max damage range;
- runtime-only state no longer influences specialization tooltip preview values;
- specialization node tooltips can show quantitative short lines alongside their qualitative description;
- specialization node tooltips keep a stable section order: description -> quantitative payoff -> warnings;
- quantitative output never duplicates the same payoff twice from structured modifiers and explicit display metadata;
- behavior-heavy nodes that cannot be auto-derived have an explicit display-metadata path instead of remaining permanently qualitative.

## 12. Deferred Follow-up

- broader profession-by-profession rollout after the first specialization slice lands;
- richer display-source introspection/debug overlays if preview tuning later needs it;
- deeper product wording pass for standardized Chinese labels such as `每秒伤害`, `总伤害`, and `完整引导` if UX review wants stricter consistency.
