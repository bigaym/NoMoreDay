# Combat Release Manual Checklist

- [x] Confirm gate report reviewed by owner (`bin/combat_gate/combat_gate_report_release.json`).
- [x] Confirm CI / Nightly / Release mode commands are documented and reproducible.
- [x] Confirm p95/p99 combat frame metrics are below configured thresholds.
- [x] Confirm regression coverage is at least 80%.
- [x] Confirm major regression reduction is at least 50% vs M1 baseline.
- [x] Confirm `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS evidence is recorded.
