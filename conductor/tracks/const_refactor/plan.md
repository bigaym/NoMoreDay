# Refactoring Track: Constant Centralization & Magic Number Elimination

**Track ID:** `const_refactor`
**Status:** Planned
**Owner:** System Architect
**Created:** 2026-01-18

## 1. Context & Motivation
A recent code risk analysis identified ~2,700 magic numbers in the codebase. These pose significant risks:
1.  **Logical Desync:** Critical caps (e.g., CDR at 0.75f) are defined in `Common.hpp` but hardcoded in `GameplayState.cpp`.
2.  **Physics Instability:** Thresholds for "stopped" entities vary between systems (`0.001f` vs `0.01f`), causing potential jitter.
3.  **Architectural Violation:** Visual constants (UI Colors) are tightly coupled with C++ logic instead of residing in `GPUData.hpp` or purely data-driven config.

## 2. Objectives
*   **Single Source of Truth:** Ensure all gameplay constants derive from `src/game/components/Common.hpp`.
*   **Decouple Rendering:** Ensure all hardcoded colors and visual offsets derive from `src/engine/render/GPUData.hpp`.
*   **Standardize Physics:** Unify all epsilon/threshold checks.

## 3. Implementation Plan

### Phase 1: Critical Logic & Physics Fixes (High Priority)
*Target: Prevent gameplay bugs and logic divergence.*

- [ ] **Task 1.1: Fix CDR & Stat Caps**
    -   **File:** `src/states/GameplayState.cpp`
    -   **Action:** Replace `if (cdr > 0.75f)` with `if (cdr > Cap::CDR)`.
    -   **Ref:** `Common.hpp` -> `namespace Cap`.
- [ ] **Task 1.2: Standardize Physics Epsilon**
    -   **File:** `src/game/components/Common.hpp`
    -   **Action:** Define `constexpr float EPSILON_VELOCITY = 0.001f;` in `Physics` namespace.
    -   **File:** `src/states/GameplayState.cpp` & `src/systems/ai/AISystem.cpp`
    -   **Action:** Replace literals (`0.001f`, `0.01f`) with `Physics::EPSILON_VELOCITY`.
- [ ] **Task 1.3: Combat Formulas**
    -   **File:** `src/systems/combat/CombatSystem.cpp`
    -   **Action:** Replace damage multipliers (e.g., `1.5f` crit damage fallback) with constants from `Combat` namespace.

### Phase 2: Visual Decoupling (Medium Priority)
*Target: Clean up UI code and prepare for future theming.*

- [ ] **Task 2.1: Define Color Palette**
    -   **File:** `src/engine/render/GPUData.hpp`
    -   **Action:** Expand `namespace Colors` to include:
        -   `UI_BACKGROUND`, `UI_PANEL_BORDER`
        -   `TEXT_PRIMARY`, `TEXT_SECONDARY`, `TEXT_HIGHLIGHT`
        -   `BUTTON_NORMAL`, `BUTTON_HOVER`, `BUTTON_PRESS`
- [ ] **Task 2.2: Migrate UI Systems**
    -   **Files:** `src/systems/ui/UIInventory.cpp`, `src/systems/ui/UICharacter.cpp`
    -   **Action:** Replace `Color{r,g,b,a}` literals with `Colors::*` constants.

### Phase 3: Particle System Limits (Low Priority)
*Target: Optimization and Scalability.*

- [ ] **Task 3.1: Configurable Particle Counts**
    -   **File:** `src/game/components/Common.hpp`
    -   **Action:** Define `Visuals::PARTICLE_COUNT_LOW`, `MEDIUM`, `HIGH`.
    -   **File:** `src/systems/combat/EffectSystem.cpp`
    -   **Action:** Replace loop limits (e.g., `i < 15`) with named constants.

## 4. Verification Strategy
1.  **Compile Check:** Ensure no compilation errors after header modifications.
2.  **Logic Verify:** Verify character stats panel reflects the correct caps defined in `Common.hpp`.
3.  **Visual Verify:** Ensure UI colors remain consistent (no accidental color shifts).
