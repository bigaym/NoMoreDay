# Design: Populate Blood Sea Preview Data

## Goal
Populate quantitative tooltip metadata (`display_lines`) for key nodes in the Blood Sea mastery tree to support the specialization tooltip preview feature.

## Architecture
- **Data Source**: `assets/data/skills.json` and `assets/data/mastery_skill_trees.json`.
- **Metadata Format**: `TalentDisplayLine` objects containing label, per-point value, and percentage flag.
- **Verification**: Unit tests in `SkillRegistryMasteryTreeTests.cpp` ensuring data is correctly loaded into the `SkillRegistry`.

## Implementation Details
- **Skill 12 (Blood Sea)**: Verify `field_duration` exists in `params`.
- **Mastery Nodes**:
  - `1200`: Add range scaling.
  - `1216`: Add pulse frequency scaling.
  - `1219`: Add duration scaling.
  - `1221`: Add move speed and range scaling.
  - `1222`: Add leech ratio and range scaling.
  - `1223`: Add damage scaling.

## Testing
- Add `[Unit] Skill Registry - Blood Sea specialization preview data is present` to `tests/unit/SkillRegistryMasteryTreeTests.cpp`.
- Run using `./bin/NoMoreDayTests.exe`.
