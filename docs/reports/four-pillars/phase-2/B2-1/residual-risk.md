# B2-1 Residual Risk

## Low-Risk Areas
- Extraction is function-level and behavior-preserving; call-site semantics in `SkillSystem::TryCast` remain unchanged.
- Existing guard behavior is covered by prior `SkillSystem`/guard tests plus new direct service tests.

## Remaining Risks
- The extracted API still uses out-parameters (`std::vector<uint32_t>*`), so null-pointer misuse by future callers is possible if used outside current pattern.
- Logging side effects are preserved in the service; if log routing changes later, assertions that rely on logs (external tooling) may need adjustment.
- Skill-focused unit/integration filters passed, but full non-skill test surfaces were not re-run in this slice.

## Follow-Up Suggestions
- If more extraction follows, consider a value-object return type to reduce pointer-based API footguns.
- Optionally add a tiny null-output defensive check if this service gains additional call sites.
