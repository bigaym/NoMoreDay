# Hybrid Barrier System Specification

## 1. Overview
Implement a Hybrid Barrier system that combines mechanisms from Path of Exile (Energy Shield) and Last Epoch (Ward). The barrier acts as a protective layer above Health.

## 2. Core Concepts
*   **Energy Shield (ES) Mode**: Auto-regenerates after strictly not taking damage for `BarrierDelay` seconds. Capped by `MaxBarrier`.
*   **Ward Mode**: Decays over time if above `MaxBarrier`. Used for temporary shields gained from hits/kills.
*   **Runtime State**: Stored in `BarrierComponent`.
*   **Calculated Stats**: `CombatStats` provides `max_barrier`, `barrier_regen`, `barrier_decay`, `barrier_retention`.

## 3. Data Structures

### 3.1 Components
**New Component: `BarrierComponent`** (in `Common.hpp` or `Stats.hpp`)
```cpp
struct BarrierComponent {
    float current_value = 0.0f;
    float last_damage_time = 0.0f; // Timestamp of last damage taken
    float accumulated_decay = 0.0f; // For smooth decay
};
```

### 3.2 CombatStats Updates (Already Applied)
*   `max_barrier`: Soft cap for ES regen. Limits the "resting" shield.
*   `barrier_regen`: Flat regen per second (active when safe).
*   `barrier_decay`: Percent decay per second (active when > max). Default 20%.
*   `barrier_retention`: Reduces decay rate. `EffectiveDecay = Base / (1 + Retention)`.
*   `barrier_delay`: Seconds to wait before regen starts.

## 4. System Logic

### 4.1 BarrierSystem (New or Integrated into RegenSystem)
*   **Frequency**: Every frame (dt).
*   **Logic**:
    For each entity with `BarrierComponent` & `CombatStats`:
    1.  **Regen State**:
        If `Time.now - last_damage_time > barrier_delay` AND `current < max_barrier`:
        `current += barrier_regen * dt`
    2.  **Decay State**:
        If `current > max_barrier`:
        `decay_rate = barrier_decay / (1 + barrier_retention)`
        `loss = (current - max_barrier) * decay_rate * dt`
        `current -= loss`
    3.  **Clamp**: Ensure `current >= 0`.

### 4.2 DamagePipeline Interaction
*   **Priority**: Damage is subtracted from Barrier first.
*   **Bypass**: Chaos/True damage bypasses Barrier (Check `DamageType`).
*   **Interruption**: On taking damage (to Barrier OR Health), update `last_damage_time = Time.now`.

### 4.3 Stats Calculation (`StatsSystem`)
*   **Intelligence Scaling**: `1 INT = +1% BarrierRetention`.
*   **Equipment**: Sum up `MaxBarrier`, `BarrierRegen` from items.

## 5. UI Representation
*   **Overlay**: A cyan/blue bar overlaying the Health bar.
*   **Visuals**: Use `UIRenderer` to draw the barrier portion. If `current > max_health`, it might cover the whole bar or use a different texture style (e.g. glowing border).

## 6. Testing Plan
*   **Unit Test**: Helper function to simulate time passing and check regen/decay.
*   **Integration**: Spawn dummy with massive barrier, hit it, verify delay and regen.
