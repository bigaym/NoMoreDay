# B2-2 Results: Combat Pipeline Stage Extraction

## Scope
- Extracted the damage mitigation settlement stage from `DamagePipeline::Calculate` into a dedicated combat service.
- Routed the single-target pipeline through the extracted service while preserving call inputs and output semantics.
- Added direct unit coverage for the extracted stage invariants.

## Extracted Stage
- New service: `DamageMitigationService::Apply`
- Files:
  - `src/game/systems/combat/DamageMitigationService.hpp`
  - `src/game/systems/combat/DamageMitigationService.cpp`
- Responsibility:
  - Apply block multiplier when a hit was blocked.
  - Apply resistance caps and resistance mitigation.
  - Apply physical armor mitigation with armor penetration.
  - Apply global damage reduction and endgame damage-taken multiplier.

## Behavior Preservation Notes
- `DamagePipeline::Calculate` still controls the same stage ordering (crit -> mitigation -> suppressor -> special interceptors/events).
- Mitigation inputs are unchanged: instance tags/type, attacker context, defender stats, endgame aggregate, skip-mitigation flag, and block state.
- Existing combat mitigation contracts remain green after routing through the service.

## Tests Added/Adjusted
- Added: `tests/unit/DamageMitigationServiceTests.cpp`
  - `[Unit] DamageMitigationService - Physical uses armor and global reduction`
  - `[Unit] DamageMitigationService - Elemental applies resistance cap and DR`

## Verification Commands
- Build:
  - `./build.bat`
- Combat-focused unit tests:
  - `./bin/NoMoreDayTests.exe --test-case="[Unit]*Combat*,[Unit]*Damage*,[Unit]*Defense*"`
- Combat-focused integration tests:
  - `./bin/NoMoreDayTests.exe --test-case="[Integration]*Combat*"`

## Verification Results
- Build: success.
- Combat-focused unit filters: success (`39` passed, `0` failed).
- Combat-focused integration filters: success (`19` passed, `0` failed).
