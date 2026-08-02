# MS-8 Evidence

MS-8 is the closing milestone of the module split. It converges the physical
layout onto the four-layer target manifests, retires the last legacy
monolithic naming in the boundary checker, removes the empty legacy
`src/systems/` directory, and records the accepted technical-debt register for
follow-up. No product C++ source, CMake target graph, or ledger policy was
changed.

## Directory vs. four-layer target manifest

Each layer's explicit `add_library` source manifest (`src/{app,game,engine,
core}/CMakeLists.txt`, no `GLOB_RECURSE`) was compared against the on-disk
implementation files (`.c/.cc/.cpp/.cxx`).

| Layer | Manifest count | On-disk .cpp | Match |
|-------|----------------|--------------|-------|
| `src/engine` | 67 | 67 | 67/67, 0 missing, 0 extra |
| `src/game`   | 136 | 149 | 136 manifest + 13 `SkillBehaviors` OBJECT sources, 0 missing, 0 extra |
| `src/core`   | 2 | 2 | 2/2, 0 missing, 0 extra |
| `src/app`    | 1 | 2 | 1 manifest (`Game.cpp`) + `main.cpp` owned by the `NoMoreDay` exe, 0 missing, 0 extra |

Exact match, 0 omissions, 0 extras. The only on-disk `.cpp` files not listed
in a layer manifest are the two documented special cases:

- **13 `SkillBehaviors` OBJECT sources** under
  `src/game/systems/skill/behaviors/*.cpp`, owned by the root `SkillBehaviors`
  OBJECT target (root `CMakeLists.txt` `file(GLOB_RECURSE ...)`).
- **`src/app/main.cpp`**, owned by the executable target
  `add_executable(NoMoreDay src/app/main.cpp)` in root `CMakeLists.txt`.

These match the MS-7 review record exactly (`game 149 = 136 + 13 behaviors`,
`engine 67/67`, `core 2/2`, `app 1 + main`).

## No forwarding includes

All `src/` headers were scanned for single-include re-export wrappers (a header
whose only substantive line is one quoted include of an `engine/`/`core/`/
`game/`/`app/` header). Result: **0 forwarding headers** remain.

## No missing sources

Every `.cpp` listed in the four layer manifests exists on disk. Result:
**0 missing sources**; every manifest entry resolves.

## Legacy naming convergence

`scripts/check_module_boundaries.py` `_candidate_for_source` retired the last
monolithic/legacy fallback strings (strings only; no logic change):

- Line 261 (PCH candidate): `("LegacyLowerPch", "lower-layer PCH",
  "legacy_global_pch")` -> `("EngineOwnedPch", "engine-owned PCH",
  "engine_owned_pch")`.
- Line 265 (candidate-root owner): `f"legacy_monolithic_{target}"` ->
  `f"{layer.lower()}_layer"` (`NoMoreDayEngine` -> `engine_layer`,
  `NoMoreDayCore` -> `core_layer`).

The current ledger has zero entries, so no ledger rows needed re-typing. The
checker emits no literal `legacy` string (the legacy-gate scanner's scope is
`src/`, and the checker is `scripts/`, but the rename removes the token there
too for consistency).

## Test assertion sync (A2)

`tests/python/ModuleBoundaryCheckerTest.py` fixture
`make_fixture_entry` referenced the old owner string
`"current_owner": "legacy_monolithic_NoMoreDayEngine"` (line 48). Because
`load_ledger` validates each entry's `current_owner` against
`_candidate_for_source`, the fixture was **required** to change to
`"engine_layer"`. The pre-audit note "current fixture does not reference it"
was inaccurate; the sync was mandatory, not optional.

## Empty directory removal (A1)

`src/systems/` (0 entries, untracked, disk-only residue) was removed with
`Remove-Item -Recurse`. No tracked file was affected: `git status --short`
shows no `src/systems`-related M/D after removal.

## Technical-debt register (7 items, for follow-up)

1. **RG-3 resource lifecycle** — rendergraph resource registry coverage and
   frame advancement incomplete (VAO and other buffer owners not yet emitted;
   `debt_register.md`). `GPUEntitySystem::Shutdown()` retains its known leaks
   by hard constraint, intentionally not fixed during MS-6/7.
2. **S1b `PersistSelectionMetadata` defect** —
   `QualityTierManager::PersistSelectionMetadata` persists `m_v3Config` (not
   `m_config`), and `TryLoadV3ConfigFromSettings` never reads the GI fields
   back, so `m_v3Config.giEnabled` stays default false and any persist path
   writes `render.gi.enabled: false` back to settings.json (gpu-s7 §5,
   unchanged).
3. **Game->App reverse-include checker blind spot** — the module-boundary
   checker scans Engine/Core candidates for `game/`/`app/` prefixes only;
   Game->App reverse includes are outside its scope. Game->App deps were
   cleared in MS-7 Batch 1, so there is no live edge today, but the guard
   structurally cannot detect a future regression.
4. **uint32 `effectiveTagMask` coupling** — the uint32 tag mask is coupled to
   the Tag element bit positions (0-6) and the `SkillVfxElementTagMask` bit
   layout (MS-6 review G). Lossless today; sensitive to future Tag layout
   growth.
5. **settings.json working-tree residue** — a pre-existing user-indicated
   modification excluded from every milestone commit; untouched by MS-8.
6. **Release + LTO unproven** — only RelWithDebInfo with `ENABLE_LTO=OFF` was
   empirically built; the `/LTCG` Release configuration has not been
   build-verified (carried since MS-7 Batch 2).
7. **P0 branch dead code** — with `REQUIRED_P0_SOURCES = frozenset()`, the
   checker branch `source in REQUIRED_P0_SOURCES and p0_blocking != P0_BLOCKER`
   (`check_module_boundaries.py:241`) is structurally unreachable (MS-6
   review Batch F). The reverse guard that rejects any entry still carrying
   `p0_blocking == P0_BLOCKER` remains active.

## Verification

1. `python scripts/check_module_boundaries.py` — exit 0.
   `[Module Boundary] Observed/ledger edges: 0/0; files: 0` /
   `[Module Boundary] PASS: ledger and observed reverse edges match.`
2. `python -m unittest tests/python/ModuleBoundaryCheckerTest.py
   tests/python/CoreCandidateContractCheckerTest.py` — `Ran 25 tests`, `OK`,
   exit 0.
3. `python scripts/check_legacy_reintroduction.py` — exit 0. Baseline
   total/files `222/71`, current `215/69`, `PASS: no marker/classification
   regression detected`.
4. `cmd.exe /c build.bat check` — exit 0; all pre-checks OK (legacy gate,
   module boundaries, MS-1 Core candidate contract, render ABI, skill_spec,
   assets, modifier v2, monster behavior-op, modifier runtime, skill contract),
   `Check mode: Skipping compilation`.
5. `git diff --check` — exit 0 (LF/CRLF normalization warnings only).
6. `git status --short` — exactly the intended changes:
   `scripts/check_module_boundaries.py`, `tests/python/
   ModuleBoundaryCheckerTest.py`, `settings.json` (pre-existing user residue,
   untouched), and this new `evidence.md`.

## Scope and residual risk

- No product C++ source, CMake target graph, source placement, or ledger
  policy was changed; MS-8 was string/documentation-level plus one empty
  directory removal.
- No commit was created (per instruction; staging/committing is explicitly
  forbidden).
- Residual risk: low. The 7-item technical-debt register above is the
  accepted follow-up list; items 2 and 4 carry behavioral coupling and items
  1/6 need dedicated runtime or build verification.
