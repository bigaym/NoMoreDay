# Sword Cultivator Masteries Closure Design

> Date: 2026-03-11
> Status: approved
> Scope: `Sword Saint`, `Heavenly Sword`, `Demon Blade`, and the shared mastery productization work needed to make them feel complete.

---

## 1. Background

Recent roadmap review, code inspection, and tests show that the Blade Ascendant mastery stack is functional but uneven.

- `Sword Saint` is the most complete specialization. Its full `1000-1025` node range is represented in `src/game/systems/skill/behaviors/SevenStarSlash.cpp`, and the runtime already supports its core loop, transmuters, follow-up windows, and payoff structure.
- `Heavenly Sword` has a working field/tier/echo baseline, but several nodes described in `assets/data/mastery_skill_trees.json` do not appear to have runtime behavior yet.
- `Demon Blade` currently behaves like a solid `BloodSea` MVP rather than a finished mastery tree. Many node descriptions exist only in data and are not represented in runtime logic.
- Shared mastery systems exist (`BladeMasteryService`, `BladeResourceService`, `PlayerHUD`, save data, skill gating), but they are still thin and do not communicate all mastery windows, forms, and lifecycle transitions clearly.

The project should therefore finish the class-depth closure first, not broaden content. The immediate engineering goal is to bring all three Sword Cultivator masteries to a consistent, testable, player-readable quality bar.

## 2. Goals

### 2.1 Goals

- Finish missing runtime functionality for `Demon Blade` and `Heavenly Sword` so data promises match real gameplay behavior.
- Preserve `Sword Saint` as the quality reference and use it to define the target bar for the other two masteries.
- Close shared mastery productization gaps that block reliable play: lifecycle cleanup, save/load consistency, gating, HUD clarity, and test coverage.
- Keep the implementation incremental and TDD-friendly so each mastery can be completed in small slices without destabilizing the class.

### 2.2 Non-goals

- Do not redesign the Blade Ascendant class from scratch.
- Do not add a fourth mastery or new class-level progression in this slice.
- Do not treat this as a full combat-presentation pass; only add HUD/readability work that is required to make the urgent mastery slices understandable and testable.
- Do not expand `Sword Saint` with new gameplay features beyond blocking fixes or parity-driven polish.

## 3. Current-State Findings

### 3.1 Sword Saint

- `Sword Saint` is feature-complete at the behavior-code level and should be treated as a polish track, not a missing-systems track.
- Remaining work is mainly player communication: window surfacing, transmuter readability, lingering scar payoff clarity, and balance validation.

### 3.2 Heavenly Sword

- Runtime support exists for the core field loop, tier spending, linked-hit echoes, and attunement-dependent debuffs.
- The following data-described nodes appear to still need runtime closure: `1100`, `1104`, `1105`, `1106`, `1108`, `1112`, `1114`, `1118`.
- The biggest systemic gap is not raw persistence; attunement already exists in mastery state and save data. The gap is an explicit selection path plus clearly-defined lifecycle semantics for when attunement should persist, reset, or be surfaced.

### 3.3 Demon Blade

- Runtime support exists for the `BloodSea` field, opening burst, overflow-heal conversion, linked pulses, void erosion, two transmuters, and resist shred.
- The following data-described nodes appear to still need runtime closure: `1200-1206`, `1208-1210`, `1212`, `1214-1216`, `1218`, `1219`, `1223`.
- Demon Blade has the largest delta between design intent and actual runtime depth and is therefore the highest-priority completion track.

### 3.4 Shared Systems

- `BladeMasteryService` and `BladeResourceService` already support profession gating, signature unlocks, and resource identity switching.
- `PlayerHUD` exposes only basic mastery state. It is useful for debugging, but not yet strong enough to communicate mastery windows and forms reliably.
- Save data stores selected mastery/resource/signature state, but the lifecycle of temporary field entities and mastery-specific temporary state still needs explicit verification.
- Existing tests skew toward `Sword Saint`; `Heavenly Sword` and `Demon Blade` need deeper per-node and lifecycle coverage.

## 4. Workstreams

### 4.1 Workstream A: Demon Blade Completion

Purpose: turn `Demon Blade` from a `BloodSea` MVP into a complete mastery tree whose high-risk sustain fantasy is fully represented in runtime code.

Focus areas:

- low-life pressure and scaling nodes
- sustain / healing-cap / overflow-conversion behavior
- chase-target, proximity-pressure, and linked debuff-extension nodes
- cadence / duration / DoT variant nodes
- transmuter payoff parity and test closure

### 4.2 Workstream B: Heavenly Sword Completion

Purpose: complete the field-control mastery fantasy by closing the missing nodes and making attunement a stable, testable gameplay axis.

Focus areas:

- impact-center and field-core payoff nodes
- return-cycle and tier-economy nodes
- edge-control / refresh / duration-extension nodes
- attunement selection, persistence, and HUD expression
- per-attunement gameplay differences with targeted tests

### 4.3 Workstream C: Sword Saint Polish

Purpose: keep `Sword Saint` as the mastery quality bar and patch only the issues that block parity or player readability.

Focus areas:

- HUD surfacing for high-value temporary windows
- transmuter payoff readability
- lingering scar visibility and payoff readability
- balance and hand-test follow-up, not net-new branch mechanics

This workstream is not urgent-plan material for this round. It stays in the design as follow-up work.

### 4.4 Workstream D: Shared Mastery Productization

Purpose: provide the minimum shared support needed for the two urgent mastery completion tracks to feel stable in real play.

Blocking shared work:

- mastery-switch cleanup expectations for active field entities / temporary state
- save/load and state-restoration verification for newly-added mastery state
- HUD expression for urgently-needed runtime states
- guard/integration/tech tests for switching, persistence, and signature gating

Shared implementation rule:

- Define the cleanup contract once in shared mastery code, then reuse it from both urgent implementation plans instead of re-solving lifecycle behavior twice.

Follow-up shared work:

- richer mastery window surfacing across all three masteries
- deeper respec UX and mastery communication polish
- broader hand-test matrix and productized readability pass

## 5. Priority Order

1. `Demon Blade completion`
2. `Heavenly Sword completion`
3. blocking shared mastery productization needed by the above
4. `Sword Saint` parity polish
5. broader shared mastery polish

Rationale:

- `Demon Blade` has the most missing gameplay promises and the weakest test depth.
- `Heavenly Sword` is closer, but still cannot be called complete while several nodes and attunement productization remain unfinished.
- `Sword Saint` already has the broadest runtime and test coverage; it should not grow scope until the other two masteries catch up.

## 6. Architecture Guidelines

### 6.1 Runtime Behavior Placement

- Keep node behavior as close as possible to the owning signature behavior files:
  - `src/game/systems/skill/behaviors/BloodSea.cpp`
  - `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- Only move logic into shared services when it is truly resource- or mastery-wide rather than field-specific.

### 6.2 Shared State Placement

- Extend existing mastery/resource components only when the state must survive across frames, save/load, or cross-skill consumers.
- Keep temporary, field-local state inside the field component or owning behavior where possible.
- Avoid introducing a new parallel mastery state machine.

### 6.3 UI/HUD Placement

- Use `src/game/systems/ui/PlayerHUD.cpp` for minimal high-value clarity improvements required by the urgent work.
- Do not fold a full presentation overhaul into these completion slices.

### 6.4 Testing Strategy

- Prefer narrow doctest slices first.
- Add or extend:
  - `tests/unit/SkillBehaviorGuardTests.cpp`
  - `tests/unit/BladeMasteryTests.cpp`
  - `tests/integration/SkillSystemTests.cpp`
  - `tests/tech/UITests.cpp` only where HUD or UI plumbing changes are required
- Preserve the existing CI-oriented verification flow after focused tests pass.

## 7. Acceptance Criteria

The Sword Cultivator mastery closure effort is successful when:

- every urgent node described in `assets/data/mastery_skill_trees.json` for `Demon Blade` and `Heavenly Sword` has matching runtime behavior or is explicitly re-scoped in data;
- mastery-specific temporary state survives the expected lifecycle transitions and is cleaned up correctly on invalid transitions;
- the urgent mastery mechanics can be verified through unit and integration tests rather than hand-wavy manual claims;
- `PlayerHUD` exposes enough state for the new urgent mastery behaviors to be understood during verification;
- `Sword Saint` remains stable and is not regressed by shared-system changes.

## 8. Deliverables for This Planning Round

This design covers all Sword Cultivator mastery closure work, but only the two urgent execution plans will be written now:

- `Demon Blade completion`
- `Heavenly Sword completion`

`Sword Saint polish` and the non-blocking shared mastery work remain follow-up items after the urgent completion plans are executed.
