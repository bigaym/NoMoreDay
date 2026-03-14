# Mastery Skill Quantification Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Quantify qualitative descriptions for Mastery Skills 10, 11, and 12 in `assets/data/mastery_skill_trees.json`.

**Architecture:** Update `desc_key` for tooltips and `stat_modifiers`/`damage_modifiers` for actual game logic using a Linear Growth strategy. Values in `stat_modifiers` should be consistent with `StatModifier` JSON structure (type, mode, value).

**Tech Stack:** JSON, C++ (for validation/build).

---

### Task 1: Quantify Skill 10 (Sword Saint)

**Files:**
- Modify: `assets/data/mastery_skill_trees.json`

**Step 1: Identify and Update Qualitative Nodes**
For all nodes in skill tree `10` (Sword Saint):
- Update `desc_key` to include quantified values in Chinese (e.g., `"攻击速度增加 5%/10%/15%/20%"`).
- Update `stat_modifiers` or `damage_modifiers` to match the description.
- **Important:** `stat_modifiers` is a list of objects following the `StatModifier` schema.
- Example for Node 1001 (Attack Speed):
  - `desc_key`: "攻击速度增加 5%/10%/15%/20%"
  - `stat_modifiers`: `[{"type": 19, "mode": 1, "value": 5.0}]` (StatType::AttackSpeed = 19, ModifierMode::PercentAdd = 1).
- Example for Node 1007 (More Damage):
  - `damage_modifiers`: `[{"source_tag": 0, "target_tag": 0, "value": 20.0, "type": 2}]` (ModifierType::More = 2).


**Step 2: Commit changes**
```bash
git add assets/data/mastery_skill_trees.json
git commit -m "feat(mastery): quantify Skill 10 (Sword Saint) descriptions and modifiers"
```

---

### Task 2: Quantify Skill 11 (Heavenly Sword)

**Files:**
- Modify: `assets/data/mastery_skill_trees.json`

**Step 1: Identify and Update Qualitative Nodes**
For all nodes in skill tree `11`:
- Update `desc_key` with linear values.
- Update `stat_modifiers`.
- Special: Ensure "cooldown_speed" is used instead of "cooldown_reduction".

**Step 2: Commit changes**
```bash
git add assets/data/mastery_skill_trees.json
git commit -m "feat(mastery): quantify Skill 11 (Heavenly Sword) with linear growth"
```

---

### Task 3: Quantify Skill 12 (Demon Blade)

**Files:**
- Modify: `assets/data/mastery_skill_trees.json`

**Step 1: Identify and Update Qualitative Nodes**
For all nodes in skill tree `12`:
- Update `desc_key` and `stat_modifiers`.
- Ensure `display_lines` in the preview metadata (if any) are consistent.

**Step 2: Commit changes**
```bash
git add assets/data/mastery_skill_trees.json
git commit -m "feat(mastery): quantify Skill 12 (Demon Blade) and sync preview data"
```

---

### Task 4: Final Verification

**Step 1: Run Schema Check / Build**
Run: `./build.bat check`
Expected: Build success, no JSON parsing errors.

**Step 2: Final Commit**
```bash
git commit --allow-empty -m "chore(mastery): quantification of Skills 10-12 verified"
```
