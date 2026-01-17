# UI Visual Polish & Salvage UX Redesign Specification

## 1. Overview
This specification addresses the visual and functional polish of the core UI panels (Character, Inventory, Crafting/Salvage). The goal is to transition from a "functional prototype" look to a "polished game" aesthetic, enhancing immersion and user feedback.

## 2. Visual Aesthetics (Global)
*   **Backgrounds**: Replace flat dark backgrounds with textured panels (e.g., subtle noise, dark metal, or parchment overlay) to add depth.
*   **Borders & Frames**:
    *   **Equipment Slots**: distinct, heavier borders to signify importance.
    *   **Inventory Slots**: standard grid lines but with slightly improved contrast.
*   **Typography**:
    *   **Headers**: Use a Serif font or a more stylized font (if available) for panel titles (e.g., "Equipment", "Forge").
    *   **Body**: Keep Sans-Serif for readability.

## 3. Data Structures
### 3.1 Salvage Filter (POD)
```cpp
struct SalvageFilter {
    uint32_t rarityMask = (1 << (uint32_t)Rarity::Magic) | (1 << (uint32_t)Rarity::Rare);
    uint32_t categoryMask = 0xFFFFFFFF; // All types
    bool keepIfTier6Plus = true;
    bool excludeLocked = true;
};
```

## 4. Equipment Panel Enhancements
*   **Ghost Icons**:
    *   When an equipment slot is empty, display a semi-transparent, darkened SVG silhouette representing the slot type (Helmet, Armor, Boots, Weapon, Ring).
    *   **Implementation**: Use the `rendering-designer` skill to generate simple SVG paths for these icons if assets are not available, or load standard texture assets with low alpha.

## 4. Salvage Panel Redesign (Right Panel)
The current "single slot" interface is insufficient. The new design follows an "Altar" metaphor.

### 4.1 Layout Structure
```text
+--------------------------------------------------+
|  [ FORGE ]   [ MERGE ]   [ >> SALVAGE << ]       |  <-- distinct, high-contrast tabs
+--------------------------------------------------+
|                                                  |
|           [   SOURCE SLOT (ALTAR)   ]            | <--- Central Visual Focus
|                                                  |
|                   ⬇   (Animated Arrow)           |
|                                                  |
|         P R E V I E W    O U T P U T             |
|    [ Icon xN ]   [ Icon xN ]   [ Gold xN ]       |
|                                                  |
+--------------------------------------------------+
|                                                  |
|   [ Quick Salvage: [x] Rare  [ ] Legend ]        |
|                                                  |
|          [ CONFIRM SALVAGE BUTTON ]              |
|                                                  |
+--------------------------------------------------+
```

### 4.2 Key Features
*   **Visual Center**: The decomposition slot should be visually larger or highlighted as an "Altar".
*   **Output Preview**:
    *   Real-time calculation of potential shards/materials.
    *   Displayed clearly *before* the user clicks Salvage.
*   **Tabs**:
    *   "Forge", "Merge", "Salvage" tabs must look like physical tags or runestones, with the active tab clearly illuminated.

## 5. Technical Requirements
*   **Rendering**: Use existing `Raylib` primitives where possible, enhanced with texture overlays.
*   **Assets**:
    *   Need SVG/PNG for Ghost Icons.
    *   Need simple texture for Background Noise.
*   **Code Structure**: Refactor `DrawSalvagePanel` in `UICrafting.cpp` to accommodate the new layout.

## 6. Logic & Interaction Refinement
### 6.1 Panel Alignment during Drag
*   **Problem**: Affix list remains static while the panel moves.
*   **Solution**: All child drawing functions (like `DrawAffixList`) must receive and respect the dynamic `startX` and `startY` offsets from `UISystem::UpdatePanelDrag`.

### 6.2 Tab-Specific Item Handles
*   **Problem**: Item in Forging slot appears in Merging/Salvage slots.
*   **Solution**: Introduce separate entity storage for each context:
    *   `m_forgeTarget` (Forging)
    *   `m_mergeBase`, `m_mergeFodder`, `m_mergeCatalyst` (Merging)
    *   `m_salvageTarget` (Salvaging)
*   Ensure these are correctly cleared or handled when items are consumed/destroyed.

### 6.3 Merging Logic Restrictions
*   **Base Item**: Must be `Rarity::Legendary` AND `legendaryPotential > 0`.
*   **Fodder Item**: Must be `Rarity::Exalted` (T6/T7 affixes).
*   **Guidance**: Slots should display these requirements as hint text when empty.

### 6.4 Deterministic Salvage Preview
*   **Problem**: Yield preview flickers due to real-time `GetRandomValue` calls.
*   **Solution**: Display quantity ranges instead of exact instances.
    *   Formula: Tier 1-3 -> `0 ~ T`. Tier 4-7 -> `(T-3) ~ T`.
    *   Display range text (e.g., "1-4") at the bottom-left of the material icon.

### 6.5 Advanced Salvage Filter Panel
*   Add a collapsible or side panel for batch salvage settings:
    *   **Categories**: All, Weapon, Armor, Jewelry, Shield.
    *   **Rarity**: Normal, Magic, Rare, Exalted (User choice).
    *   **Options**: "Keep item if it has T6+ affix", "Keep if locked".
