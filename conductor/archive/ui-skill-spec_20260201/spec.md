# Specification: Skill Specialization System

**Track ID:** ui-skill-spec_20260201
**Type:** Feature
**Created:** 2026-02-01
**Status:** Draft

## Summary
Rebuild the Skill Specialization UI to resemble *Last Epoch*'s "Central Hub + 4 Branches" layout. This includes distinct visual themes per skill and a structured, directional node layout.

## Context
The current skill tree is a generic tree. We need a "Specialization" feel where players commit to specific variations (fire, void, utility, etc.) branching out from the center.

## User Story
As a player, I want to clearly see the distinct upgrade paths for my active skills, so that I can make informed build decisions.

## Acceptance Criteria
- [ ] **Layout**: Nodes are arranged in 4 distinct quadrants/branches radiating from a central hub.
- [ ] **Visuals**: Each skill can have a "Theme" (colors, background tint).
- [ ] **Shapes**: Nodes have distinct shapes (Diamond=Keystone, Square=Modifier, Circle=Passive).
- [ ] **Connections**: "Channel" style connections (groove + energy fill).

## Dependencies
- `UISkillTalentTree` (existing file to be heavily refactored or replaced)

## Out of Scope
- Changing the actual *effects* of the skills (Visuals/Layout only).

## Technical Notes
- Use `UISkillSpecRenderer` to separate rendering from logic.
- Node positions should be calculated relative to the center (0,0).
