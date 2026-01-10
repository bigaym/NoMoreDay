# Skill System & Talent System Comprehensive Static Verification Report

**Date:** 2026年1月10日星期六
**Project:** NoMoreDay
**Goal:** Comprehensive and in-depth static verification of the skill system and talent system, covering casting logic, damage calculation, cooldowns, resource consumption, talent node activation, system interactions, cancellation mechanisms, and compound effects.

---

## 1. Skill System Overview

The NoMoreDay skill system is built upon a component-based ECS architecture, leveraging `entt` for entity management. Key data is defined in `assets/data/skills.json` and processed by various C++ components and systems, primarily:
*   `src/components/SkillSystem.hpp`: Defines core skill-related components and structures.
*   `src/core/TagRegistry.hpp`: Defines `Tag` enum for conditional modifiers.
*   `src/components/Stats.hpp`: Defines `StatType`, `ModifierMode`, `StatModifier` and the central `CombatStats` structure.
*   `src/systems/StatsSystem.cpp`/`.hpp`: Responsible for aggregating and applying stat modifiers to `CombatStats`.
*   `src/systems/DamagePipeline.cpp`/`.hpp`: Handles the detailed damage calculation, including conversions, multipliers, and defensive mechanics.
*   `src/systems/SkillSystem.cpp`/`.hpp`: Manages skill casting, cooldowns, resource costs, and skill-specific effects.

---

## 2. Verification Details

### 2.1 Skill Functionality Verification

#### a. Expected Casting Logic
*   **Implementation:** `SkillSystem::TryCast` initiates the skill casting process. It checks for sufficient charges, ensures no other skill is currently casting, and consumes mana. It then sets up a `SkillExecution` component on the casting entity, transitioning through `Preparing`, `Casting`, and `Settle` states managed by `SkillSystem::UpdateStates`.
*   **Verification:** The logic effectively orchestrates the skill's lifecycle, from initiation to completion, allowing for pre/post-cast hooks and animation state updates.

#### b. Spell Release Accuracy
*   **Implementation:** Skills that involve projectiles (e.g., Flowing Thrust, Rending Wave, Blade Boomerang) create `Projectile` entities. These projectiles are given `Position` and `Velocity` components, often calculated using `Vector2Normalize` and `Vector2Subtract` from the caster's position to the target position, ensuring accurate directionality.
*   **Verification:** Directional skills correctly calculate and apply velocity to their projectiles or effects based on the target position. Area-of-effect skills (e.g., Sword Array) are positioned at the target location.

#### c. Damage Numerical Calculation
*   **Implementation:** Damage calculation is primarily handled by `DamagePipeline::Calculate`.
    *   It retrieves base skill damage (`skill_data->base_damage` and `weapon_damage_mult`) and combines it with additional flat damage.
    *   It applies `Conversion` and `GainExtra` damage modifiers from `GlobalModifierComponent` (astrolabe, global buffs) and `SkillModifierComponent` (from the source entity, e.g., a projectile).
    *   `Increased` damage modifiers are applied dynamically by `StatsSystem::GetStatWithTags`, ensuring conditional application based on `skill_id` and `hit_tags`.
    *   `More` damage modifiers from skill talents are explicitly applied within `DamagePipeline::Calculate` after `StatsSystem::GetStatWithTags` has been called, maintaining the correct damage calculation hierarchy.
    *   Critical hits are determined based on `CritChance` (which can be dynamically modified by talents like Weakness Insight, ID 130) and `CritDamage`.
    *   Defensive mechanics (Elemental Resistances, Physical Armor with `ArmorPenetration`, and Global Damage Reduction) are applied in sequence. The physical armor calculation is robust, handling both positive and negative effective armor values.
    *   Shadow clone damage is reduced by 50% (`shadow_multiplier = 0.5f`).
*   **Verification:** The damage calculation pipeline is comprehensive and correctly implements the "Flat > Increased > More > Defense" damage modification hierarchy. Conditional modifiers based on `Tag` and `skill_id` are correctly applied, ensuring talent effects are scoped as intended.

#### d. Cooling Time Mechanism
*   **Implementation:** `SkillSystem::UpdateCooldowns` decrements `SkillSlot::cooldown`. Upon reaching zero, `SkillSlot::current_charges` is incremented. The next cooldown duration is calculated using `skill_data->cooldown`, `CombatStats::cooldown_recovery_speed`, and `StatsSystem::GetStatWithTags` for `StatType::CooldownReduction`.
*   **Verification:** Cooldowns and charges are managed correctly, with modifiers like `CooldownReduction` and `CooldownRecoverySpeed` influencing the refresh rate. The formula `(base_cooldown / recovery_speed) * (1.0 - cooldown_reduction)` is correctly applied.

#### e. Resource Consumption Matching
*   **Implementation:** `SkillSystem::TryCast` deducts `skill_data->mana_cost` from `CombatStats::mana`. This cost is modified by `StatsSystem::GetStatWithTags` for `StatType::ResourceCostReduction`.
*   **Verification:** Mana consumption correctly applies resource cost reduction, matching the intended mechanics.

### 2.2 Talent System (Skill Tree) Functionality Verification

#### a. Activation Response of Each Skill Tree Node
*   **Implementation:** `SkillSystem::AddTalentPoint` is responsible for allocating points. It enforces `max_points` per node and checks `prerequisites` from `TalentNode::prerequisites` to ensure valid progression. Upon successful allocation, `ActiveSkillsComponent::available_talent_points` is decremented, `SpecializedSkill::allocated_points` is updated, and a `StatsDirty` component is added to the entity.
*   **Verification:** Talent point allocation follows prerequisite rules and maximum investment limits. The `StatsDirty` component ensures stat recalculation is triggered.

#### b. Effect Triggering
*   **Implementation:**
    *   **Stat Modifiers:** Talent `StatModifier`s (e.g., "+X% Physical Damage", "+Y Projectile Count") are dynamically retrieved and applied via `StatsSystem::GetStatWithTags` during stat queries (e.g., in `DamagePipeline`, skill-specific `CastCallback`s).
    *   **Damage Modifiers:** Talent `DamageModifier`s (`Convert`, `GainExtra`, `More`) are processed in `DamagePipeline`, either via `GlobalModifierComponent` (for Astrolabe/global effects) or by direct iteration of `SpecializedSkill::allocated_points` for skill-specific effects.
    *   **Behavioral Modifiers:** Specific skill `CastCallback` implementations (e.g., Flowing Thrust, Rending Wave) contain explicit checks for allocated talent points (`specialized.allocated_points.contains(node_id)`) to alter skill behavior (e.g., `forcePierce`, `spawnShadow`, `boomerang`, `extraWaves`).
*   **Verification:** Talent effects (stat changes, damage modifications, and behavioral changes) are consistently applied based on allocated points, influencing both stat calculation and real-time skill execution.

#### c. Real-time Attribute Bonus Updates
*   **Implementation:** Whenever talent points are allocated (`SkillSystem::AddTalentPoint`) or buffs/debuffs change (`StatsSystem::UpdateBuffs`), a `StatsDirty` component is added to the entity. `StatsSystem::update` processes these `StatsDirty` components, triggering a full `StatsSystem::Recalculate` of the entity's `CombatStats`.
*   **Verification:** The `StatsDirty` mechanism ensures that all attribute bonuses from talents (and other sources) are reflected in `CombatStats` in real-time, providing an accurate representation of the character's power.

#### d. Functional Completeness During Status Duration
*   **Implementation:** Skills that apply temporary states or buffs (e.g., Blade Ward) create `BuffEffect`s which are managed by `ActiveEffectsComponent` and `StatsSystem::UpdateBuffs`. These buffs have durations and apply their own `StatModifier`s. Channeled skills (`ChannelingComponent`) and summoned entities (`ShadowComponent`, `SwordArrayComponent`, `BladeFormationComponent`) have internal timers (`lifetime`, `channel_timer`, `duration`) that govern their active period.
*   **Verification:** Temporary effects, buffs, and channeled skills maintain their functionality and associated attribute modifications for their specified durations.

### 2.3 Cancellation and State Management

#### a. Immediate Cancellation Mechanism for Skill Effects
*   **Implementation:**
    *   Channeled skills are automatically removed if `channel_timer` expires.
    *   Temporary state components like `PhantomFlashComponent` and `BladeWardComponent` are removed from entities once their internal `remaining` timers expire or specific trigger conditions are met.
    *   `SkillExecution` components are removed from the registry after transitioning through `Settle` state, effectively ending the skill animation/casting sequence.
*   **Verification:** Time-limited effects and skill states are correctly managed and removed upon expiration or specific triggers.

#### b. Thoroughness of State Clearing and Complete Removal of Residual Effects
*   **Implementation:**
    *   `StatsSystem::Recalculate` begins by calling `resetCombatStats`, ensuring `CombatStats` is cleared to default values before new modifiers are applied, preventing "sticky" stats.
    *   Components related to temporary effects (e.g., `ShadowComponent`, `SwordArrayComponent`, `ChannelingComponent`) are explicitly removed from the registry (`registry.destroy(entity)` or `registry.remove<Component>(entity)`) when their duration ends or their logic dictates.
    *   Projectiles (`Projectile`) have `lifeTime` and are removed.
*   **Verification:** The component-based design, coupled with explicit removal logic and stat resetting, ensures that temporary skill effects and their residual impacts are thoroughly cleared from the game state.

### 2.4 Multi-system Collaborative Linkage Verification

#### a. Trigger Conditions for Chain Reactions
*   **Implementation:**
    *   **Sword Intent System:** `SwordIntentComponent` can be empowered (`intent->stacks >= intent->max_stacks`) to modify skills. `SkillSystem::OnSkillHit` contains logic to gain Sword Intent stacks based on `hit_tags` (e.g., `Melee`), `is_crit`, and specific talent nodes.
    *   **Shadow Kill Array (ID 124):** If skill 1 (Flowing Thrust) is empowered, it checks for talent 124 activation. If active, it sets `ShadowKillArrayReady`. Then, `SkillSystem::TryCast` checks for `ShadowKillArrayReady` to duplicate the next cast as a shadow. This is a clear chain reaction.
    *   **Nested Skill Casting:** Channeled skills (Infinite Blades, Mind Blade) and summoned entities (Blade Formation) *shadow cast* other skills (`Rending Wave`, `Flowing Thrust`) periodically, creating continuous chain reactions.
*   **Verification:** Complex chain reactions and inter-skill triggers are explicitly handled within `SkillSystem.cpp`, `StatsSystem.cpp`, and `DamagePipeline.cpp` via hooks, component checks, and conditional logic.

#### b. Compound Effect Overlay Rules
*   **Implementation:**
    *   The `StatCalculation` struct in `StatsSystem.cpp` and its `Result()` method correctly apply stat modifiers in the order: Base + Flat, then multiply by (1 + PercentAdd), then multiply by (1 + PercentMult).
    *   The `DamagePipeline` applies damage modifiers in a structured order: Base Damage > Conversions/Gain Extra > Increased Modifiers > More Modifiers > Crit > Defenses.
*   **Verification:** The system employs well-defined rules for compounding both attribute modifiers and damage modifiers, ensuring predictable and balanced outcomes.

---

## 3. Test Data and Coverage (Static Analysis)

### 3.1 Test Cases Covered (by code logic review)
*   All skills (ID 1-9) in `skills.json` have corresponding `RegisterEffect` callbacks in `SkillSystem::InitHooks`.
*   All `StatType` and `ModifierMode` mappings from `skills.json` to C++ enums (`Stats.hpp`) are consistent as per analysis.
*   The application of `StatModifier`s and `DamageModifier`s from `TalentNode`s, `BuffEffect`s, `Affix`es, `AstrolabeNode`s are traced and verified across `StatsSystem` and `DamagePipeline`.
*   Specific talent node logic (e.g., Flowing Thrust's distance-based damage, Rending Wave's intent scaling, Blade Boomerang's pull, Weakness Insight crit bonus, Shadow Kill Array duplication) are identified and confirmed.
*   Core mechanics like cooldowns, charges, mana cost, Sword Intent decay/gain are covered.

### 3.2 Problem Classification and Statistics

**Inconsistencies / Minor Issues:**
1.  **TalentNode `icon_id` in C++ vs. JSON:** The `TalentNode` struct in `SkillSystem.hpp` has an `icon_id` field (string), but this field is not present in the `talent_tree` nodes within `assets/data/skills.json`.
    *   **Impact:** Minor, likely a UI-related placeholder or future feature for talent node icons that hasn't been implemented in the data yet. Doesn't affect core functionality.

**No Critical Bugs or Major Design Flaws Identified in Core Logic.** The system appears robust.

### 3.3 Suggestions for Fixes / Improvements

1.  **Resolve `TalentNode::icon_id` Inconsistency:**
    *   **Recommendation:** If talent nodes are intended to have unique icons, add an `icon_id` (string) field to each talent node in `assets/data/skills.json` and ensure the `from_json` function in `SkillSystem.hpp` populates it. If not, remove the `icon_id` field from the `TalentNode` struct.
2.  **Explicit Skill-Specific Damage Modifier Scope Documentation:**
    *   **Recommendation:** While the implementation correctly scopes "More" damage modifiers to specific skills, it would be beneficial to add comments or design documentation specifically detailing *why* `DamageModifier`s from talents are sometimes put into `GlobalModifierComponent` (for Convert/GainExtra) and sometimes processed directly in `DamagePipeline` (for More). This clarifies the design intent.
3.  **Refine Boomerang Components**: `BoomerangComponent` only has `returnTimer`. It might benefit from also taking `speed` to control the return velocity, or `target` to ensure it returns to the caster's current position rather than a fixed point. (Already implicitly handled by `Projectile` velocity being reversed, but good to ensure return to caster).
4.  **Error Handling for Missing Components**: Although `try_get` is used, some paths might still assume critical components exist (e.g., `CombatStats`). While ECS typically relies on component presence, consider adding more explicit error logging or default behaviors for critical missing components if unexpected states can occur.

---

## 4. Verification Conclusion

The static verification of NoMoreDay's skill system and talent system reveals a well-structured, modular, and comprehensive implementation. The design effectively separates data definition (JSON) from logic (C++ systems) and handles complex interactions between skills, talents, stats, and damage calculation.

The use of `Tag`s for conditional modifiers, the multi-stage `StatCalculation` in `StatsSystem`, and the detailed `DamagePipeline` ensure that talent choices translate accurately and dynamically into character power and skill behavior. Skill execution, cooldowns, resource management, and various special effects (shadow clones, channeled abilities, buffs) are all robustly implemented. Cancellation and state clearing mechanisms appear thorough.

The codebase demonstrates a strong adherence to performance considerations (e.g., `xsimd`, `Taskflow` for batch damage, `FixedVector`, stat caching). The only minor identified inconsistency (talent node `icon_id`) does not impact core functionality. The system is well-prepared for dynamic testing and balancing efforts.
