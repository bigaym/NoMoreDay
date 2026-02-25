# Combat Anti-Meta Layer — Implementation Plan

> Track ID: `combat_anti_meta_layer_20260225` | Series: CS-M3-01  
> Depends on: CS-M2-02, CS-M2-04, CS-M2-05

## Phase 1: Mutual Exclusion System
- [ ] Define Keystone exclusion groups in config.
- [ ] Implement exclusion enforcement in SkillSystem.
- [ ] UI feedback for excluded nodes.

## Phase 2: Cost Affixes & Diminishing Returns
- [ ] Define cost affix schema.
- [ ] Implement diminishing returns formula in StatsSystem.
- [ ] Integrate with DamagePipeline.

## Phase 3: Testing & Gate
- [ ] Balance regression tests.
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables
- Keystone exclusion system.
- Cost affix + diminishing returns integration.
