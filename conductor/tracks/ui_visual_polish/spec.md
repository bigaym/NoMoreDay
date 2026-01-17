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

## 3. Equipment Panel Enhancements
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
