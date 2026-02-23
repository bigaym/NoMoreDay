# Skill Tree Doc Alignment Spec

> Track ID: `skill_tree_doc_alignment_20260223`  
> Type: bugfix/content-alignment  
> Priority: P0  
> Scope: Align `assets/data/skills.json` specialization trees with `设计文档/职业设计草案_剑修.md`

---

## 1. Problem Statement

Current specialization data is inconsistent with the design draft contract.

Confirmed mismatches:

- Document requires per-skill target node count `24-26`; current JSON contracts are `15-21`.
- Document defines one explicit Trigger route for each skill in section `3.1` to `3.9`; current contracts expose Trigger role only for skill `1`.
- Document target keystone density is `2-3`; current contracts contain `4-8` keystones per skill.
- Section `3.9` design name evolved to `绝影绝剑 (Phantom Trance)` while runtime still uses legacy naming/shape for skill `9`.

This causes design-review drift, balancing mismatch, and incomplete behavior coverage in runtime.

---

## 2. Source of Truth & Boundaries

Primary source of truth for this track:

- `设计文档/职业设计草案_剑修.md` section `3.1` to `3.9` and contract constraints near line `50`.

Runtime artifacts to align:

- `assets/data/skills.json` (`talent_tree` + `skill_contract`)
- Supporting tests that validate contract and behavior coverage.

Out of scope:

- Re-architecting skill runtime systems.
- Full rebalance of damage numbers outside required structural alignment.
- Non-Blade-Ascendant classes.

---

## 3. Alignment Contract

For each skill id `1..9`:

- Node count is aligned to target band `24-26`.
- Exactly one Trigger path is represented in contract nodes (role `Trigger`) and maps to documented trigger behavior.
- Keystone count aligns to design target (`2-3`) unless explicitly documented exception is added.
- At least one Synergy node exists and is contract-tagged.
- Transmuter mutual exclusion remains explicit and runtime-compatible.

Cross-skill consistency:

- Resist model mapping in contract (`TypeA..TypeE`) follows documented per-skill intent.
- `scope_policy`, trigger cooldown, and effectiveness values match documented trigger semantics.

---

## 4. Data Model Changes

No new C++ ECS component is introduced.

Content schema changes are limited to existing JSON schema:

- Update `skills[*].talent_tree` nodes.
- Update `skills[*].skill_contract`:
  - `min_nodes/max_nodes`
  - role mapping (`Keystone/Trigger/Synergy/Transmuter/Passive`)
  - trigger payload (`trigger_skill_id`, `effectiveness`, `internal_cooldown`, `consumes_mana`)
  - resist/scope flags.

---

## 5. Implementation Targets

- `assets/data/skills.json`
- `tests/integration/SkillContractRegistryTests.cpp`
- `tests/integration/SkillSystemTests.cpp` (trigger and specialization branches)
- `tests/unit/SkillBehaviorGuardTests.cpp` (trigger/transmuter/scope checks)
- Any required sync scripts for skill contracts if repository already relies on them.

---

## 6. Verification Strategy

### Structural checks

- Parse and validate updated `skills.json`.
- Per-skill report confirms:
  - node count `24-26`
  - trigger role count `<=1` and expected `=1` for documented skills
  - synergy role count `>=1`
  - keystone count in allowed range or documented override.

### Runtime checks

- Existing contract validation tests pass.
- Trigger dispatch and guard tests pass for all relevant skills.
- Transmuter mutex and scope policy tests remain green.

---

## 7. Acceptance Criteria

- [ ] `skills.json` for skills `1..9` is structurally aligned with design section `3.1..3.9`.
- [ ] Every skill has documented Trigger path represented in contract role mapping.
- [ ] Node count and role distribution satisfy design constraints (or explicit documented exceptions).
- [ ] Contract/behavior tests are updated and pass.
- [ ] No regression in existing specialization runtime safety tests.

---

## 8. Risks & Mitigations

- Risk: Massive data churn increases behavior regressions.
  - Mitigation: enforce per-skill phased updates and run targeted tests after each skill batch.
- Risk: Design draft may contain unresolved/placeholder entries.
  - Mitigation: keep an explicit exception table inside track validation notes; do not silently diverge.
- Risk: Name/description encoding drift in data pipeline.
  - Mitigation: validate UTF-8 parsing and keep canonical text pipeline documented in validation evidence.

