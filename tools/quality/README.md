# Quality Baseline Tools

## Systems Baseline Metrics

`tools/quality/systems_baseline.py` provides a lightweight architecture hotspot snapshot for `src/game/systems`.

It reports:

- per-subsystem `.cpp` file counts
- top N largest `.cpp` files by LOC (non-empty lines)
- cross-subsystem include edges from `#include "game/systems/..."` found in `.cpp` files

### Usage

From repository root:

```powershell
python tools/quality/systems_baseline.py
```

Write JSON output:

```powershell
python tools/quality/systems_baseline.py --top 15 --json-out docs/reports/four-pillars/phase-0/P0-2/systems-baseline.json
```
