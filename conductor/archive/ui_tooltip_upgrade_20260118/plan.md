# Plan: Equipment Tooltip Upgrade

## Tasks

### Phase 1: Constants & Preparation
- [x] Add `#D0EFE8` color to `src/engine/render/GPUData.hpp` as `COLOR_SOCKET_INFO`.
- [x] Identify the exact insertion point in `UIRenderer::DrawTooltip` for the icon and socket info.

### Phase 2: Tooltip Logic Refinement
- [x] **Icon Integration**:
    - Update height calculation to include space for a 64x64 icon.
    - Implement `DrawTexturePro` call for the item icon below the title.
    - Handle padding and alignment.
- [x] **Socket Info Integration**:
    - Add logic to push a new line to the `lines` vector if `itemComp->socketCount > 0`.
    - Use `COLOR_SOCKET_INFO` color for this line.
    - Ensure it is placed towards the bottom (e.g., after legendary potential but before description).

### Phase 3: Verification & Polish
- [x] Test with various item types (Weapon, Armor, Material).
- [x] Verify alignment on different screen resolutions (using `s_uiScale`).
- [x] Confirm the color matches the design specification.
