# D4-3 Residual Risk

## Residual Risks

1. Runtime-to-canonical migration is intentionally bounded to the current `skill_spec` runtime JSON shape with a single-op mapping (`ADD_STAT_FLAT`, `ADD_STAT_PERCENT_MULT`); newly introduced opcodes are dropped until mapping support is added.
2. Canonical migration infers fields that are not represented in runtime payload (`conditions.min_player_level` defaults to `1`, `tags` derived from `stat_path` token), which is acceptable for this slice but not full-fidelity for future broader migration.
3. Other modifier domains (`equipment`, `talent`, `map`, `monster`) are out of D4-3 scope and still use existing paths.

## Why Acceptable For This Slice

- D4-3 objective is bounded migration completion for the canonicalized `skill_spec` modifier slice, not cross-domain migration.
- Unsupported records are now explicitly rejected into `drop-list.json` instead of silently passing through.
- Build pre-check now enforces both artifact drift and drop-free migration state for this slice.
