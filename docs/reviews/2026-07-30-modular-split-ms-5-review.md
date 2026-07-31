# MS-5 UI Presentation Ownership Review

## Review Target

- Plan: `docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- Evidence: `docs/reports/modular-split-exe-lib-dll/ms-5/evidence.md`
- Standards: `docs/workflows/review.md`, `conductor/code_standard.md`
- Protected exclusion: `docs/designs/modular-split-exe-lib-dll-design.md` is user-owned worktree state and was neither read for review nor staged.

## Round One

**Conclusion:** 修改

### Findings

- **Blocker (process)** The `git mv` used for the UIRenderer move left two `R100` renames staged; `git diff --cached --quiet` exited 1. Review requires an empty index.
- **Blocker (evidence)** `evidence.md:28` claimed per-file MSBuild compilation evidence ("zero C1083 ... in the MSBuild log") that is not auditable from the referenced `ms-5-build.log`: `build.bat` discards the per-run MSBuild detail log (build.bat 536-544), so per-file MSBuild lines cannot be independently verified.
- **Low** "byte-identical" wording should be qualified at the Git clean-filter/blob level under `core.autocrlf=true`.

### Verified in Round One

- 20 include sites updated; `git grep 'engine/render/UIRenderer' -- src tests` zero matches; no forwarding headers.
- `UITests.cpp` changed only the 3 path-sensitive candidate arrays (9 additions / 9 deletions); test logic untouched.
- Ledger exactly `85 -> 71`: only the 14 MS-5 rows removed (11 cpp + 3 hpp); 66 MS-6 + 5 MS-7 rows intact.
- `check_module_boundaries.py`: PASS `71/71`, 20 files; Python suites 25/25; `git diff --check` PASS.
- Plan status correct: MS-5 `[~]`, MS-5.1 `[x]`, MS-5.2 pending.
- No CMake/PCH/Types/`build.bat`/GPU/RenderSystem changes; `PlayerHUD.cpp` include-only update; MS-6 ledger edges untouched.
- Build wrapper log contains both success markers.

### Required Actions

1. Clear the staged renames (`git reset -q`).
2. Narrow the evidence claims to verifiable facts and preserve an auditable focused-test output.

## Final Acceptance Review

**Conclusion:** 提交

### Resolved

- Index empty (`git diff --cached --quiet` exits 0); worktree boundary matches the MS-5 package exactly.
- `evidence.md` narrowed the MSBuild claim to verifiable facts (obj rebuild timestamp, linked artifacts, wrapper log markers, exit 0) and documented the `build.bat` log-discard behavior.
- Focused tests re-run with `--success`; output preserved at `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-5-ui-tests.log`: `test cases: 4 | 4 passed | 0 failed | 670 skipped`, `assertions: 20 | 20 passed | 0 failed`, `Status: SUCCESS!`, exit 0. The log names the three assertion-bearing cases; `[Tech] UIRenderer - Tooltip Logic Smoke Test` (UITests.cpp:916) is a no-assertion smoke test for which doctest emits no `TEST CASE` header — separately verified: 1 passed / 0 assertions / SUCCESS.

### Scope Alignment

- `src/engine/render/UIRenderer.hpp/.cpp` moved to `src/game/systems/ui/`; `.hpp` byte-identical to HEAD blob, `.cpp` differs only in the line-1 self-include; namespace `NoMoreDay` unchanged.
- 20 include sites updated; no forwarding headers; `git grep 'engine/render/UIRenderer' -- src tests` zero matches.
- `UITests.cpp` only the 3 path-sensitive candidate arrays updated.
- Ledger `85 -> 71` (14 MS-5 rows removed; 66 MS-6 + 5 MS-7 intact).
- No CMake, PCH, Types, `build.bat`, GPU, RenderSystem, or P0 rendering work modified; `UIRenderer` keeps allowed Game -> Engine resource includes.

### Verification

- `python scripts/check_module_boundaries.py`: PASS, `71/71` across 20 files.
- `python scripts/check_core_candidate_contract.py`: PASS.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py tests/python/CoreCandidateContractCheckerTest.py`: PASS, 25 tests.
- `cmd.exe /c build.bat`: PASS; `C:\Users\yuminao\AppData\Local\Temp\opencode\ms-5-build.log` contains both success markers; exit 0.
- Focused UI tests: 4/4 cases, 20/20 assertions, SUCCESS (auditable output saved).
- `git diff --check` excluding the protected design document: PASS.

### Findings

- None.

### Accepted Risks

- `UIRenderer` still draws via raylib through Engine resource registries (`AssetLoadingSystem`, `UIAssetRegistry`); no Engine drawing primitive extracted, matching the plan's presentation-policy objective.
- `src/engine/render/` retains remaining render/GPU files; UIRenderer's MS-6 ledger edges were not touched.
- `build.bat` per-run MSBuild detail logs are discarded by design; build evidence is limited to wrapper markers plus artifact timestamps.

### Next Step

- Commit this reviewed MS-5 package; mark MS-5 complete in the plan.