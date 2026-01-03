# Specification: Sword Cultivator Foundation Expansion

## 1. Overview
This track focuses on expanding the core technical foundations of the skill and movement systems to support the advanced mechanics of the **Sword Cultivator (Blade Ascendant)** class. Instead of implementing specific skills, we are building the architectural "hooks" and systems that will allow those skills to be implemented with high flexibility and performance.

## 2. Functional Requirements

### 2.1 Skill Modification System (A)
- **Component-Driven Modifiers:** Implement a system where specialization nodes (from the UI) can attach components to an entity's `SkillExecution`.
- **Pipeline Injection:** The `DamagePipeline` must be updated to read these skill-specific modifiers from the `DamageEvent` or `Projectile` entity to apply logic like damage conversion, conditional multipliers (e.g., distance-based), and "on-hit/on-kill" triggers.

### 2.2 Lightweight Shadow System (B)
- **Shadow Entities:** Create a `ShadowComponent` for lightweight entities that act as "ghosts".
- **Snapshot Logic:** When a shadow is spawned, it should "snapshot" specific skill data (ID, Position, Direction, relevant stats).
- **Specialized ShadowSystem:** A dedicated system will process these shadows, executing their visual effects and damage logic once before they are destroyed (or according to a brief lifetime).

### 2.3 Skill State Logic Hooks (C)
- **Hook Architecture:** Add `PreCast` and `PostCast` logic hooks to the `SkillSystem`.
- **Empowered Casts:** Implement a hook that checks for 10 stacks of *Sword Intent*. If present, it sets an `isEmpowered` flag on the `SkillExecution` and consumes the stacks.
- **Buff/Mechanic Triggering:** These hooks will handle complex interactions like triggering secondary skills, spawning shadows, or applying temporary buffs without bloating the core state machine.

### 2.4 Stance & Enhanced Movement (D)
- **Movement Components:** Implement a `MovementStanceComponent` to handle states like "Sword Riding".
- **Interaction Logic:** Add logic to handle stance transitions (e.g., 2s continuous movement to enter) and cancellation (e.g., taking damage or manual cancellation).

## 3. Non-Functional Requirements
- **Performance:** Shadows must be extremely lightweight to support dozens of them appearing in high-intensity combat without affecting FPS.
- **Extensibility:** The hook system should allow other classes (not just Sword Cultivators) to easily add their own pre/post-cast logic.
- **Maintainability:** Avoid bloating `SkillSystem.cpp` by using a decoupled, component-based approach for modifiers.

## 4. Acceptance Criteria
- [ ] A "Shadow" entity can successfully repeat a simplified version of a skill (e.g., just the damage and visual effect) based on a snapshot.
- [ ] Specialization modifiers attached to a player's skill successfully alter the output of the `DamagePipeline` (verified via unit tests).
- [ ] Reaching 10 stacks of *Sword Intent* correctly triggers an "Empowered" state through a `PreCast` hook.
- [ ] Continuous movement for 2s successfully enters a movement stance that is canceled upon taking damage.

## 5. Out of Scope
- Implementation of the full specialization trees for all skills (only the underlying system is in scope).
- Final visual assets for Sword Riding or Shadows (placeholders will be used).
- Balancing of specific numerical values for the new mechanics.
