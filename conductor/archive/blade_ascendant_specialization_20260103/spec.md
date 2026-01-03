# Specification: Blade Ascendant Skill Specialization (Remaining 7 Skills)

## Overview
This track involves the implementation of the specialization talent trees and unique mechanics for the remaining 7 active skills of the **Blade Ascendant** class. Building upon the established infrastructure used for *Flowing Thrust* and *Rending Wave*, we will complete the skill system's depth by adding complex branches for automation, defense, ultimates, and utility.

## Skills to be Implemented
1.  **万剑诀 (Blade Formation):** Sentry-style automated blades with branches for quantity (Sword Rain), quality (Heavy Blade), and defense (Guardian Blade).
2.  **剑气护体 (Blade Ward):** Defensive buff with branches for armor/resistances, block/parry, and counter-attacks (Blade Reflection).
3.  **万剑归宗 (Infinite Blades):** Ultimate channeling skill with branches for frequency, auto-targeting, and Sword Intent synergy.
4.  **剑阵·诛仙 (Sword Array: Execution):** Area-of-effect zone with branches for resonance, debuffs (Armor Shred), and player buffs within the array.
5.  **心剑·无影 (Mind Blade: Shadowless):** High-frequency channeling sniper skill with branches for stacking damage (Mind Flow), auto-lock, and Intelligence scaling.
6.  **御剑·回旋 (Blade Boomerang):** Returning projectile with branches for speed/scaling, magnetic pull (CC), and multi-hit cutting.
7.  **绝影闪 (Phantom Flash):** Strategic counter-blink with branches for high-damage riposte, stealth/assassination, and evasion resets.

## Functional Requirements
- **Data Integration:** Populate `assets/data/skills.json` with the complete talent trees for all 7 skills, including IDs, descriptions, coordinates, prerequisites, and stat modifiers.
- **Skill Hooks:** Implement specialized C++ logic (hooks) for mechanical shifts that cannot be handled by pure stat changes (e.g., pulling enemies, invisibility, automated counter-attacks, channeling stack logic).
- **Stat Scaling:** Ensure all new nodes correctly apply modifiers to the `SkillComponent` or `CombatStats` through the `StatsSystem`.
- **Pre/Post Cast Logic:** Update skill execution pipelines to handle new triggers (e.g., "On Dodge" triggers for Phantom Flash, "On Channeling" frequency scaling).

## Non-Functional Requirements
- **Performance:** Maintain the 10,000+ entity performance target; skill hooks must be optimized for zero-allocation in the main loop.
- **Maintainability:** Use the hybrid Data-Driven/Hook architecture to keep skill logic modular and easy to tune.

## Acceptance Criteria
- All 7 skills have fully functional talent trees visible and interactable in the UI.
- All "Ultimate" (终极) nodes trigger their unique mechanical effects correctly.
- Automated unit tests verify that every node applies its intended stat modifiers and prerequisites.
- Skill hooks trigger as expected (e.g., Blade Boomerang pulls enemies toward the player on return).

## Out of Scope
- Visual asset creation (textures/icons) - will use placeholders or existing assets.
- Sound effect implementation.
- Balance tuning (numerical values are subject to change after implementation).