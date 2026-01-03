# Specification: Skill Management & Specialization System (Hotkey: S)

## 1. Overview
Implement a comprehensive Skill Management Interface (Hotkey: 'S') inspired by *Last Epoch*. This system allows players to view available skills, assign them to the action hotbar, and specialize up to 5 skills. Each specialized skill will have its own unique talent tree for further customization.

## 2. Functional Requirements
### 2.1 UI Management
- **Hotkey Binding**: Pressing 'S' toggles the Skill Management Interface.
- **Central Hub Layout**: A main view featuring 5 large slots for specialized skills, surrounded by the pool of all learned/available skills.
- **Skill Tooltips**: Hovering over any skill icon displays its name, base stats, description, and tags (e.g., Melee, Cold, Area).
- **Sub-View (Talent Trees)**: Clicking a specialized skill in the Central Hub transitions to a dedicated Talent Tree view for that specific skill.

### 2.2 Specialization & Talents
- **Specialization Slots**: Maximum of 5 skills can be specialized simultaneously.
- **Talent Trees**: Each skill has a unique tree of nodes that modify its behavior (e.g., increased damage, converted element, added projectiles).
- **Skill Points**: 
    - Players start with 1 point at Level 1.
    - Gain +1 point per character level.
    - System must support "Bonus Points" awarded from other sources (e.g., quests/items).

### 2.3 Skill Assignment
- **Action Bar Mapping**: Players can map learned skills to the 5 active slots: Q, W, E, R, and RMB.
- **Hotbar Sync**: Changes in the skill menu are immediately reflected in the gameplay hotbar UI.

## 3. Non-Functional Requirements
- **Performance**: The UI transition between the Hub and Talent Trees should be smooth without FPS drops.
- **UI Scaling**: The interface must adhere to the project's `scaleFactor` to look correct on different resolutions.
- **Persistence**: Skill specialization and talent allocations must be saved and persist across game sessions/level transitions.

## 4. Acceptance Criteria
- Pressing 'S' correctly opens and closes the skill menu.
- 5 specialization slots are functional; players can assign and unassign skills to them.
- Talent trees are accessible for specialized skills, and points can be spent.
- Spent talent points correctly modify the skill's combat behavior (e.g., damage calculation or visual effects).
- Skill assignment to Q/W/E/R/RMB works as expected and updates the HUD.

## 5. Out of Scope
- Skill "Respec" (Refund) cost logic (to be added in a later track).
- Complex skill animations for the talent preview.
