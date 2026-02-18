# Render V3 Release Gate Strict Closeout Validation

## 1. Required Commands

1. `build.bat`
2. `build.bat analyze`
3. `build.bat perf`
4. `build.bat gate`
5. `python scripts/v3_release_gate.py --build-dir build --config Release --allow-missing-screenshots --update-baseline --final-verification`

## 2. Mandatory Evidence

- [ ] Gate report artifact attached and schema-valid.
- [ ] Summary line recorded: checks/pass/warning/fail.
- [ ] `clustered_128_improvement_pct` value recorded with run date.
- [ ] Waiver list and bug links updated.
- [ ] Screenshot gate status explicitly documented.

## 3. Governance Checklist

- [ ] No undocumented warning downgrade.
- [ ] All carry-over items have owner + due date.
- [ ] Release recommendation is traceable to evidence.

## 4. Final Verdict

- Status: `PENDING`
- Reviewer:
- Date:
