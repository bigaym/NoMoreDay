# Spec: Equipment Tooltip Upgrade

## Core Concept
Improve the visual clarity and information density of the equipment tooltip in the inventory system by adding a prominent item icon and explicit socket count information.

## User Stories
- **Icon Visibility**: Players can quickly identify items by seeing a larger version of the item icon within the tooltip.
- **Socket Awareness**: Players can easily see the total number of sockets (empty or filled) at the bottom of the tooltip, helping them plan for rune inlaying.

## Design Details

### 1. Visual Layout
- **Title Area**: Keep the current item name at the top.
- **Icon Section**:
    - Position: Below the title and category, or to the left of the main stats.
    - **Decision**: To keep the tooltip narrow and readable, we will place a **48x48** or **64x64** icon to the **left** of the primary stats (Attack, Defense, etc.), with the text wrapping or indented.
    - Alternatively, if it's "bigger", we can place it below the name header and above the stats. Let's go with **64x64** centered below the title.
- **Socket Info**:
    - Location: Near the bottom, after affixes but before the long description.
    - Color: `#D0EFE8` (RGB: 208, 239, 232).
    - Text: `插槽数量: X` (Chinese) / `Sockets: X` (English).

### 2. Implementation Strategy
- **Constant**: Add `UI_SOCKET_INFO` to `src/engine/render/GPUData.hpp`.
- **UIRenderer Changes**:
    - Modify `UIRenderer::DrawTooltip`.
    - Increase the calculated height `h` to accommodate the icon.
    - Draw the texture using `DrawTexturePro` with high-quality scaling.
    - Append the socket count line to the `lines` vector in the tooltip generation logic.

## Marginal Cases
- **No Texture**: If an item has no `textureId`, the tooltip should gracefully fallback to the current layout without a massive empty space.
- **Large Descriptions**: Ensure the tooltip doesn't exceed screen height; scroll or truncation might be needed (though not requested, it's a risk).

## Performance
- No significant performance impact as tooltips are only drawn for one item at a time (on hover).
