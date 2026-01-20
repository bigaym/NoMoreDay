# Optimization Track Audit Report

**Date**: 2026-01-20
**Reviewer**: Antigravity (Auditor Skill)
**Scope**: `conductor/tracks/optimization` (String Refactoring & Dependency Elimination)
**Status**: ⚠️ **Changes Requested**

## 1. Executive Summary
The audit of the `optimization` track implementation confirms that 5 out of 6 targeted systems have been successfully refactored. The introduction of `BiomeID`, `MaterialCategory`, `EnemyRace`, and `FragmentElement` enums has substantially reduced string dependencies in core logic.

However, a **Performance & Logic Consistency** issue was identified in `UIMinimap.cpp`, where legacy string comparison remains in the render loop, violating the track's primary objective.

## 2. Component Review Verification

| Component | Status | Findings |
| :--- | :---: | :--- |
| **RunewordSystem** | ✅ PASS | implemented `kStringToType` map; uses pure ID logic for runeword detection. |
| **EnemySpawnSystem** | ✅ PASS | implemented `kRaceMap`; spawn logic uses `EnemyRace` enum switch. |
| **PortalSystem** | ✅ PASS | Uses `BiomeID` for portal targets and collision checks. |
| **FragmentDropSystem** | ✅ PASS | Uses `FragmentElement` enum for probability rolls and component data. |
| **LootFilter** | ✅ PASS | Uses `kStringToRarity` map; `evaluate` loop relies on Enums. |
| **MaterialRegistry** | ✅ PASS | Parses `category_id` into `MaterialCategory` enum; safe JSON handling. |
| **UIMinimap** | ❌ FAIL | Legacy string comparison found in `Draw` loop. |

## 3. Detailed Findings

### 🔴 Critical Issue: Legacy String Comparison in `UIMinimap.cpp`
**Location**: `src/game/systems/ui/UIMinimap.cpp:197`
```cpp
// Current Code
if (pStats && levelManager.getCurrentBiome() != "town") { ... }
```
**Violation**:
The `string_dependency_elimination` spec explicitly mandated replacing `getCurrentBiome() != "town"` with `getCurrentBiomeID() != BiomeID::Town`. While Line 179 was updated correctly, Line 197 was missed. This leaves a string comparison in the per-frame render path.

**Recommendation**:
Apply the following fix immediately:
```cpp
// Corrected Code
if (pStats && levelManager.getCurrentBiomeID() != NoMoreDay::BiomeID::Town) { ... }
```

### 🟢 Observation: Runeword Optimization
In `RunewordSystem::checkForRuneword`, the system compares `std::vector<std::string>` (Rune Names) to determine matches. While this doesn't strictly violate the "Configuration String Removal" scope, converting Runeword definitions to store `std::vector<uint32_t>` (Rune IDs) would offer further performance benefits in future iterations.

## 4. Action Plan
1.  **Rectify `UIMinimap.cpp`**: Replace the failing line with the Enum-based check.
2.  **Close Track**: Once verified, the optimization track can be marked as "Implementation Complete".
