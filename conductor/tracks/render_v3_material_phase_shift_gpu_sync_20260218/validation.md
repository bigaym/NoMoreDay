# Render V3 Material Phase Shift GPU Sync Validation

## 1. Required Commands

1. `build.bat`
2. `build.bat analyze`
3. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
4. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## 2. Mandatory Evidence

- [ ] Unit evidence for active-phase payload change.
- [ ] Unit evidence for reset-to-baseline payload change.
- [ ] Integration evidence for phase-shift lifecycle.
- [ ] No schema regression in material/vfx loaders.

## 3. Risk Regression Checklist

- [ ] No new allocations introduced in render hot path.
- [ ] No UB/UAF findings introduced in static analysis.
- [ ] No degradation in non-phase-shift material rendering path.

## 4. Final Verdict

- Status: `PENDING`
- Reviewer:
- Date:
