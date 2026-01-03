# Plan: Skill Management & Specialization System Implementation

## Phase 1: Infrastructure & Data Models [checkpoint: cbb43f7]
- [x] Task: Define `SkillTree` and `TalentNode` data structures in `SkillSystem.hpp`. f3ee4e0
- [x] Task: Update `ActiveSkillsComponent` to include specialization state and talent point allocation. f3ee4e0
- [x] Task: Implement `SkillRegistry` extension to load talent tree definitions from `assets/data/skills.json`. ec810f1
- [ ] Task: Conductor - User Manual Verification ' Phase 1: Infrastructure & Data Models' (Protocol in workflow.md)

## Phase 2: Core Logic & Progression [checkpoint: 3fc1f78]
- [x] Task: Implement `SkillSystem::AddTalentPoint` logic with validation (available points, prerequisites). 366207e
- [x] Task: Update `ProgressionSystem` to award Skill Specialization points on character level-up (starting with 1 at Level 1). 366207e
- [x] Task: Create `SkillEffectSystem` hooks to apply talent modifications to actual combat skills (e.g., damage multipliers, cooldown reduction). 366207e
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Core Logic & Progression' (Protocol in workflow.md)

## Phase 3: UI - Central Hub (Hotkey: S) [checkpoint: 575fe28]
- [x] Task: Register 'S' key in `InputSystem` to toggle the Skill Management Interface. ed0cc50
- [x] Task: Implement `UISkillHub` component in `UISystem.cpp`: e479aed
    - Display 5 specialization slots.
    - Display a grid of all available skills.
    - Implement drag-and-drop or click logic to assign/unassign specialized skills.
- [x] Task: Add skill tooltips with tags and base stats. 57b7739
- [ ] Task: Conductor - User Manual Verification 'Phase 3: UI - Central Hub' (Protocol in workflow.md)

## Phase 4: UI - Talent Trees [checkpoint: d6ca591]
- [x] Task: Implement `UISkillTalentTree` view: a8a5325
    - Render a node-link diagram for the selected specialized skill.
    - Show node status (Locked, Available, Maxed).
    - Handle point spending and visual feedback.
- [x] Task: Implement navigation between the Central Hub and specific Talent Trees. a8a5325
- [ ] Task: Conductor - User Manual Verification 'Phase 4: UI - Talent Trees' (Protocol in workflow.md)

## Phase 5: Integration & Polish
- [x] Task: Integrate talent modifications into `CombatSystem` (e.g., skill behavior changes).
- [x] Task: Ensure skill specialization and talent data are correctly serialized/deserialized in `PersistenceTest`.
- [x] Task: Visual polish: UI animations for spending points and switching views.
- [x] Task: Conductor - User Manual Verification 'Phase 5: Integration & Polish' (Protocol in workflow.md)
