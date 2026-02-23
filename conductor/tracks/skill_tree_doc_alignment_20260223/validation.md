# Validation - skill_tree_doc_alignment_20260223

## Scope

- Aligned `assets/data/skills.json` specialization trees for skills `1..9`.
- Updated compact contracts in `assets/data/skill_contracts_compact.json` and regenerated runtime contracts via `python scripts/gen_skill_contracts.py`.
- Synced contract/guard tests for updated trigger mapping.

## Mismatch Matrix (Before -> After)

| Skill | Nodes | Trigger Count | Trigger Node | Keystone Count | Synergy Count |
|---|---:|---:|---|---:|---:|
| 1 | 21 -> 25 | 1 -> 1 | `[114] -> [114]` | 7 -> 7 | 1 -> 1 |
| 2 | 17 -> 25 | 0 -> 1 | `[] -> [233]` | 5 -> 8 | 1 -> 1 |
| 3 | 18 -> 25 | 0 -> 1 | `[] -> [373]` | 6 -> 9 | 1 -> 1 |
| 4 | 16 -> 25 | 0 -> 1 | `[] -> [451]` | 8 -> 10 | 1 -> 1 |
| 5 | 17 -> 25 | 0 -> 1 | `[] -> [533]` | 6 -> 8 | 1 -> 1 |
| 6 | 17 -> 25 | 0 -> 1 | `[] -> [633]` | 7 -> 9 | 1 -> 1 |
| 7 | 15 -> 25 | 0 -> 1 | `[] -> [713]` | 5 -> 9 | 1 -> 1 |
| 8 | 16 -> 25 | 0 -> 1 | `[] -> [831]` | 4 -> 8 | 1 -> 1 |
| 9 | 16 -> 25 | 0 -> 1 | `[] -> [951]` | 5 -> 7 | 1 -> 1 |

## Notes / Exceptions

- Design section `3.0` target keystone density is `2-3`, but current runtime compatibility keeps many existing 1-point legacy nodes mapped as `Keystone`.
- This track preserves existing behavior-critical node IDs and prioritizes trigger/transmuter/synergy contract alignment plus node-scale alignment.

## Contract/Test Evidence

- `python scripts/gen_skill_contracts.py` -> PASS (updated contracts)
- `python scripts/gen_skill_contracts.py --check` -> PASS
- `build.bat` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` -> PASS
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` -> PASS

