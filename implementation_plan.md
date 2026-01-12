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