# V5 Release Posture - v5_validation_release_gate_20260219

## Decision

- `GO` for V5 core GI release (as of 2026-02-21).
- SPH branch decision is `NO-GO` and remains optional/non-blocking.

## Scope Of This Decision

- Included in release scope:
  - JFA distance field
  - Radiance Cascades GI (tiered, with temporal/stability/contract coverage)
  - V5 rollback and compatibility path to V4
- Excluded from release scope:
  - SPH fluid simulation (exploration target not met)

## Evidence Snapshot

- Build + test pipeline:
  - `.\build.bat`: PASS
  - `ctest -C RelWithDebInfo -L ci`: PASS
  - `ctest -C RelWithDebInfo -L unit`: PASS
  - `ctest -C RelWithDebInfo -L integration`: PASS
  - `ctest -C Release -L performance`: PASS
- Core release metrics:
  - `combat_180_fps=190455`
  - `baseline_270_fps=941.072`
  - `gi_ultra_standard_mean_ms=1.88112`
  - `vram_proxy_delta_bytes=0`
- Optional SPH input:
  - `fluid_reference_10k_target_hit=0` (`mean=0.99006ms`, target `<=0.80ms`) → `NO-GO`

## Remaining Items

1. None for this track.
