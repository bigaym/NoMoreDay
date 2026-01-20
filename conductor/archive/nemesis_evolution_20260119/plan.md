# Track: Nemesis Evolution System (Monster Affix V2.0 - Part 4)

**Goal**: Implement the "Meta-Game" evolution system. The Nemesis boss adapts to the player's playstyle.

## 1. Data Persistence
- [ ] **Player Combat History**:
    - Create `PlayerCombatHistory` component/resource.
    - Track:
        - Damage dealt by type (Phys/Fire/Cold/etc.).
        - Average engagement distance (Melee vs Ranged).
        - Average kill time (Burst vs DoT).
    - Save/Load this history in `SaveManager`.

## 2. Nemesis Generation Logic
- [ ] **Adaptive Algorithm**:
    - Analyze `PlayerCombatHistory` during map generation.
    - Select specific "Evolution Traits" (Tags) based on analysis.
- [ ] **Evolution Traits Implementation**:
    - `[Adaptive Resistance]`: +50% Res to player's primary element.
    - `[Anti-Kite]`: If player is Ranged -> Force `Teleporter` or `Vortex`.
    - `[Anti-Burst]`: If player is Burst -> Add `PhaseShield` (Invuln at 50% HP).

## 3. UI & Presentation
- [ ] **Nemesis Introduction**:
    - When Nemesis spawns, show a UI notification detailing its "Evolution" (e.g., "Nemesis Adapted: Fire Resistance").
    - Visual cue on the monster (e.g., specific color tint or aura).

## 4. Final Polish
- [ ] **Review**: Ensure the evolution feels "fair" and not just frustrating.
- [ ] **Testing**: Simulate different player builds and verify Nemesis adapts correctly.
