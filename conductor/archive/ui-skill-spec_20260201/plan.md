# Implementation Plan: Skill Specialization

**Track ID:** ui-skill-spec_20260201
**Spec:** [spec.md](./spec.md)
**Created:** 2026-02-01
**Status:** [ ] Not Started

## Overview
Refactor `UISkillTalentTree` to support radial layouts and thematic rendering.

## Phase 1: Layout & Data
Update the node data to support "Branch ID" and "Depth".

### Tasks
- [ ] Task 1.1: Add `branch_id` (0-4) and `depth` (int) to `TalentNode`.
- [ ] Task 1.2: Implement `RadialLayoutSystem` to calculate `(x, y)` based on branch/depth.
- [ ] Task 1.3: Update existing skill JSONs/Data to assign branches to nodes.

### Verification
- [ ] Nodes appear in a cross/star shape instead of a random list.

## Phase 2: Thematic Rendering
Implement the "Last Epoch" style visuals.

### Tasks
- [ ] Task 2.1: Implement `DrawNodeShape` (Diamond, Square, Circle) with metal borders.
- [ ] Task 2.2: Implement "Channel" connection rendering (Groove + Fill).
- [ ] Task 2.3: Add `SkillTheme` struct (colors for background, energy, borders).

### Verification
- [ ] UI looks like the reference image (Dark void + Gold/Blue accents).

## Final Verification
- [ ] All acceptance criteria met.
