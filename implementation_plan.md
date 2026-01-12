# Implementation Plan - Skill System Phase 1: Shadow Core

## Goal Description
Establish the **Shadow Entity** system to support the "Shadow Kill" branch of *Flowing Thrust* (Skill ID 1) and future skills like *Phantom Flash*. A "Shadow" is a short-lived summoned entity that mimics specific player skills or performs follow-up attacks.

## User Review Required
> [!NOTE]
> **Architecture Decision**: We will reuse `SummonComponent` for generic lifetime management (despawn on timer) but implement a dedicated `ShadowSystem` for the specific mimicry logic. This ensures separation of concerns: `SummonSystem` handles existence, `ShadowSystem` handles action.

## Proposed Changes

### 1. New Components
#### [EXISTING] [ShadowComponent](file:///f:/NoMoreDay/src/game/components/SkillDefs.hpp)
Reused existing `ShadowComponent` in `SkillDefs.hpp` which contains `SkillSnapshot` and `delay`.

### 2. New System Logic
#### [NEW] [ShadowSystem](file:///f:/NoMoreDay/src/game/systems/skill/ShadowSystem.hpp)
The `ShadowSystem` manages the lifecycle of shadow actions (delay -> cast).

### 3. Integration Points
#### [MODIFY] [FlowingThrust.cpp](file:///f:/NoMoreDay/src/game/systems/skill/behaviors/FlowingThrust.cpp)
- **Implemented**: Logic to spawn shadow entity with `ShadowComponent` and `SummonComponent`.
- **Logic**:
    1. Check for "Leave Shadow" talent.
    2. Create entity with `ShadowComponent` (delay 0.5s) and `SummonComponent` (lifetime 1.5s).

#### [MODIFY] [GameplayState.cpp](file:///f:/NoMoreDay/src/game/states/GameplayState.cpp)
- Registered `ShadowSystem::Update` in the main update loop.

## Verification Plan

### Automated Tests
- **`tests/TestShadowSystem.cpp`**:
    - `TestShadowSpawn`: Verify `ShadowComponent` is initialized correctly.
    - `TestShadowExecution`: Verified `SkillExecution` creation after delay.
    - `TestShadowCleanup`: Verified entity destruction.

### Manual Verification
- Tests passed via `tests_runner.exe -tc="ShadowSystem Tests"`.

# Implementation Plan - Skill System Phase 2: Blade Formation Deep Dive

## Goal Description
Refactor **Blade Formation (Ling Jian Jue)** to support advanced behaviors: "Giant Sword" (Heavy) and "Standard/Sword Rain" modes.

## Changes

### 1. Components
#### [MODIFY] [SkillDefs.hpp](file:///f:/NoMoreDay/src/game/components/SkillDefs.hpp)
- Updated `SpiritSwordAI` to include `State` (Idle, Attacking, etc.) - *Not strictly used yet but ready*.
- `BladeFormationComponent` handles flags for `has_giant_sword`.

### 2. Systems
#### [MODIFY] [SummonSystem.cpp](file:///f:/NoMoreDay/src/game/systems/skill/SummonSystem.cpp)
- **Refactored `UpdateSpiritSwords`**:
    - Checks `BladeFormationComponent` from owner.
    - **Giant Sword**: Slower orbit, larger radius, 1.5x Damage, Heavy visual effect (Ink Splash).
    - **Standard**: Faster orbit, 0.5x Damage, Standard visual effect (Ink Trail).
- **Damage Logic**: Uses `CombatStats` snapshot on proxy entity to scale damage multipliers before casting `Rending Wave` (Skill ID 2).

#### [MODIFY] [BladeFormation.cpp](file:///f:/NoMoreDay/src/game/systems/skill/behaviors/BladeFormation.cpp)
- **Talent Logic**: Correctly sets `has_giant_sword` flag if Talent 310 is allocated.
- **Merging**: Caps `max_swords` to 1 if Giant Sword is active.

### 3. Tests
#### [NEW] [TestBladeFormation.cpp](file:///f:/NoMoreDay/tests/TestBladeFormation.cpp)
- **Deep Dive Tests**:
    - Verify "Giant Sword" caps max swords to 1.
    - Verify "Standard Mode" allows multiple swords.
    - **Damage Scaling**: Uses `SkillSystem::AddPreCastHook` to verify that the proxy entity cast has the correct damage multipliers (1.5x vs 0.5x).

## Verification
- **Test Results**: `Blade Formation Deep Dive` PASSED.
- **Build**: Fixed `-fno-rtti` issues in `tests/CMakeLists.txt` to allow `spdlog` compatibility.

# Implementation Plan - Skill System Phase 3: Defensive & Counter Mechanics

## Goal Description
Implement defensive mechanics for **Blade Ward** (Projectile Interception) and **Phantom Flash** (Evasion Counter).

## Changes

### 1. Blade Ward Interception
#### [MODIFY] [ProjectileSystem.cpp](file:///f:/NoMoreDay/src/game/systems/skill/ProjectileSystem.cpp)
- **Existing Logic**: `ProjectileSystem` already contains logic to check `BladeWardComponent` on target during collision.
- **Verification**: Logic verified via tests.

### 2. Phantom Flash Counter
#### [MODIFY] [DamagePipeline.cpp](file:///f:/NoMoreDay/src/game/systems/combat/DamagePipeline.cpp)
- **Implement Counter**: In `Calculate`, check if defender has `PhantomFlashComponent` active.
- **Effect**: If active (and not triggered), negate damage (`total_damage = 0`) and trigger a counter-attack (`SkillSystem::ShadowCast` ID 2 from defender to attacker).

### 3. Tests
#### [NEW] [TestDefenseMechanics.cpp](file:///f:/NoMoreDay/tests/TestDefenseMechanics.cpp)
- **Blade Ward**: Verify projectile destruction and sword count decrement. (Note: Required valid `Velocity` on defender for grid inclusion).
- **Phantom Flash**: Verify 0 damage and `triggered` state on defender.

## Verification
- **Test Results**: `Defense Mechanics` PASSED.

# Implementation Plan - Skill System Phase 4: Visual Polish

## Goal Description
Enhance visual feedback for skills using `VisualFXSystem`, event-driven VFX, and Sword Intent auras.

## Changes

### 1. Visual FX System
#### [NEW] [VisualFXSystem.hpp/cpp](file:///f:/NoMoreDay/src/game/systems/combat/VisualFXSystem.hpp)
- **Initialize**: Registers event handlers for `OnSkillHit` and `OnCrit` via `CombatEventDispatcher`.
- **Update**: Handles Sword Intent aura particles based on stack count.
- **Centralized Logic**: Moved hardcoded VFX from `ProjectileSystem` to this system.

### 2. Integration
#### [MODIFY] [GameplayState.cpp](file:///f:/NoMoreDay/src/game/states/GameplayState.cpp)
- Registered `VisualFXSystem::Initialize` in `OnEnter`.
- Registered `VisualFXSystem::Update` in `OnUpdate`.

#### [MODIFY] [ProjectileSystem.cpp](file:///f:/NoMoreDay/src/game/systems/skill/ProjectileSystem.cpp)
- Removed hardcoded hit effects to avoid duplication.

## Verification
- **Visuals**: Verified compilation and logic. 
- **Tests**: Re-ran `Defense Mechanics` to ensure no regression. PASSED.
