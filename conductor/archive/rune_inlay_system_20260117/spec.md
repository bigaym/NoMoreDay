# Spec: Rune Inlay System

## Overview
The Rune Inlay System allows players to enhance equipment by inserting magical Runes into item Sockets. Specific sequences of Runes inserted into the correct item type trigger **Runewords**, transforming the Common (White) base item into a powerful Legendary/Unique item.

## Core Mechanics

### 1. Sockets
- **Capacity**: Items can have up to **3 Sockets**.
- **Visibility**: Sockets are visible on the item icon and tooltip.
- **Rules**:
    - Only specific item types (Weapons, Armor, Shields) can have sockets.
    - Runes can be inserted into any empty socket.
    - Once inserted, a Rune provides its base bonuses (e.g., +Hit, +Resist).

### 2. Runes
- **Type**: `ItemType::Material` (or specialized sub-type logic).
- **Properties**: Each Rune has stats that apply depending on where it's socketed (Weapon vs Armor).
- **Drag & Drop**: Players drag a Rune from the inventory and drop it onto a socketed item.

### 3. Runewords
- **Trigger**: Inserting a specific sequence of Runes (e.g., "Tal" -> "Eth") into an item with the *exact* number of sockets required.
- **Effect**: 
    - The item's Name changes (e.g., "Stealth").
    - The item's Rarity becomes **Legendary** (or specialized Runeword rarity).
    - The item gains a powerful set of Affixes defined by the Runeword.
    - The original Rune bonuses are usually retained (or overridden depending on balance, typically retained).
- **Constraint**: Must be a "White Board" (Common) item initially to form a Runeword (Classic styling, though code might allow others).

## UI/UX

### Inventory
- **Drag Interaction**:
    - Dragging a Rune highlights compatible items with empty sockets.
    - Releasing adds the Rune.
- **Visuals**:
    - **Empty Socket**: Small grey circle overlay on item slot.
    - **Filled Socket**: Small colored/gem circle overlay.
- **Tooltip**: Lists current runes and runeword status.

## Technical Architecture
- **Components**: `ItemComponent` stores `sockets` (vector of entities) and `activeRunewordId`.
- **Systems**: 
    - `CraftingSystem`: Handles the `socketRune` action.
    - `RunewordSystem`: Validates and applies Runeword logic.
    - `UIInventory`: Handles input.
    - `UIRenderer`: Handles drawing.
