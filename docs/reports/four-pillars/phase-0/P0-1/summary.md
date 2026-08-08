# P0-1 Legacy/Version Inventory Summary

Generated at (UTC): 2026-03-02T15:52:46+00:00
Scanned files: 476
Files with matches: 71
Total marker matches: 222

## Marker counts

- deprecated: 7
- fallback: 118
- legacy: 14
- v2: 4
- v3: 63
- v4: 8
- v5: 8

## Classification counts (heuristic)

- metadata_only: 2
- migration_path_dependent: 86
- removable_runtime_branch: 134

## Top hotspots by file

- src/engine/render/core/QualityTierManager.cpp: 51 matches
- src/engine/vfx/VFXSequenceManager.cpp: 16 matches
- src/game/systems/world/BiomeMapGenerator.cpp: 11 matches
- src/engine/render/core/RenderConstants.hpp: 7 matches
- src/game/data/SkillRegistry.cpp: 6 matches
- src/engine/render/GPUData.hpp: 6 matches
- src/engine/render/RenderSystem.cpp: 6 matches
- src/game/systems/item/ItemFactory.cpp: 5 matches

## Worktree mapping prerequisite check

- Command: `python scripts/check_worktree_mapping.py`
- Result: pass
- Key output:
  - `[Mapping Check] OK: third_party -> D:\PRJ\NoMoreDay\third_party`
  - `[Mapping Check] PASS: all required mappings are available.`

## Reproduction

- Regenerate inventory and this summary: `python scripts/four_pillars_phase0_inventory.py`
- Re-run mapping prerequisite check: `python scripts/check_worktree_mapping.py`
