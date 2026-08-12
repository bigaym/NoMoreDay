#pragma once

#include "game/application/ui/UiRuntimeTypes.hpp"

namespace NoMoreDay::ui {

// Shared UI resource ids (R4, remediation design §3.4): the host registers the
// real raylib resources with the backend under these ids at Initialize time,
// and the controllers reference only the ids in draw-list commands — raylib
// types never cross into the controller/Draw layer. The backend resolves the
// ids during the single-pass submit.
inline constexpr UiResourceId kGlobalFontResourceId = 1;
inline constexpr UiResourceId kMessageBoxTextureResourceId = 2;
// R5 (remediation design §3.4): HUD icon textures registered by the host.
inline constexpr UiResourceId kSwordIntentIconResourceId = 3;
inline constexpr UiResourceId kMinimapTextureResourceId = 4;
// R6: panel surface textures (Button_Frost_Rect / Button_Frost_Square) used
// by the character/inventory/overlay paint paths; registered by the host.
inline constexpr UiResourceId kPanelRectTextureResourceId = 5;
inline constexpr UiResourceId kPanelSquareTextureResourceId = 6;
// R6: the player avatar texture. The raw raylib GL id cannot cross the
// draw-list boundary, so the host syncs the current SpriteComponent texture
// under this fixed id (same pattern as the minimap texture sync).
inline constexpr UiResourceId kPlayerAvatarTextureResourceId = 7;
// R8: custom painters (design §3.4) registered by the host at Initialize time.
// The skill hub / talent tree / astrolabe render their dense canvases through
// these painter ids (raylib confined to the backend painter contract); the
// tooltip top-most overlay paints through its own painter id.
inline constexpr UiResourceId kTooltipPainterResourceId = 8;
inline constexpr UiResourceId kSkillHubPainterResourceId = 9;
inline constexpr UiResourceId kSkillTreePainterResourceId = 10;
inline constexpr UiResourceId kAstrolabePainterResourceId = 11;

} // namespace NoMoreDay::ui
