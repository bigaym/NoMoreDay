# Track Specification: `影杀阵` (Shadow Kill Array) Safeguards & Refinement

## Overview
The `影杀阵` (Skill ID 124) talent currently duplicates the player's next skill execution. To prevent game-breaking scaling and ensure mechanical balance, we are introducing a "Shadow Clone" system with explicit resource costs, cooldowns, and tag-based exclusions.

## Functional Requirements
1.  **Shadow Clone Manifestation:**
    -   When `影杀阵` is active (triggered by consuming Sword Intent), the next valid skill execution will spawn a brief "Shadow Clone" at the player's position.
    -   The Shadow Clone will execute the same skill after a negligible delay.
    -   The Clone entity is temporary and disappears immediately after the skill execution finishes.
2.  **Exclusion Rules:**
    -   Skills with any of the following tags are **ineligible** for duplication:
        -   `Movement` (e.g., Dashes, Blinks).
        -   `Buff` / `Aura` (Passive or persistent effects).
        -   `Channeled` (Skills requiring continuous hold).
3.  **Resource & Combat Penalties:**
    -   **Mana Cost:** The duplicated skill execution consumes **50%** of the original skill's Mana cost from the player's resource pool. If Mana is insufficient, the duplication fails.
    -   **Damage Penalty:** Skills cast by the Shadow Clone deal **50% reduced damage** (0.5x multiplier applied to final damage).
4.  **Internal Cooldown (ICD):**
    -   The duplication effect has a fixed internal cooldown of **3 seconds** to prevent burst abuse in high-attack-speed builds.

## Non-Functional Requirements
-   **Performance:** Shadow clones must be lightweight entities that do not trigger recursive duplication (clones cannot spawn clones).
-   **Visual Feedback:** The shadow clone should have a distinct visual (e.g., semi-transparent or tinted version of the player character).

## Acceptance Criteria
- [x] Executing a movement skill does NOT trigger the shadow clone.
- [x] Valid skill duplication consumes an additional 50% Mana.
- [x] Damage from the shadow clone's skill is verified to be 50% of the player's damage.
- [x] The 3-second internal cooldown is strictly enforced.
- [x] Automated tests verify the tag exclusion and resource consumption logic.

## Out of Scope
-   Modifying the visual assets of the player characters.
-   Changing the "Sword Intent" accumulation logic.
