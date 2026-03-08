# Mastery Node Icon Hash Sync Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Align mastery specialization node `icon_id` values with the existing `skill_node_<node_id>` FNV1a hash convention used by regular skill trees.

**Architecture:** Keep runtime code unchanged and fix the problem in data/tooling. Extend `scripts/sync_skill_node_icon_ids.py` so it can sync both `assets/data/skills.json` and `assets/data/mastery_skill_trees.json`, then run it to rewrite mastery node icon ids to hashed resource ids.

**Tech Stack:** Python 3, `unittest`, JSON asset files, existing FNV1a hashing convention.

---

### Task 1: Add coverage for multi-file JSON syncing

**Files:**
- Create: `tests/python/SyncSkillNodeIconIdsTest.py`
- Modify: `scripts/sync_skill_node_icon_ids.py`

**Step 1: Write the failing test**

Add a Python `unittest` that imports `sync_skill_node_icon_ids` and verifies a helper can sync both a regular `skills.json`-shaped document and a `mastery_skill_trees.json`-shaped document in one call.

**Step 2: Run test to verify it fails**

Run: `python -m unittest tests.python.SyncSkillNodeIconIdsTest -v`
Expected: FAIL because the multi-file sync helper does not exist yet.

**Step 3: Write minimal implementation**

Add a helper that accepts multiple JSON paths, reuses the existing node traversal, and returns non-zero in `--check` mode if any file needs changes.

**Step 4: Run test to verify it passes**

Run: `python -m unittest tests.python.SyncSkillNodeIconIdsTest -v`
Expected: PASS.

### Task 2: Extend CLI defaults to include mastery trees

**Files:**
- Modify: `scripts/sync_skill_node_icon_ids.py`
- Test: `tests/python/SyncSkillNodeIconIdsTest.py`

**Step 1: Write the failing test**

Add an assertion that the default JSON path list includes both `assets/data/skills.json` and `assets/data/mastery_skill_trees.json`.

**Step 2: Run test to verify it fails**

Run: `python -m unittest tests.python.SyncSkillNodeIconIdsTest -v`
Expected: FAIL because only `skills.json` is currently wired as default input.

**Step 3: Write minimal implementation**

Introduce a default JSON path tuple/list and wire `main()` to sync all default files unless the caller passes explicit paths.

**Step 4: Run test to verify it passes**

Run: `python -m unittest tests.python.SyncSkillNodeIconIdsTest -v`
Expected: PASS.

### Task 3: Rewrite committed mastery icon ids and verify

**Files:**
- Modify: `assets/data/mastery_skill_trees.json`

**Step 1: Run check mode before applying**

Run: `python scripts/sync_skill_node_icon_ids.py --check`
Expected: exit code `1` because mastery node `icon_id` values still use raw node ids.

**Step 2: Apply the sync**

Run: `python scripts/sync_skill_node_icon_ids.py`
Expected: script rewrites mismatched node `icon_id` values.

**Step 3: Re-run verification**

Run: `python -m unittest tests.python.SyncSkillNodeIconIdsTest -v && python scripts/sync_skill_node_icon_ids.py --check`
Expected: tests PASS and check mode exits `0`.
