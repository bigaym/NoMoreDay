# Sword Cultivator Mastery Skill Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Audit and implement Sword Cultivator (Blade Ascendant) mastery skill nodes (Skill 10, 11, 12) to match quantified data in `assets/data/mastery_skill_trees.json`.

**Architecture:** Data-driven logic sync. Runtime behavior in C++ will be updated to match JSON constants. Logic for missing nodes (e.g., node 1211) will be added. Integration tests will ensure compliance.

**Tech Stack:** C++20, JSON, doctest.

---

### Task 1: Audit and Sync Skill 11 (Heavenly Sword Descent)

**Files:**
- Modify: `src/game/skills/HeavenlySwordDescent.cpp`
- Data: `assets/data/mastery_skill_trees.json`

**Step 1: Update Echo Damage (Node 1111)**
- JSON: `echo_damage: 0.1` (10%)
- Code current: `0.25f`
- Change to: `0.10f`

**Step 2: Update Critical Chance (Node 1121)**
- JSON: `crit_chance_inc: 0.15` (15%)
- Code current: `0.10f`
- Change to: `0.15f`

**Step 3: Verify build**
Run: `./build.bat check`
Expected: PASS

**Step 4: Commit**
```bash
git add src/game/skills/HeavenlySwordDescent.cpp
git commit -m "fix(mastery): sync Skill 11 constants with mastery data"
```

### Task 2: Implement Missing Logic for Skill 12 (Blood Sea - Node 1211)

**Files:**
- Modify: `src/game/skills/BloodSea.cpp`
- Test: `tests/game/skills/BladeMasteryTests.cpp`

**Step 1: Write failing test for FreshBloodReturn (Node 1211)**
```cpp
TEST_CASE("[Unit] BloodSea - Node 1211 FreshBloodReturn") {
    // Setup player with 50% HP
    // Trigger BloodSea burst
    // CHECK(player.hp == 60%); // 10% missing hp heal
    // CHECK(player.thirst_stacks == 2);
}
```

**Step 2: Run test to verify it fails**
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] BloodSea - Node 1211 FreshBloodReturn"`
Expected: FAIL (No heal/stacks applied)

**Step 3: Implement healing and stack gain in BloodSea.cpp**
Add logic to `BloodSea::onBurst()` to check for node 1211 and apply:
- `heal(0.1f * (max_hp - current_hp))`
- `addThirstStacks(2)`

**Step 4: Run test to verify it passes**
Run: `./bin/NoMoreDayTests.exe --test-case="[Unit] BloodSea - Node 1211 FreshBloodReturn"`
Expected: PASS

**Step 5: Commit**
```bash
git add src/game/skills/BloodSea.cpp tests/game/skills/BladeMasteryTests.cpp
git commit -m "feat(mastery): implement node 1211 FreshBloodReturn for Skill 12"
```

### Task 3: Data-Driven Refactor for Skill 10 (Seven Star Slash)

**Files:**
- Modify: `src/game/skills/SevenStarSlash.cpp`

**Step 1: Replace hardcoded node 1011 range bonus with data lookup**
- Current: `if (hasNode(1011)) range *= 1.2f;`
- New: `range *= getModifier(1011, "range_mult", 1.0f);`

**Step 2: Run build and verify**
Run: `./build.bat check`
Expected: PASS

**Step 3: Commit**
```bash
git add src/game/skills/SevenStarSlash.cpp
git commit -m "refactor(mastery): use data-driven modifiers for Skill 10"
```
