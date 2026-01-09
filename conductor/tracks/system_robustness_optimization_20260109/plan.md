# Implementation Plan: System Robustness & Performance Optimization

## Phase 1: Keystone Balancing & Data Audit
Adjust high-impact 'More' multipliers and verify the data-driven scaling. [checkpoint: 7f8a2b3]

- [x] Task: Update `astrolabe.json` to reduce `心剑合一` (ID: 301) multiplier from 50% to 15%.
- [x] Task: Audit `astrolabe.json` for other Keystones with >20% 'More' damage and normalize them (e.g., `剑心通明`).
- [x] Task: Implement a unit test in `AstrolabeRegistryTest.hpp` to verify the new values are parsed correctly.
- [x] Task: Conductor - User Manual Verification 'Phase 1: Keystone Balancing & Data Audit' (Protocol in workflow.md)

## Phase 2: StatsSystem Performance Optimization
Enforce frequency reduction and optimize the calculation hot-path for high refresh rates. [checkpoint: 7f8a2b3]

- [x] Task: Audit `StatsSystem::Recalculate` and all call sites (e.g., `BuffSystem`, `EquipmentSystem`) to ensure `StatsDirty` is the *only* trigger for recalculation.
- [x] Task: Optimize `StatsSystem::GetStatWithTags` by replacing string-based tag lookups with a more efficient mechanism (e.g., bitsets or cached enum indices).
- [x] Task: Implement a caching layer for tag-based stat queries that invalidates only when `StatsDirty` is set.
- [x] Task: Create a benchmark test `StatsBenchmark.cpp` to measure `Recalculate` time for 1,000 entities and ensure it meets 240 FPS targets (< 0.5ms total budget).
- [x] Task: Conductor - User Manual Verification 'Phase 2: StatsSystem Performance Optimization' (Protocol in workflow.md)

## Phase 3: Stat Cap Logic & UI Visualization
Implement soft/hard cap logic in the backend and provide clear UI feedback. [checkpoint: 7f8a2b3]

- [x] Task: Define hard caps for targeted stats (Resistances: 75%, CDR: 60%, etc.) in `GameConstants.hpp`.
- [x] Task: Update `StatsSystem::Recalculate` to store both "Raw Value" and "Effective Value" (Capped) in `CombatStats`.
- [x] Task: Update `PlayerHUD` or `StatsUI` to display the "Capped" status for relevant stats.
- [x] Task: Write an integration test in `StatsSystemTest.hpp` to verify that raw stats exceeding caps are correctly truncated in the `CombatStats` component.
- [x] Task: Conductor - User Manual Verification 'Phase 3: Stat Cap Logic & UI Visualization' (Protocol in workflow.md)