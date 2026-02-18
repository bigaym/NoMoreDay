# Render V3 Clustered Shader Hardening Validation

## 1. Required Commands

1. `build.bat`
2. `build.bat analyze`
3. `build.bat perf`
4. `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## 2. Mandatory Evidence

- [ ] Integration report includes `ClusteredLightingIntegrationTest` pass.
- [ ] No compute shader compile errors for `light_culling.comp`.
- [ ] Fallback diagnostics verified (negative scenario).
- [ ] Perf benchmark emits clustered metric line(s).

## 3. Risk Regression Checklist

- [ ] No binding conflict introduced.
- [ ] No pass-order violation introduced.
- [ ] No crash when clustered path is disabled.

## 4. Final Verdict

- Status: `PENDING`
- Reviewer:
- Date:
