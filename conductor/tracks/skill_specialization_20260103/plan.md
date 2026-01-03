# Plan: Skill Management & Specialization System Implementation

## Phase 1: Infrastructure & Data Models
- [x] Task: Define `SkillTree` and `TalentNode` data structures in `SkillSystem.hpp`. f3ee4e0
- [x] Task: Update `ActiveSkillsComponent` to include specialization state and talent point allocation. f3ee4e0
- [ ] Task: Implement `SkillRegistry` extension to load talent tree definitions from `assets/data/skills.json`.
- [ ] Task: Conductor - User Manual Verification ' Phase 1: Infrastructure & Data Models' (Protocol in workflow.md)

## Phase 2: Core Logic & Progression
- [ ] Task: Implement `SkillSystem::AddTalentPoint` logic with validation (available points, prerequisites).
- [ ] Task: Update `ProgressionSystem` to award Skill Specialization points on character level-up (starting with 1 at Level 1).
- [ ] Task: Create `SkillEffectSystem` hooks to apply talent modifications to actual combat skills (e.g., damage multipliers, cooldown reduction).
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Core Logic & Progression' (Protocol in workflow.md)

## Phase 3: UI - Central Hub (Hotkey: S)
- [ ] Task: Register 'S' key in `InputSystem` to toggle the Skill Management Interface.
- [ ] Task: Implement `UISkillHub` component in `UISystem.cpp`:
    - Display 5 specialization slots.
    - Display a grid of all available skills.
    - Implement drag-and-drop or click logic to assign/unassign specialized skills.
- [ ] Task: Add skill tooltips with tags and base stats.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: UI - Central Hub' (Protocol in workflow.md)

## Phase 4: UI - Talent Trees
- [ ] Task: Implement `UISkillTalentTree` view:
    - Render a node-link diagram for the selected specialized skill.
    - Show node status (Locked, Available, Maxed).
    - Handle point spending and visual feedback.
- [ ] Task: Implement navigation between the Central Hub and specific Talent Trees.
- [ ] Task: Conductor - User Manual Verification 'Phase 4: UI - Talent Trees' (Protocol in workflow.md)

## Phase 5: Integration & Polish
- [ ] Task: Integrate talent modifications into `CombatSystem` (e.g., skill behavior changes).
- [ ] Task: Ensure skill specialization and talent data are correctly serialized/deserialized in `PersistenceTest`.
- [ ] Task: Visual polish: UI animations for spending points and switching views.
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Integration & Polish' (Protocol in workflow.md)
