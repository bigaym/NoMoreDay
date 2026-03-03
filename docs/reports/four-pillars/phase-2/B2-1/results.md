# B2-1 Results: Skill Orchestrator/Service Split

## Scope
- Extracted one skill-contract cast-constraint unit from `SkillSystem` orchestration into a dedicated service under `src/game/systems/skill/`.
- Routed `SkillSystem::TryCast` through the extracted service.
- Added unit tests that directly exercise the extracted service behavior.

## Extracted Component
- New service: `skill::ValidateContractCastConstraints`
- Files:
  - `src/game/systems/skill/SkillCastConstraintService.hpp`
  - `src/game/systems/skill/SkillCastConstraintService.cpp`
- Responsibility:
  - Count allocated trigger/transmuter nodes for a cast attempt.
  - Enforce `max_transmuters` / `max_triggers` contract limits.
  - Resolve transmuter mutex to a single active transmuter by contract preference order.
  - Preserve existing warning diagnostics for blocked casts and mutex collapse.

## Behavior Preservation Notes
- No gameplay intent or contract semantics were changed.
- `SkillSystem::TryCast` still receives the same transmuter/trigger outputs and same pass/fail gate.
- Existing transmuter mutex diagnostics remain (`SKILL_GUARD_TRANSMUTER_MUTEX`).

## Verification Commands
- Build:
  - `./build.bat`
- Unit tests (skill-focused):
  - `./bin/NoMoreDayTests.exe --test-case="[Unit]*Skill*"`
- Integration tests (skill-focused):
  - `./bin/NoMoreDayTests.exe --test-case="[Integration]*Skill*"`

## Verification Results
- Build: success.
- Unit skill-focused suite: success (`27` passed, `0` failed).
- Integration skill-focused suite: success (`20` passed, `0` failed).
