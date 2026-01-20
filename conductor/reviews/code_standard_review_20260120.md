# Result of Code Standard Review

## 1. Executive Summary
A structural audit targeting "String-Free Core Logic" (Section 7.2 of `code_standard.md`) has identified **one critical performance violation** and several architectural deviations. The codebase currently performs string comparisons in hot paths (Update loops) and relies heavily on string literals for logic branching, violating the project's Data-Oriented Design (DOD) principles.

## 2. Violation Detail

### 2.1. Critical Performance Violations (Must Fix)
**Location:** `src/game/systems/world/PortalSystem.cpp` : `UpdatePortalCollision` (Line 72)
```cpp
if (portalComp.targetBiome == "town") { ... }
```
*   **Issue:** String comparison executed every frame for every player near a portal.
*   **Violation:** Section 7.2 - "Strictly forbid using string comparisons ... in hot paths".
*   **Risk:** Unnecessary CPU cycles and potential cache thrashing if `targetBiome` is long (though likely SSO).
*   **Fix:** Use `BiomeID` enum or pre-hashed `uint32_t`.

**Location:** `src/game/systems/ui/UIInventory.cpp` : `Draw` (Lines 537, 549)
```cpp
bool isSelected = (m_selectedCategory == cat) || (std::string(cat) == "All" && ...);
```
*   **Issue:** `std::string(cat)` constructs a temporary string object **every frame** inside the UI draw loop.
*   **Violation:** Section 2.1 "Avoid heap allocations in hot paths" & Section 7.2.
*   **Risk:** Frequent heap allocations (if string is long) or at least copy overhead per frame.
*   **Fix:** Use `strcmp`, `std::string_view`, or (best) `MaterialCategory` enum.

### 2.2. Architectural Deviations (Priority Fix)
**Location:** `src/game/systems/skill/AstrolabeSystem.cpp` : `handle_node_effect`
```cpp
if (effect.value == "SwordHeart") { ... }
else if (effect.value == "SwordIntentUnlock") { ... }
```
*   **Issue:** Business logic (`GrantComponent`) depends on exact string matching.
*   **Violation:** Section 7.2 - "Enums over Strings: Use enum class... for logic branching".
*   **Risk:** Typos in JSON configuration (`SwordHeaart`) will silently fail without compiler warnings.
*   **Fix:** Parse strings to `EffectType` enum or `TraitID` at load time (Registry Pattern).

**Location:** `src/game/systems/world/EnemySpawnSystem.cpp` : `initData`
```cpp
if (raceName == "undead" || raceName == "skeleton") ...
else if (raceName == "demon") ...
```
*   **Issue:** Hardcoded string-to-enum mapping using `else if` chains.
*   **Violation:** Section 7.2 - "Registry Pattern".
*   **Fix:** Use a `static const std::unordered_map<std::string, EnemyRace>` loop-up.

## 3. Recommendations
1.  **Immediate**: Refactor `PortalSystem` to use `BiomeID` enum instead of raw strings for town checks.
2.  **Immediate**: Fix `UIInventory` to use `std::string_view` or C-string comparison to eliminate allocations.
3.  **Short-term**: Introduce `AstrolabeEffectRegistry` to map JSON strings to Enums/IDs at startup, keeping the runtime logic strictly numeric.
4.  **Short-term**: Replace `EnemySpawnSystem` if-else chain with a `RaceRegistry`.

## 4. Conclusion
The project generally follows the structure but slips into "stringly typed" programming in logic layers. STRICT enforcement of Section 7.2 is required to maintain the "High Performance" identity of NoMoreDay.
