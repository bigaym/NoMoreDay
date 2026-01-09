# Plan: Fix 'Increased' Damage Multiplier Bug & Re-tune Scaling

## Phase 1: Research & Reproduction [checkpoint: baca902]

- [x] Task: Review `src/systems/StatsSystem.cpp` to pinpoint the exact line where `DamageIncreased` modifiers are combined.
- [x] Task: Create a reproduction test case in a new test file `tests/IncreasedDamageBugTest.hpp` that demonstrates the current exponential scaling (e.g., verifying that 50% + 50% equals 2.25x instead of 2.0x). [b467295]
- [x] Task: Conductor - User Manual Verification 'Phase 1: Research & Reproduction' (Protocol in workflow.md)

## Phase 2: Core Logic Fix [checkpoint: 8dc7c2c]

- [x] Task: Modify `src/systems/StatsSystem.cpp` to use additive aggregation for all `DamageIncreased` stat types.
- [x] Task: Verify the fix using the `IncreasedDamageBugTest.hpp` created in Phase 1 (it should now expect 2.0x).
- [x] Task: Run all existing tests in `tests/` and identify which ones fail due to the changed damage numbers.
- [x] Task: Conductor - User Manual Verification 'Phase 2: Core Logic Fix' (Protocol in workflow.md)

## Phase 3: Data Re-tuning & Test Updates

- [ ] Task: Audit `assets/data/affixes.json`, `assets/data/skills.json`, and `assets/data/astrolabe.json` for significant "Increased Damage" values.
- [ ] Task: Apply a compensatory buff (approx. 20-40%) to these values in the JSON files to mitigate the power loss.
- [ ] Task: Update expected values in `tests/CombatSystemTest.hpp`, `tests/DamagePipelineTest.hpp`, and any other failing tests to match the new formula and tuned data.
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Data Re-tuning & Test Updates' (Protocol in workflow.md)

## Phase 4: Final Integration & Verification

- [ ] Task: Run the full test suite (`.\build.bat` or `ctest`) to ensure no regressions.
- [ ] Task: Perform a final code review of `StatsSystem` to ensure the logic is clean and well-commented.
- [ ] Task: Conductor - User Manual Verification 'Phase 4: Final Integration & Verification' (Protocol in workflow.md)
