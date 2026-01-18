# Monster Affix System Specification

## 1. Overview
Implement the Monster Affix system described in `设计文档/怪物词缀设计.md`. This system transforms standard enemies into Elites/Champions by attaching gameplay modifiers that alter stats, behaviors, and mechanics.

## 2. Core Architecture

### 2.1 ECS Components
**`MonsterAffixComponent`**
Stored in `game/components/EnemyComponent.hpp`.
```cpp
struct MonsterAffixComponent {
    std::vector<uint16_t> affixes; // List of Affix IDs
    // Cached flags for fast checks
    bool has_on_hit = false;
    bool has_on_death = false;
    bool has_update = false;
};
```

**`AffixStateComponent`**
Runtime state for specific mechanics.
```cpp
struct AffixStateComponent {
    // Shared timers/counters
    float generic_timer_1 = 0.0f; // e.g., for Molten tick
    float generic_timer_2 = 0.0f; // e.g., for Teleport cooldown
    entt::entity linked_target = entt::null; // e.g., for Shielding target
};
```

### 2.2 MonsterAffixSystem
A new system `MonsterAffixSystem` handles the logic. It should effectively dispatch events based on the affixes present.

*   **Logic Hooks**:
    *   `Update(dt)`: For active effects like Molten trails or periodic buffs.
    *   `OnHit(attacker, victim, damage)`: For Vampiric, Nullifier, etc.
    *   `OnDeath(victim)`: For Avenger, Explode.

*   **Affix Definition (Data-Driven)**:
    We need an `AffixRegistry` (singleton or static) that defines:
    *   Name/Desc.
    *   Stat Modifiers (vector of `StatModifier` -> Applied once on spawn).
    *   Callback functions/Enums for logic.

## 3. Specific Affix Implementations

### 3.1 Stat-Based (Prefix/Suffix)
Implemented via `StatsSystem` modifiers applied at spawn.
*   **Fast**: +MoveSpeed, +AttackSpeed.
*   **Tanky**: +Armor, +Health.
*   **Powerful**: +DamageMult.

### 3.2 Mechanic-Based
*   **Molten (熔火)**:
    *   *Update*: Every 0.5s, spawn a "FireZone" entity at current position.
    *   *FireZone*: `HazardComponent`, deals Fire damage/sec, lasts 3s.
*   **Teleporter (闪烁)**:
    *   *Update*: If `distance(player) > threshold` and `timer > cooldown`, set position to player.back().
*   **Nullifier (虚无)**:
    *   *OnHit*: `registry.remove<BuffComponent>(target)`.
*   **Shielding (护盾)**:
    *   *Update*: Find nearest ally without shield, apply `InvulnBuff`.

## 4. Visuals (VFX)
*   **Outline Shader**: Enemies with affixes get a colored outline (Gold/Blue/Red).
*   **Particles**:
    *   Molten: Fire trail (GPU Particles).
    *   Frozen: Ice mist.

## 5. Integration
*   **EnemySpawnSystem**: When spawning Elite/Rare, randomly select affixes from allowed pool (based on Enemy Rank).
*   **NemesisGenerator**: Uses this system but forces specific "Evolved" affixes.
