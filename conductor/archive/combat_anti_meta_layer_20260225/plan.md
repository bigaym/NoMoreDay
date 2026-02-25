# Combat Anti-Meta Layer — Implementation Plan

> Track ID: `combat_anti_meta_layer_20260225` | Series: CS-M3-01  
> Depends on: CS-M2-02, CS-M2-04, CS-M2-05

## Phase 1: Mutual Exclusion System
- [x] Define Keystone exclusion groups in config.
- [x] Implement exclusion enforcement in SkillSystem.
- [x] UI feedback for excluded nodes.

## Phase 2: Cost Affixes & Diminishing Returns
- [x] Define cost affix schema.
- [x] Implement diminishing returns formula in StatsSystem.
- [x] Integrate with DamagePipeline.

## Phase 3: Testing & Gate
- [x] Balance regression tests.
- [x] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

## Deliverables
- Keystone exclusion system.
- Cost affix + diminishing returns integration.
