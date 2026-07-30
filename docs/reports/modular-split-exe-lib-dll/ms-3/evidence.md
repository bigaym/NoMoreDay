# MS-3 Input and ECS Physics Ownership Evidence

## Changes

- Moved `InputSystem` from `src/engine/input/` to `src/game/systems/input/`.
- Moved `PhysicsSystem` from `src/engine/physics/` to `src/game/systems/physics/`.
- Updated active Game and test consumers to the Game-owned headers.
- Added `tests/unit/InputSystemTests.cpp` to verify typing clears movement, attack, and dash input after forcing every other UI gate closed; its scope guard restores typing, skill-tree, quantity-popup, and the exact Astrolabe visibility/fade state.
- Removed the six InputSystem and nine PhysicsSystem stale reverse edges from the MS-0 ledger.

## Boundary Ledger

- `python scripts/check_module_boundaries.py`: PASS, `113/113` observed/ledger direct quoted edges.
- The checker reports 33 source files: the prior 36-file inventory lost the InputSystem implementation and both PhysicsSystem files that carried the removed edges.

## Verification

- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat check`: PASS.
- `cmd.exe /c build.bat` log: `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-3-build-final.log`; final run contains both `Build completed successfully` and `All steps completed successfully`.
- `NoMoreDayTests.exe --test-case="[Unit] InputSystem - Typing clears gameplay actions"`: PASS, 1 case and 7 assertions.
- `NoMoreDayTests.exe --test-case="[Bugfix] Dash - Dash Tunneling Reproduction"`: PASS, 1 case and 2 assertions.
- `NoMoreDayTests.exe --test-case="[Performance] PhysicsSystem*"`: PASS, 3 cases and 3 assertions.
- `git diff --check` (excluding the protected design document): PASS.

## Deferred Scope and Risks

- `SpatialGrid` remains deferred because of the stale `RenderSystem` include.
- `SIMDSpatialGrid` remains deferred because it requires separate primitive decoupling.
- No P0 rendering, GPU, RenderGraph, or ResourceManager files were changed.
- The first full-build attempt exposed only the new test's incorrect namespace qualification for global ECS/UI types; it was corrected before the passing final build.
