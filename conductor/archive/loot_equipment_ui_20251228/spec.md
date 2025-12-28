# UI Inventory and Equipment Slot System

## 1. Overview
This feature introduces a comprehensive User Interface (UI) system for managing player inventory and equipment slots. The system will support intuitive interactions for item management, differentiate storage for materials versus other items, and incorporate dynamic display logic for item names. The UI will be designed to be flexible, accommodating various text lengths, particularly for Chinese localized content.

## 2. Functional Requirements

### 2.1 Inventory Management
- **Drag and Drop:** Users shall be able to drag and drop items between inventory slots and equipment slots.
- **Click to Equip/Unequip:** Users shall be able to click on an item to equip it (if applicable) or unequip it, moving it to/from an available inventory slot.
- **Contextual Actions:** Right-clicking on an item in the inventory or equipment slot shall display a contextual menu with options such as "Discard," "Use" (if consumable), and "Equip"/"Unequip."
- **Inventory Grid:** A grid-based inventory system for equipment and other consumable items. These items will be limited by the number of available inventory slots.
- **Material Storage:** A separate UI tab will be dedicated to materials, which will have unlimited storage capacity and not be constrained by inventory slots.

### 2.2 Equipment Slots
- **Layout:** Eleven dedicated equipment slots will be displayed on the left side of the UI, arranged from top to bottom (e.g., Helmet, Shoulders, Chest, Gloves, Legs, Boots, Weapon (Main Hand), Weapon (Off Hand), Ring 1, Ring 2, Amulet).
- **Equipping Items:** Only items appropriate for a specific slot can be placed into that slot.
- **Visual Feedback:** Clear visual feedback will be provided when an item is successfully equipped or unequipped.

### 2.3 Item Information Display
- **Tooltips:** Hovering the mouse over an item (whether in inventory, equipment slot, or on the ground) shall display a detailed tooltip showing item information (stats, description, etc.).
- **On-Ground Display:** Items dropped on the ground will display their full name, with the text color bound to the item's rarity as defined in the design document (e.g., "残暴的鲨鱼刀" displayed in a color corresponding to its rarity).
- **In-Slot Display:** Items within inventory or equipment slots will display a shortened representation of their item type, with the text color bound to the item's rarity (e.g., "剑" displayed in a color corresponding to its rarity) due to space constraints.

## 3. Non-Functional Requirements

### 3.1 Performance
- **Zero-Allocation UI Updates:** UI updates, especially for item changes, should strive for zero runtime memory allocations to maintain high performance.
- **Efficient Rendering:** The UI system must be optimized for efficient rendering, minimizing drawing calls and CPU overhead.

### 3.2 Localization Support
- **Dynamic Text Scaling:** The UI system must support dynamic text scaling and layout adjustments to correctly display item names and descriptions in Chinese, accounting for potentially larger texture sizes or glyph counts compared to English.

### 3.3 Technical Integration
- **AI Workflow Integration:** The UI will leverage an AI workflow for generating texture and font assets.
- **Asset Loading System:** A dedicated `AssetLoadingSystem` will be responsible for loading UI assets (textures, fonts) using references provided in a central header file.
- **Header File for UI Assets:** A specific header file will contain references (e.g., file paths, asset IDs) to the AI-generated texture/font assets for dynamic loading.

## 4. Acceptance Criteria
- [ ] Users can open the inventory/equipment UI and interact with it.
- [ ] Items can be successfully moved between inventory and equipment slots via drag-and-drop.
- [ ] Items can be equipped/unequipped via clicking.
- [ ] Right-click contextual menus for items function correctly (Discard, Use, Equip/Unequip).
- [ ] Materials are stored in a separate, unlimited tab.
- [ ] Equipment and other items respect inventory slot limits.
- [ ] Item names on the ground display full name with rarity-bound color.
- [ ] Item names in inventory/equipment slots display shortened type with rarity-bound color.
- [ ] Tooltips display comprehensive item information on hover.
- [ ] UI layout adapts to Chinese text without visual issues.
- [ ] UI assets are loaded dynamically via the `AssetLoadingSystem` using the header file references.

## 5. Out of Scope
- Specific item stat calculations or attribute modifications resulting from equipping items.
- Full visual design and implementation of every UI element (initial focus on functionality and layout).
- Backend persistence of inventory and equipment state (assumed to be handled by another system).
- AI generation of the UI assets themselves (this specification focuses on integration).
