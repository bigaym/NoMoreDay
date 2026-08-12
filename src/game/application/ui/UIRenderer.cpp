#include "game/application/ui/UIRenderer.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/foundation/data/BuffRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/SharedContext.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/systems/skill/SkillDisplayPreviewService.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"

#include <algorithm>



#include <cmath>
#include <cstdio>
#include <string>
#include <sstream>


// --- New Tooltip Constants ---
static constexpr float TT_PADDING = 16.0f;
static constexpr float TT_HEADER_HEIGHT = 64.0f;
static constexpr float TT_ICON_SIZE = 48.0f;
static constexpr float TT_MAX_WIDTH = 360.0f;
static constexpr float TT_SECTION_SPACING = 12.0f;
static constexpr Color TT_COLOR_BG = {20, 20, 25, 245};
static constexpr Color TT_COLOR_BORDER = {70, 70, 85, 255};
static constexpr Color TT_COLOR_HEADER = {255, 215, 100, 255}; // Gold
static constexpr Color TT_COLOR_STAT_LABEL = {160, 160, 175, 255};
static constexpr Color TT_COLOR_TAG_BG = {45, 45, 55, 255};
static constexpr Color TT_COLOR_TAG_BORDER = {80, 80, 95, 255};

namespace NoMoreDay {

// R8: the snapshot tooltip render path consumes the bounded display views from
// NoMoreDay::ui; bring the types in for the definitions below.
using ui::GameUiAffixView;
using ui::GameUiBuffView;
using ui::GameUiItemView;
using ui::GameUiSkillBarSlotView;
using ui::GameUiSnapshot;

namespace {

bool FontHasCodepointExact(const Font &font, int codepoint) {
  if (!IsFontValid(font) || codepoint < 0 || font.glyphCount <= 0 ||
      font.glyphs == nullptr) {
    return false;
  }

  const int glyphIndex = GetGlyphIndex(font, codepoint);
  if (glyphIndex < 0 || glyphIndex >= font.glyphCount) {
    return false;
  }
  return font.glyphs[glyphIndex].value == codepoint;
}

float GetCodepointAdvance(const Font &font, int codepoint, float scaledSize,
                          float spacing) {
  if (!IsFontValid(font) || font.baseSize <= 0 || font.glyphCount <= 0 ||
      font.glyphs == nullptr || font.recs == nullptr) {
    return spacing;
  }

  const int glyphIndex = GetGlyphIndex(font, codepoint);
  if (glyphIndex < 0 || glyphIndex >= font.glyphCount) {
    return spacing;
  }

  float advance = (float)font.glyphs[glyphIndex].advanceX;
  if (advance <= 0.0f) {
    advance = font.recs[glyphIndex].width + font.glyphs[glyphIndex].offsetX;
  }

  const float sizeScale = scaledSize / (float)font.baseSize;
  return advance * sizeScale + spacing;
}

bool NeedsEmojiFallback(const Font &primary, const Font &emojiFallback,
                        const char *text) {
  if (!text || text[0] == '\0' || !IsFontValid(primary) ||
      !IsFontValid(emojiFallback)) {
    return false;
  }

  // FAST PATH: Check if string is ASCII-only. ASCII characters (0-127)
  // are almost certainly handled by the primary font.
  bool hasNonAscii = false;
  const char *testPtr = text;
  while (*testPtr != '\0') {
    if (static_cast<unsigned char>(*testPtr) > 127) {
      hasNonAscii = true;
      break;
    }
    testPtr++;
  }
  if (!hasNonAscii) {
    return false;
  }

  const char *ptr = text;
  while (*ptr != '\0') {
    int bytesProcessed = 0;
    const int codepoint = GetCodepointNext(ptr, &bytesProcessed);
    if (bytesProcessed <= 0) {
      bytesProcessed = 1;
    }
    ptr += bytesProcessed;

    if (codepoint == '\n') {
      continue;
    }

    if (!FontHasCodepointExact(primary, codepoint) &&
        FontHasCodepointExact(emojiFallback, codepoint)) {
      return true;
    }
  }
  return false;
}

Vector2 MeasureTextUI(const Font &primary, const Font &emojiFallback,
                      const char *text, float scaledSize, float spacing) {
  if (!text || text[0] == '\0') {
    return {0.0f, 0.0f};
  }

  if (!IsFontValid(primary) || !NeedsEmojiFallback(primary, emojiFallback, text)) {
    if (IsFontValid(primary)) {
      return MeasureTextEx(primary, text, scaledSize, spacing);
    }
    return {(float)MeasureText(text, (int)scaledSize), scaledSize};
  }

  float lineWidth = 0.0f;
  float maxWidth = 0.0f;
  int lineCount = 1;

  const char *ptr = text;
  while (*ptr != '\0') {
    int bytesProcessed = 0;
    const int codepoint = GetCodepointNext(ptr, &bytesProcessed);
    if (bytesProcessed <= 0) {
      bytesProcessed = 1;
    }
    ptr += bytesProcessed;

    if (codepoint == '\n') {
      maxWidth = std::max(maxWidth, lineWidth);
      lineWidth = 0.0f;
      ++lineCount;
      continue;
    }

    const bool useEmojiFallback = !FontHasCodepointExact(primary, codepoint) &&
                                  FontHasCodepointExact(emojiFallback, codepoint);
    const Font &activeFont = useEmojiFallback ? emojiFallback : primary;
    lineWidth += GetCodepointAdvance(activeFont, codepoint, scaledSize, spacing);
  }

  maxWidth = std::max(maxWidth, lineWidth);
  const float lineStep = scaledSize + spacing * 4.0f;
  return {maxWidth, scaledSize + (float)(lineCount - 1) * lineStep};
}

void DrawTextWithEmojiFallback(const Font &primary, const Font &emojiFallback,
                               const char *text, Vector2 pos,
                               float scaledSize, float spacing, Color color) {
  float cursorX = pos.x;
  float cursorY = pos.y;
  const float lineStep = scaledSize + spacing * 4.0f;

  const char *ptr = text;
  while (*ptr != '\0') {
    int bytesProcessed = 0;
    const int codepoint = GetCodepointNext(ptr, &bytesProcessed);
    if (bytesProcessed <= 0) {
      bytesProcessed = 1;
    }
    ptr += bytesProcessed;

    if (codepoint == '\n') {
      cursorX = pos.x;
      cursorY += lineStep;
      continue;
    }

    const bool useEmojiFallback = !FontHasCodepointExact(primary, codepoint) &&
                                  FontHasCodepointExact(emojiFallback, codepoint);
    const Font &activeFont = useEmojiFallback ? emojiFallback : primary;
    DrawTextCodepoint(activeFont, codepoint, {cursorX, cursorY}, scaledSize,
                      color);
    cursorX += GetCodepointAdvance(activeFont, codepoint, scaledSize, spacing);
  }
}

const char *ResolveDamageLabel(SkillDisplayDamageMode mode) {
  switch (mode) {
  case SkillDisplayDamageMode::Hit:
    return "预估伤害";
  case SkillDisplayDamageMode::PerSecond:
    return "每秒伤害";
  case SkillDisplayDamageMode::Total:
    return "总伤害";
  case SkillDisplayDamageMode::ChannelWindow:
    return "引导伤害(1秒)";
  }
  return "预估伤害";
}

} // namespace


void UIRenderer::SetTheme(const UITheme &theme) { s_theme = theme; }

UITheme &UIRenderer::GetTheme() { return s_theme; }

void UIRenderer::SetScale(float scale) { s_uiScale = scale; }

float UIRenderer::GetScale() { return s_uiScale; }

void UIRenderer::DrawTextUI(const Font &font, const char *text, float x,
                            float y, float fontSize, Color color, float alpha) {
  if (!text || text[0] == '\0') {
    return;
  }

  float scaledSize = fontSize * s_uiScale;
  float scaledSpacing = 1.0f * s_uiScale;
  Vector2 pos = {x * s_uiScale, y * s_uiScale};
  Color finalColor = Fade(color, alpha);

  if (IsFontValid(font)) {
    const Font emojiFont = UISystem::GetEmojiFont();
    if (NeedsEmojiFallback(font, emojiFont, text)) {
      DrawTextWithEmojiFallback(font, emojiFont, text, pos, scaledSize,
                                scaledSpacing, finalColor);
    } else {
      DrawTextEx(font, text, pos, scaledSize, scaledSpacing, finalColor);
    }
  } else {
    DrawText(text, (int)pos.x, (int)pos.y, (int)scaledSize, finalColor);
  }
}

void UIRenderer::DrawTextScaled(const Font &font, const char *text, float x,
                                float y, float fontSize, float maxWidth,
                                Color color, float alpha) {
  if (!text || text[0] == '\0')
    return;

  float logicWidth = IsFontValid(font)
                         ? MeasureTextEx(font, text, fontSize, 1.0f).x
                         : (float)MeasureText(text, (int)fontSize);

  float finalFontSize = fontSize;
  float yOffset = 0.0f;

  if (logicWidth > maxWidth && maxWidth > 0) {
    float scale = maxWidth / logicWidth;
    finalFontSize = fontSize * scale;
    yOffset = (fontSize - finalFontSize) * 0.5f;
  }

  DrawTextUI(font, text, x, y + yOffset, finalFontSize, color, alpha);
}

void UIRenderer::DrawButton(const Font &font, Texture2D texture,
                             Rectangle bounds, const char *text, float fontSize,
                             Color textColor, Color textureTint, bool isHovered,
                             bool isPressed, float alpha) {
  float scale = s_uiScale;
  Rectangle dest = {bounds.x * scale, bounds.y * scale, bounds.width * scale,
                    bounds.height * scale};

  Color baseTint = Fade(textureTint, alpha);
  if (isPressed) {
    baseTint = ColorBrightness(baseTint, -0.2f);
  } else if (isHovered) {
    baseTint = ColorBrightness(baseTint, 0.2f);
  }

  if (texture.id > 0) {
    Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
    DrawTexturePro(texture, source, dest, {0, 0}, 0, baseTint);
  } else {
    // Fallback
    Color bg = isPressed ? s_theme.buttonPress
                         : (isHovered ? s_theme.buttonHover : s_theme.buttonNormal);
    DrawRectangleRec(dest, Fade(bg, alpha));
    DrawRectangleLinesEx(dest, 1.0f * scale, Fade(s_theme.panelBorder, alpha));
  }

  if (text && text[0] != '\0') {
    float scaledFontSize = fontSize * scale;
    const Font emojiFont = UISystem::GetEmojiFont();
    const float scaledSpacing = 1.0f * scale;
    Vector2 textSize = MeasureTextUI(font, emojiFont, text, scaledFontSize,
                                     scaledSpacing);

    Vector2 textPos = {dest.x + (dest.width - textSize.x) * 0.5f,
                       dest.y + (dest.height - textSize.y) * 0.5f};

    if (IsFontValid(font)) {
      const float logicalTextX = scale > 0.0f ? textPos.x / scale : textPos.x;
      const float logicalTextY = scale > 0.0f ? textPos.y / scale : textPos.y;
      const float shadowOffset = scale > 0.0f ? 1.0f / scale : 1.0f;

      DrawTextUI(font, text, logicalTextX + shadowOffset,
                 logicalTextY + shadowOffset, fontSize, BLACK,
                 0.6f * alpha);
      DrawTextUI(font, text, logicalTextX, logicalTextY, fontSize, textColor,
                 alpha);
    } else {
      DrawText(text, (int)textPos.x + 1, (int)textPos.y + 1, (int)scaledFontSize, Fade(BLACK, 0.6f * alpha));
      DrawText(text, (int)textPos.x, (int)textPos.y, (int)scaledFontSize,
               Fade(textColor, alpha));
    }
  }
}

Color UIRenderer::GetRarityColor(NoMoreDay::Rarity rarity) {
  // Implementation moved to NoMoreDayGameUiShared (design §5.3 ring 2 break).
  return UiShared::GetRarityColor(rarity);
}

const char *UIRenderer::GetShortItemTypeName(const ItemComponent &item) {
  if (item.type == ItemType::Weapon)
    return "武";
  if (item.type == ItemType::Armor)
    return "甲";
  if (item.type == ItemType::Jewelry)
    return "饰";
  if (item.type == ItemType::Consumable)
    return "耗";
  if (item.type == ItemType::Material)
    return "料";
  return "物";
}

const char *UIRenderer::GetItemCategoryString(const ItemComponent &item) {
  if (item.type == ItemType::Bag)
    return "背包";
  if (item.type == ItemType::Consumable)
    return "消耗品";
  if (item.type == ItemType::Material)
    return "材料";

  if (item.type == ItemType::Weapon) {
    if (item.isTwoHanded)
      return "双手武器";
    return "单手武器";
  }

  if (item.type == ItemType::Jewelry) {
    if (item.slot == EquipmentSlot::Neck)
      return "项链";
    return "戒指";
  }

  if (item.type == ItemType::Armor || item.type == ItemType::Shield) {
    switch (item.slot) {
    case EquipmentSlot::Head:
      return "头盔";
    case EquipmentSlot::Shoulder:
      return "护肩";
    case EquipmentSlot::Chest:
      return "胸甲";
    case EquipmentSlot::Hands:
      return "手套";
    case EquipmentSlot::Legs:
      return "护腿";
    case EquipmentSlot::Feet:
      return "鞋子";
    case EquipmentSlot::Neck:
      return "项链";
    case EquipmentSlot::Ring:
      return "戒指";
    case EquipmentSlot::OffHand:
      return "副手";
    default:
      return "装备";
    }
  }
  return "物品";
}

void UIRenderer::DrawSlot(const Font &font, entt::registry &registry, float x,
                          float y, float size, entt::entity item,
                          const char *defaultLabel, bool highlighted,
                          bool isLocked, float alpha, EquipmentSlot slotHint) {
  float sx = x * s_uiScale;
  float sy = y * s_uiScale;
  float sSize = size * s_uiScale;

  Rectangle rec = {sx, sy, sSize, sSize};

  auto ApplyAlpha = [&](Color c, float a) -> Color {
    return {c.r, c.g, c.b, (unsigned char)((float)c.a * a)};
  };

  // Background
  Color bg = highlighted ? ApplyAlpha(s_theme.panelBorderHighlight, 0.2f)
                         : s_theme.slotBackground;
  if (isLocked)
    bg = ApplyAlpha(BLACK, 0.8f);

  DrawRectangleRec(rec, ApplyAlpha(bg, alpha));

  // Bevel Effect
  DrawLineEx({sx, sy}, {sx + sSize, sy}, 2.0f * s_uiScale,
             ApplyAlpha(BLACK, 0.5f * alpha));
  DrawLineEx({sx, sy}, {sx, sy + sSize}, 2.0f * s_uiScale,
             ApplyAlpha(BLACK, 0.5f * alpha));
  DrawLineEx({sx, sy + sSize}, {sx + sSize, sy + sSize}, 1.0f * s_uiScale,
             ApplyAlpha(WHITE, 0.1f * alpha));
  DrawLineEx({sx + sSize, sy}, {sx + sSize, sy + sSize}, 1.0f * s_uiScale,
             ApplyAlpha(WHITE, 0.1f * alpha));

  // Border
  Color border =
      highlighted ? s_theme.panelBorderHighlight : s_theme.panelBorder;
  float borderThick = (slotHint != EquipmentSlot::None) ? 2.0f : 1.0f;
  DrawRectangleLinesEx(rec, borderThick * s_uiScale, ApplyAlpha(border, alpha));

  // Determine Background Texture
  entt::id_type bgTextureId = assets::ui::textures::Inventory_Slot.id;
  bool usingSpecificSlotIcon = false;

  if (slotHint != EquipmentSlot::None) {
      using namespace assets::ui::textures;
      usingSpecificSlotIcon = true;
      switch (slotHint) {
          case EquipmentSlot::MainHand: bgTextureId = Slot_Weapon_Main.id; break;
          case EquipmentSlot::OffHand:  bgTextureId = Slot_Weapon_Off.id; break;
          case EquipmentSlot::Head:     bgTextureId = Slot_Helmet.id; break;
          case EquipmentSlot::Shoulder: bgTextureId = Slot_Pauldrons.id; break;
          case EquipmentSlot::Chest:    bgTextureId = Slot_Armor_Chest.id; break;
          case EquipmentSlot::Hands:    bgTextureId = Slot_Gauntlets.id; break;
          case EquipmentSlot::Legs:     bgTextureId = Slot_Leggings.id; break;
          case EquipmentSlot::Feet:     bgTextureId = Slot_Boots.id; break;
          case EquipmentSlot::Neck:     bgTextureId = Slot_Amulet_Mirror.id; break;
          case EquipmentSlot::Ring1:    bgTextureId = Slot_Ring_1.id; break;
          case EquipmentSlot::Ring2:    bgTextureId = Slot_Ring_2.id; break;
          case EquipmentSlot::Ring:     bgTextureId = Slot_Ring_1.id; break;
          default: usingSpecificSlotIcon = false; break;
      }
  }

  // Draw Slot Background Texture if available
  Texture2D slotBg = AssetLoadingSystem::GetTexture(bgTextureId);
  if (slotBg.id > 0) {
    float bgAlpha = usingSpecificSlotIcon ? 0.8f : 0.5f; // More opaque for specific icons
    DrawTexturePro(slotBg, {0, 0, (float)slotBg.width, (float)slotBg.height},
                   rec, {0, 0}, 0.0f, ApplyAlpha(WHITE, bgAlpha * alpha));
  }

  // Ghost Icon (Procedural) - Only if NOT using a specific slot icon
  if (item == entt::null && slotHint != EquipmentSlot::None && !usingSpecificSlotIcon) {
      Color ghostColor = ApplyAlpha(WHITE, 0.1f * alpha);
      float cx = sx + sSize / 2;
      float cy = sy + sSize / 2;
      float r = sSize * 0.3f;

      switch (slotHint) {
          case EquipmentSlot::Head:
              DrawCircleLines(cx, cy, r, ghostColor);
              DrawLine(cx - r, cy, cx + r, cy, ghostColor);
              break;
          case EquipmentSlot::Shoulder:
              DrawRectangleLines(cx - r, cy - r*0.5f, r*2, r, ghostColor);
              break;
          case EquipmentSlot::Chest:
              DrawRectangleLines(cx - r*0.8f, cy - r, r*1.6f, r*2, ghostColor);
              break;
          case EquipmentSlot::Hands:
              DrawCircleLines(cx - r*0.5f, cy, r*0.4f, ghostColor);
              DrawCircleLines(cx + r*0.5f, cy, r*0.4f, ghostColor);
              break;
          case EquipmentSlot::Legs:
              DrawLine(cx - r*0.5f, cy - r, cx - r*0.5f, cy + r, ghostColor);
              DrawLine(cx + r*0.5f, cy - r, cx + r*0.5f, cy + r, ghostColor);
              DrawLine(cx - r*0.5f, cy - r, cx + r*0.5f, cy - r, ghostColor);
              break;
          case EquipmentSlot::Feet:
              DrawRectangleLines(cx - r*0.8f, cy, r*0.6f, r*0.5f, ghostColor);
              DrawRectangleLines(cx + r*0.2f, cy, r*0.6f, r*0.5f, ghostColor);
              break;
          case EquipmentSlot::Neck:
              DrawCircleLines(cx, cy - r*0.2f, r*0.6f, ghostColor);
              DrawCircleLines(cx, cy + r*0.6f, r*0.2f, ghostColor);
              break;
          case EquipmentSlot::Ring:
          case EquipmentSlot::Ring1:
          case EquipmentSlot::Ring2:
              DrawCircleLines(cx, cy, r*0.5f, ghostColor);
              DrawCircleLines(cx, cy, r*0.4f, ghostColor); // Double ring
              break;
          case EquipmentSlot::MainHand:
              DrawLine(cx - r, cy + r, cx + r, cy - r, ghostColor); // Slash
              DrawLine(cx + r*0.5f, cy - r*0.5f, cx + r, cy - r, ghostColor); // Tip
              break;
          case EquipmentSlot::OffHand:
              DrawCircleLines(cx, cy, r * 0.8f, ghostColor);
              DrawLine(cx, cy - r*0.8f, cx, cy + r*0.8f, ghostColor);
              break;
          default:
              break;
      }
  }

  if (item != entt::null && registry.valid(item)) {
    auto *itemComp = registry.try_get<ItemComponent>(item);
    if (itemComp) {
      Color rarityColor = GetRarityColor(itemComp->rarity);
      DrawRectangleLinesEx(rec, 2.0f * s_uiScale,
                           ApplyAlpha(rarityColor, alpha));

      Texture2D tex = AssetLoadingSystem::GetTexture(itemComp->textureId);
      if (tex.id > 0) {
        Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
        float pad = 4.0f * s_uiScale;
        Rectangle dest = {sx + pad, sy + pad, sSize - pad * 2, sSize - pad * 2};
        DrawTexturePro(tex, source, dest, {0, 0}, 0.0f,
                       ApplyAlpha(WHITE, alpha));
      } else {
        const char *shortName = GetShortItemTypeName(*itemComp);
        DrawTextUI(font, shortName, x + 10, y + 10, 16, rarityColor, alpha);
      }

      if (itemComp->quantity > 1) {
        DrawTextUI(font, std::to_string(itemComp->quantity).c_str(),
                   x + size - 15, y + size - 15, 12, s_theme.textPrimary,
                   alpha);
      }

      // Draw Legendary Potential (LP) - Shown at Top
      if (itemComp->legendaryPotential > 0) {
          float dotRadius = sSize * 0.08f;
          float gap = sSize * 0.05f;
          float totalW = itemComp->legendaryPotential * (dotRadius * 2) + (itemComp->legendaryPotential - 1) * gap;
          float startX = sx + (sSize - totalW) / 2.0f + dotRadius;
          float dotY = sy + dotRadius + 4.0f * s_uiScale; // Top padding

          for (int i = 0; i < itemComp->legendaryPotential; ++i) {
              float dotX = startX + i * (dotRadius * 2 + gap);
               DrawCircleV({dotX, dotY}, dotRadius, ApplyAlpha(VIOLET, 0.90f * alpha));
              DrawCircleLines((int)dotX, (int)dotY, dotRadius, ApplyAlpha(WHITE, 0.5f * alpha));
          }
      }

      // Draw Sockets - Shown at Bottom
      if (itemComp->socketCount > 0) {
          float dotRadius = sSize * 0.08f; // Relative size
          float gap = sSize * 0.05f;
          float totalW = itemComp->socketCount * (dotRadius * 2) + (itemComp->socketCount - 1) * gap;
          float startX = sx + (sSize - totalW) / 2.0f + dotRadius;
          float dotY = sy + sSize - dotRadius - 4.0f * s_uiScale; // Bottom padding

          for (int i = 0; i < itemComp->socketCount; ++i) {
              float dotX = startX + i * (dotRadius * 2 + gap);
              Vector2 center = {dotX, dotY};

              bool isFilled = false;
              if (i < (int)itemComp->sockets.size() && registry.valid(itemComp->sockets[i])) {
                  isFilled = true;
              }

              if (isFilled) {
                  // Filled Socket (Gold/Rune Color)
                  bool drawn = false;
                  if (i < (int)itemComp->sockets.size()) {
                       auto socketEntity = itemComp->sockets[i];
                       if (registry.valid(socketEntity)) {
                           if (const auto* sockItem = registry.try_get<ItemComponent>(socketEntity)) {
                                Texture2D tex = AssetLoadingSystem::GetTexture(sockItem->textureId);
                                if (tex.id > 0) {
                                     // Draw texture scaled
                                     Rectangle source = {0,0,(float)tex.width, (float)tex.height};
                                     float iconSize = dotRadius * 2.8f; // Larger than dot
                                     Rectangle dest = {center.x - iconSize/2, center.y - iconSize/2, iconSize, iconSize};
                                     DrawTexturePro(tex, source, dest, {0,0}, 0.0f, ApplyAlpha(WHITE, alpha));
                                     drawn = true;
                                }
                           }
                       }
                  }

                  if (!drawn) {
                      DrawCircleV(center, dotRadius, ApplyAlpha(GOLD, alpha));
                      DrawCircleLines((int)center.x, (int)center.y, dotRadius, ApplyAlpha(WHITE, 0.8f * alpha));
                  } else {
                      // Optional: Draw a thin border around the rune
                      // DrawCircleLines((int)center.x, (int)center.y, dotRadius * 1.4f, ApplyAlpha(GOLD, 0.5f * alpha));
                  }
              } else {
                  // Empty Socket (Dark Gray)
                  DrawCircleV(center, dotRadius, ApplyAlpha(DARKGRAY, 0.8f * alpha));
                  DrawCircleLines((int)center.x, (int)center.y, dotRadius, ApplyAlpha(GRAY, 0.5f * alpha));
              }
          }
      }

      if (isLocked || itemComp->isLocked) {
          // Draw a small lock icon or indicator
          DrawRectangleRec({sx + 2, sy + 2, 12 * s_uiScale, 12 * s_uiScale}, ApplyAlpha(RED, 0.8f * alpha));
          DrawTextUI(font, "L", x + 3, y + 2, 11, WHITE, alpha);
      }
    }
  }
}

void UIRenderer::DrawSkillSlot(const Font &font, float x, float y, float size,
                               Texture2D icon, const char *keyLabel,
                               float cooldownRatio, float remainingCooldown,
                               float manaCost, int charges, int maxCharges,
                               bool hasEnoughMana, bool isHighlighted,
                               bool isPressed, float alpha) {
  float sx = x * s_uiScale;
  float sy = y * s_uiScale;
  float sSize = size * s_uiScale;

  Rectangle rec = {sx, sy, sSize, sSize};

  auto ApplyAlpha = [&](Color c, float a) -> Color {
    return {c.r, c.g, c.b, (unsigned char)((float)c.a * a)};
  };

  // Background
  DrawRectangleRec(rec, ApplyAlpha(s_theme.slotBackground, alpha));
  if (isHighlighted || isPressed) {
    DrawRectangleRec(
        rec, ApplyAlpha(isPressed ? WHITE : s_theme.panelBorderHighlight,
                        0.15f * alpha));
  }

  // Draw Slot Background Texture if available
  Texture2D slotBg =
      AssetLoadingSystem::GetTexture(assets::ui::textures::Inventory_Slot.id);
  if (slotBg.id > 0) {
    DrawTexturePro(slotBg, {0, 0, (float)slotBg.width, (float)slotBg.height},
                   rec, {0, 0}, 0.0f, ApplyAlpha(WHITE, 0.3f * alpha));
  }

  if (icon.id > 0) {
    Rectangle source = {0, 0, (float)icon.width, (float)icon.height};
    float pad = (isPressed ? 6.0f : 4.0f) *
                s_uiScale; // Shrink slightly when pressed for "push" effect
    Rectangle dest = {sx + pad, sy + pad, sSize - pad * 2, sSize - pad * 2};

    Color iconColor = WHITE;
    if (!hasEnoughMana)
      iconColor = {100, 100, 255, 255}; // Mana tint
    else if (cooldownRatio > 0 && charges == 0)
      iconColor = ApplyAlpha(GRAY, 0.7f);

    DrawTexturePro(icon, source, dest, {0, 0}, 0.0f,
                   ApplyAlpha(iconColor, alpha));
  }

  // Cooldown Overlay (Circular Sector)
  if (cooldownRatio > 0.0f) {
    float startAngle = -90.0f;
    float endAngle = startAngle + (cooldownRatio * 360.0f);

    // If we have charges, the skill is usable, so don't darken it as much.
    // If charges == 0, it's fully on cooldown, so darken it more.
    float sectorAlpha = (charges == 0) ? 0.6f : 0.25f;

    DrawCircleSector({sx + sSize / 2, sy + sSize / 2}, sSize / 2, startAngle,
                     endAngle, 32, ApplyAlpha(BLACK, sectorAlpha * alpha));

    // Draw a subtle ring for the remaining cooldown
    DrawRing({sx + sSize / 2, sy + sSize / 2}, sSize / 2 - 2.0f * s_uiScale,
             sSize / 2, startAngle, 270.0f, 32,
             ApplyAlpha(s_theme.panelBorder, 0.3f * alpha));

    // Draw Remaining Time Text
    // Always draw text if we are showing the cooldown overlay
    char timeStr[16];
    if (remainingCooldown < 0.1f && remainingCooldown > 0.0f) {
      utils::FormatToBuffer(timeStr, "0.1"); // Minimum display
    } else {
      utils::FormatToBuffer(timeStr, "{:.1f}", remainingCooldown);
    }

    float fontSize = 24.0f;
    float textW =
        IsFontValid(font)
            ? MeasureTextEx(font, timeStr, fontSize * s_uiScale, 1.0f).x /
                  s_uiScale
            : (float)MeasureText(timeStr, (int)(fontSize * s_uiScale)) /
                  s_uiScale;

    // Draw drop shadow for text
    DrawTextUI(font, timeStr, x + (size - textW) / 2.0f + 1,
               y + (size - fontSize) / 2.0f + 1, fontSize,
               ApplyAlpha(BLACK, alpha));
    // Draw text in Yellow/Gold for better visibility
    DrawTextUI(font, timeStr, x + (size - textW) / 2.0f,
               y + (size - fontSize) / 2.0f, fontSize, ApplyAlpha(GOLD, alpha));
  }

  if (keyLabel)
    DrawTextUI(font, keyLabel, x + 4, y + 2, 12,
               ApplyAlpha(isHighlighted ? s_theme.textHighlight
                                        : s_theme.textSecondary,
                          alpha));

  if (manaCost > 0) {
    char manaStr[16];
    utils::FormatToBuffer(manaStr, "{:.0f}", manaCost);
    Color mColor = hasEnoughMana ? SKYBLUE : ApplyAlpha(BLUE, 0.7f);
    DrawTextUI(font, manaStr, x + 4, y + size - 14, 11,
               ApplyAlpha(mColor, alpha));
  }

  if (maxCharges > 1) {
    char chargeStr[16];
    utils::FormatToBuffer(chargeStr, "{}", charges);
    DrawTextUI(font, chargeStr, x + size - 12, y + size - 14, 13,
               ApplyAlpha(WHITE, alpha));
  }

  // Border
  float borderThick = (isHighlighted || isPressed) ? 2.0f : 1.0f;
  Color borderColor = isPressed ? WHITE
                                : (isHighlighted ? s_theme.panelBorderHighlight
                                                 : s_theme.panelBorder);
  DrawRectangleLinesEx(rec, borderThick * s_uiScale,
                       ApplyAlpha(borderColor, alpha));
}

void UIRenderer::DrawBuffIcon(const Font &font, float x, float y, float size,
                              Texture2D icon, const char *text,
                              float durationRatio, int stacks, bool isDebuff,
                              float alpha) {
  float sx = x * s_uiScale;
  float sy = y * s_uiScale;
  float sSize = size * s_uiScale;

  auto ApplyAlpha = [&](Color c, float a) -> Color {
    return {c.r, c.g, c.b, (unsigned char)((float)c.a * a)};
  };

  Rectangle rec = {sx, sy, sSize, sSize};
  DrawRectangleRec(rec, ApplyAlpha(s_theme.slotBackground, alpha));

  if (icon.id > 0) {
    Rectangle source = {0, 0, (float)icon.width, (float)icon.height};
    float pad = 2.0f * s_uiScale;
    Rectangle dest = {sx + pad, sy + pad, sSize - pad * 2, sSize - pad * 2};
    DrawTexturePro(icon, source, dest, {0, 0}, 0.0f, ApplyAlpha(WHITE, alpha));
  } else if (text && text[0] != '\0') {
    float fontSize = 16.0f;
    float sFontSize = fontSize * s_uiScale;
    Vector2 textSize =
        IsFontValid(font)
            ? MeasureTextEx(font, text, sFontSize, 1.0f)
            : Vector2{(float)MeasureText(text, (int)sFontSize), sFontSize};
    DrawTextUI(font, text, x + (size - textSize.x / s_uiScale) / 2.0f,
               y + (size - textSize.y / s_uiScale) / 2.0f, fontSize,
               ApplyAlpha(isDebuff ? RED : GREEN, alpha));
  } else {
    DrawRectangleRec(rec, ApplyAlpha(isDebuff ? RED : GREEN, 0.3f * alpha));
  }

  if (durationRatio > 0.0f) {
    float startAngle = -90.0f;
    float endAngle = startAngle + (durationRatio * 360.0f);
    // Draw a semi-transparent ring instead of sector for cleaner look
    DrawRing({sx + sSize / 2, sy + sSize / 2}, sSize / 2 - 2.0f * s_uiScale,
             sSize / 2, startAngle, endAngle, 16,
             ApplyAlpha(isDebuff ? RED : YELLOW, 0.4f * alpha));
  }

  if (stacks > 1) {
    char stackStr[16];
    utils::FormatToBuffer(stackStr, "{}", stacks);
    DrawTextUI(font, stackStr, x + size - 12, y + size - 12, 12,
               ApplyAlpha(WHITE, alpha));
  }

  DrawRectangleLinesEx(rec, 1.0f * s_uiScale,
                       ApplyAlpha(isDebuff ? RED : s_theme.panelBorder, alpha));
}

void UIRenderer::DrawSummonIcon(const Font &font, float x, float y, float width,
                                float height, Texture2D icon, float healthPct,
                                const char *name, float alpha) {
  float sx = x * s_uiScale;
  float sy = y * s_uiScale;
  float sw = width * s_uiScale;
  float sh = height * s_uiScale;

  auto ApplyAlpha = [&](Color c, float a) -> Color {
    return {c.r, c.g, c.b, (unsigned char)((float)c.a * a)};
  };

  // Panel Background
  DrawRectangleRec({sx, sy, sw, sh},
                   ApplyAlpha(s_theme.panelBackground, 0.8f * alpha));
  DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f * s_uiScale,
                       ApplyAlpha(s_theme.panelBorder, alpha));

  // Icon Background Glow (Optional but looks premium)
  float iconSize = sh - 8.0f * s_uiScale;
  float iconX = sx + 4.0f * s_uiScale;
  float iconY = sy + 4.0f * s_uiScale;

  if (icon.id > 0) {
    // Draw a subtle radial glow behind the icon
    DrawCircleGradient((int)(iconX + iconSize/2), (int)(iconY + iconSize/2), iconSize * 0.8f,
                       ApplyAlpha(SKYBLUE, 0.4f * alpha), ApplyAlpha(SKYBLUE, 0.0f));

    DrawTexturePro(
        icon, {0, 0, (float)icon.width, (float)icon.height},
        {iconX, iconY, iconSize, iconSize},
        {0, 0}, 0.0f, ApplyAlpha(WHITE, alpha));
  } else {
    DrawRectangleRec({iconX, iconY, iconSize, iconSize}, ApplyAlpha(DARKGRAY, 0.5f * alpha));
  }

  // Bar and Text Area
  float contentX = iconX + iconSize + 6.0f * s_uiScale;
  float barW = sw - (contentX - sx) - 8.0f * s_uiScale;
  float barH = 6.0f * s_uiScale;

  // Name
  float fontSize = 16.0f * s_uiScale;
  DrawTextEx(font, name, {contentX, sy + 6.0f * s_uiScale}, fontSize, 1.0f, ApplyAlpha(WHITE, alpha));

  // Health/Duration Bar
  Rectangle barRec = {contentX, sy + sh - 12.0f * s_uiScale, barW, barH};
  DrawRectangleRec(barRec, ApplyAlpha(BLACK, 0.5f * alpha));

  Color barColor = healthPct > 0.3f ? SKYBLUE : ORANGE;
  DrawRectangleRec({barRec.x, barRec.y, barRec.width * std::clamp(healthPct, 0.0f, 1.0f), barRec.height}, ApplyAlpha(barColor, 0.8f * alpha));
  DrawRectangleLinesEx(barRec, 1.0f, ApplyAlpha(WHITE, 0.2f * alpha));
}

void UIRenderer::DrawTooltip(const Font &font, entt::registry &registry,
                             entt::entity item, float alpha) {
  if (!IsWindowReady())
    return;
  auto *itemComp = registry.try_get<ItemComponent>(item);
  if (!itemComp)
    return;

  std::vector<TooltipLine> lines;
  lines.push_back({GetItemCategoryString(*itemComp), s_theme.textSecondary});

  // [NEW] Item Level Display
  int playerLevel = 1;
  // U8 收尾: 玩家实体经 registry 查询（原 State.playerEntity 缓存已删）。
  auto playerView = registry.view<PlayerTag, PlayerStats>();
  if (playerView.begin() != playerView.end()) {
    playerLevel = playerView.get<PlayerStats>(playerView.front()).level;
  }

  char lvlBuf[64];
  utils::FormatToBuffer(lvlBuf, "物品等级: {}", itemComp->itemLevel);
  Color lvlColor = (playerLevel >= itemComp->itemLevel) ? GREEN : RED;
  if (playerLevel < itemComp->itemLevel) {
      // Append warning if too low
      utils::FormatToBuffer(lvlBuf, "物品等级: {} (需要 Lv.{})",
                            itemComp->itemLevel, itemComp->itemLevel);
  }
  lines.push_back({lvlBuf, lvlColor});

  // --- Rune Sequence Display ---
  if (!itemComp->sockets.empty()) {
    std::string runeStr = "Runes: ";
    bool first = true;
    bool hasRunes = false;

    for (auto socketEntity : itemComp->sockets) {
      if (registry.valid(socketEntity)) {
        if (const auto *runeComp =
                registry.try_get<ItemComponent>(socketEntity)) {
          if (!first)
            runeStr += " • ";
          // Strip "Rune of " prefix if present for cleaner display
          std::string name = runeComp->name;
          const std::string prefix = "Rune of ";
          if (name.rfind(prefix, 0) == 0) {
            name = name.substr(prefix.length());
          }
          runeStr += name;
          first = false;
          hasRunes = true;
        }
      }
    }

    if (hasRunes) {
      lines.push_back({runeStr, GOLD});
    }
  }

  // --- Legendary Potential ---
  if (itemComp->legendaryPotential > 0) {
      char lpBuf[64];
      utils::FormatToBuffer(lpBuf, "传奇潜力: {}",
                            itemComp->legendaryPotential);
      lines.push_back({lpBuf, VIOLET});
  }

  char buffer[128];
  if (itemComp->attack > 0) {
    utils::FormatToBuffer(buffer, "攻击力: {:.0f}", itemComp->attack);
    lines.push_back({buffer, s_theme.textPrimary});
  }
  if (itemComp->defense > 0) {
    utils::FormatToBuffer(buffer, "护甲: {:.0f}", itemComp->defense);
    lines.push_back({buffer, s_theme.textPrimary});
  }
  if (itemComp->bagCapacity > 0) {
    utils::FormatToBuffer(buffer, "容量: {} 格", itemComp->bagCapacity);
    lines.push_back({buffer, s_theme.textPrimary});
  }

  for (const auto &aff : itemComp->implicits) {
    lines.push_back(
        {GetAffixDescription(aff, false), GetAffixTierColor(aff.tier)});
  }

  if ((itemComp->attack > 0 || itemComp->defense > 0 ||
       !itemComp->implicits.empty()) &&
      !itemComp->affixes.empty()) {
    lines.push_back({"---", WHITE, true});
  }

  for (const auto &aff : itemComp->affixes) {
    Color color = GetAffixTierColor(aff.tier);
    if (aff.isLegendary)
      color = RED;
    lines.push_back({GetAffixDescription(aff, true), color});
  }

  // --- Socket Count ---
  if (itemComp->socketCount > 0) {
      char sockBuf[64];
      utils::FormatToBuffer(sockBuf, "插槽数量: {}", itemComp->socketCount);
      lines.push_back({sockBuf, NoMoreDay::components::Colors::COLOR_SOCKET_INFO});
  }

  if (!itemComp->description.empty()) {
    lines.push_back({" ", WHITE});

    // Handle multi-line descriptions (e.g. Map Fragments)
    std::stringstream ss(itemComp->description);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        // Remove CR if present (Windows specific safety)
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) {
             lines.push_back({line, s_theme.textPrimary});
        }
    }
  }

  float fontSize = 18.0f;
  float titleSize = 22.0f;
  float padding = 10.0f;
  float lineHeight = fontSize + 4.0f;

  float maxW = 0.0f;
  Vector2 titleDim =
      IsFontValid(font)
          ? MeasureTextEx(font, itemComp->name.c_str(), titleSize, 1.0f)
          : Vector2{(float)MeasureText(itemComp->name.c_str(), (int)titleSize),
                    titleSize};
  maxW = std::max(maxW, titleDim.x);
  for (const auto &line : lines) {
    if (line.isSeparator || line.text == " ")
      continue;
    float w = IsFontValid(font)
                  ? MeasureTextEx(font, line.text.c_str(), fontSize, 1.0f).x
                  : (float)MeasureText(line.text.c_str(), (int)fontSize);
    maxW = std::max(maxW, w);
  }

  float iconSectionHeight = 0.0f;
  Texture2D iconTex = { 0 };
  if (itemComp->textureId != 0) {
      iconTex = AssetLoadingSystem::GetTexture(itemComp->textureId);
      if (iconTex.id > 0) {
          iconSectionHeight = 64.0f + 10.0f; // Icon height + spacing
      }
  }

  float w = std::max(maxW + padding * 2, 64.0f + padding * 2);
  float h = padding * 2 + titleSize + 5.0f + iconSectionHeight + lines.size() * lineHeight;

  Vector2 m = GetMousePosition();
  float x = m.x + 15 * s_uiScale;
  float y = m.y + 15 * s_uiScale;
  float sW = w * s_uiScale;
  float sH = h * s_uiScale;

  if (IsWindowReady()) {
    if (x + sW > (float)GetScreenWidth())
      x -= (sW + 20 * s_uiScale);
    if (y + sH > (float)GetScreenHeight())
      y -= (sH + 20 * s_uiScale);
  }

  DrawRectangle((int)x, (int)y, (int)sW, (int)sH,
                Fade(s_theme.panelBackground, 0.95f * alpha));
  DrawRectangleLinesEx({x, y, sW, sH}, 1.0f * s_uiScale,
                       Fade(GetRarityColor(itemComp->rarity), alpha));

  DrawTextUI(font, itemComp->name.c_str(),
             (x + padding * s_uiScale) / s_uiScale,
             (y + padding * s_uiScale) / s_uiScale, titleSize,
             GetRarityColor(itemComp->rarity), alpha);

  float curSY = y + (padding + titleSize + 5.0f) * s_uiScale;

  if (iconTex.id > 0) {
      float sIconSize = 64.0f * s_uiScale;
      Rectangle source = { 0, 0, (float)iconTex.width, (float)iconTex.height };
      Rectangle dest = { x + (sW - sIconSize) / 2.0f, curSY, sIconSize, sIconSize };
      DrawTexturePro(iconTex, source, dest, { 0, 0 }, 0.0f, Fade(WHITE, alpha));
      curSY += (64.0f + 10.0f) * s_uiScale;
  }

  for (const auto &line : lines) {
    if (line.isSeparator) {
      DrawLineEx(
          {x + padding * s_uiScale, curSY + lineHeight * s_uiScale / 2},
          {x + sW - padding * s_uiScale, curSY + lineHeight * s_uiScale / 2},
          1.0f * s_uiScale, Fade(s_theme.panelBorder, alpha));
    } else if (line.text != " ") {
      DrawTextUI(font, line.text.c_str(), (x + padding * s_uiScale) / s_uiScale,
                 curSY / s_uiScale, fontSize, line.color, alpha);
    }
    curSY += lineHeight * s_uiScale;
  }
}

std::vector<UIRenderer::TooltipLine>
UIRenderer::GetSkillTooltipLines(entt::registry &registry, uint32_t skillId) {
  const auto *skill = SkillRegistry::Get().GetSkill(skillId);
  if (!skill)
    return {};

  std::vector<TooltipLine> lines;
  lines.push_back({skill->name_key, YELLOW});

  auto playerView = registry.view<PlayerTag, CombatStats>();
  const SkillDisplayPreview preview =
      (playerView.begin() != playerView.end())
          ? SkillDisplayPreviewService::Build(registry, playerView.front(),
                                              skillId)
          : SkillDisplayPreview{};

  char buffer[128];
  if (preview.has_estimated_damage) {
    utils::FormatToBuffer(buffer, "{}: {:.0f}",
                          ResolveDamageLabel(preview.estimated_damage_mode),
                          preview.estimated_damage_value);
    lines.push_back({buffer, {255, 150, 50, 255}});
  }

  utils::FormatToBuffer(buffer, "法力消耗: {:.0f}", skill->mana_cost);

  lines.push_back({buffer, s_theme.textSecondary});

  if (skill->cooldown > 0) {
    utils::FormatToBuffer(buffer, "冷却时间: {:.1f}s", skill->cooldown);
    lines.push_back({buffer, s_theme.textSecondary});
  }

  // --- NEW: Tags Display ---
  std::string tagStr = "标签: ";
  bool firstTag = true;
  for (int i = 0; i < 64; ++i) { // Check all 64 bits
    Tag t = static_cast<Tag>(1ULL << i);
    if (HasTag(skill->tags, t)) {
      if (!firstTag)
        tagStr += ", ";
      tagStr += std::string(GetTagName(t));
      firstTag = false;
    }
  }
  if (!firstTag) { // Only add if tags exist
    lines.push_back({tagStr, GRAY});
  }

  if (!skill->desc_key.empty()) {
    lines.push_back({" ", WHITE}); // Spacer
    lines.push_back({skill->desc_key, s_theme.textPrimary});
  }

  return lines;
}

static std::string TruncateTextToWidth(const Font &font,
                                       const std::string &text,
                                       float maxWidth,
                                       float fontSize) {
  if (text.empty() || maxWidth <= 0.0f) {
    return text;
  }
  if (MeasureTextEx(font, text.c_str(), fontSize, 1.0f).x <= maxWidth) {
    return text;
  }

  static constexpr const char *kEllipsis = "...";
  const float ellipsisW = MeasureTextEx(font, kEllipsis, fontSize, 1.0f).x;
  if (ellipsisW >= maxWidth) {
    return kEllipsis;
  }

  std::string out;
  const char *ptr = text.c_str();
  while (*ptr != '\0') {
    int bytes = 0;
    const int cp = GetCodepointNext(ptr, &bytes);
    if (bytes <= 0 || cp == '\n') {
      break;
    }

    int utf8Bytes = 0;
    const char *utf8 = CodepointToUTF8(cp, &utf8Bytes);
    const std::string candidate = out + std::string(utf8, utf8Bytes);
    if (MeasureTextEx(font, (candidate + kEllipsis).c_str(), fontSize, 1.0f).x >
        maxWidth) {
      break;
    }

    out = candidate;
    ptr += bytes;
  }

  if (out.empty()) {
    return kEllipsis;
  }
  return out + kEllipsis;
}

static void DrawTooltipHeader(const Font &font, const char *name, uint32_t iconId,
                              float x, float y, float w, float alpha) {
  float iconSize = TT_ICON_SIZE * UIRenderer::GetScale();
  float padding = TT_PADDING * UIRenderer::GetScale();

  // Icon
  if (iconId != 0) {
    Texture2D icon = AssetLoadingSystem::GetTexture(iconId);
    if (icon.id > 0) {
      Rectangle source = {0, 0, (float)icon.width, (float)icon.height};
      Rectangle dest = {x + padding, y + padding, iconSize, iconSize};
      // Icon Background / Glow
      DrawCircleGradient((int)(dest.x + iconSize / 2), (int)(dest.y + iconSize / 2),
                         iconSize * 0.7f, Fade(GOLD, 0.3f * alpha), Fade(GOLD, 0));
      DrawTexturePro(icon, source, dest, {0, 0}, 0.0f, Fade(WHITE, alpha));
      // Icon Border
      DrawRectangleLinesEx(dest, 1.0f, Fade(TT_COLOR_BORDER, alpha));
    }
  }

  // Name
  const float titleX = x + padding +
                       (iconId != 0 ? iconSize + 12.0f * UIRenderer::GetScale() : 0.0f);
  const float titleW = std::max(0.0f, x + w - padding - titleX);
  const std::string title =
      TruncateTextToWidth(font, name ? name : "", titleW, 24.0f * UIRenderer::GetScale());

  UIRenderer::DrawTextUI(font, title.c_str(), titleX / UIRenderer::GetScale(),
                         (y + padding + 4.0f * UIRenderer::GetScale()) /
                             UIRenderer::GetScale(),
                         24.0f, TT_COLOR_HEADER, alpha);

  // Subtitle / Type (Example: Active Skill)
  UIRenderer::DrawTextUI(font, "主动技能", titleX / UIRenderer::GetScale(),
                         (y + padding + 32.0f * UIRenderer::GetScale()) / UIRenderer::GetScale(),
                         14.0f, TT_COLOR_STAT_LABEL, alpha);
}

static void DrawTooltipStatRow(const Font &font, const char *label, const char *value,
                               float x, float y, float w, Color valueColor, float alpha) {
  float padding = TT_PADDING * UIRenderer::GetScale();
  UIRenderer::DrawTextUI(font, label, (x + padding) / UIRenderer::GetScale(),
                         y / UIRenderer::GetScale(), 16.0f, TT_COLOR_STAT_LABEL, alpha);

  float valW = MeasureTextEx(font, value, 16.0f * UIRenderer::GetScale(), 1.0f).x;
  UIRenderer::DrawTextUI(font, value, (x + w - padding - valW) / UIRenderer::GetScale(),
                         y / UIRenderer::GetScale(), 16.0f, valueColor, alpha);
}

static void DrawTagChip(const Font &font, const char *tag, float x, float y, float alpha) {
  float scale = UIRenderer::GetScale();
  float fontSize = 13.0f;
  Vector2 textSize = MeasureTextEx(font, tag, fontSize * scale, 1.0f);

  float padH = 8.0f * scale;
  float padV = 4.0f * scale;
  Rectangle rec = {x, y, textSize.x + padH * 2, textSize.y + padV * 2};

  DrawRectangleRec(rec, Fade(TT_COLOR_TAG_BG, alpha));
  DrawRectangleLinesEx(rec, 1.0f, Fade(TT_COLOR_TAG_BORDER, alpha));

  UIRenderer::DrawTextUI(font, tag, (x + padH) / scale, (y + padV) / scale, fontSize,
                         {200, 200, 210, 255}, alpha);
}

static std::vector<std::string> GetWrappedLines(const Font &font, const char *text, float maxWidth, float fontSize) {
  std::vector<std::string> lines;
  if (!text || text[0] == '\0') return lines;

  std::string currentLine;
  std::string word;
  const char *ptr = text;

  while (*ptr != '\0') {
    int bytes = 0;
    int cp = GetCodepointNext(ptr, &bytes);
    if (bytes <= 0) break;

    if (cp == '\n') {
      lines.push_back(currentLine + word);
      currentLine.clear();
      word.clear();
    } else if (cp == ' ' || cp == '\t' || cp > 127) {
      // Space or CJK character (CJK acts as a word break in simple wrapping)
      std::string nextWord = word;
      if (cp > 127) {
        int utf8Bytes = 0;
        const char* utf8 = CodepointToUTF8(cp, &utf8Bytes);
        nextWord += std::string(utf8, utf8Bytes);
      } else {
        nextWord += " ";
      }

      float testW = MeasureTextEx(font, (currentLine + nextWord).c_str(), fontSize, 1.0f).x;
      if (testW > maxWidth && !currentLine.empty()) {
        lines.push_back(currentLine);
        currentLine = nextWord;
      } else {
        currentLine += nextWord;
      }
      word.clear();
    } else {
      int utf8Bytes = 0;
      const char* utf8 = CodepointToUTF8(cp, &utf8Bytes);
      word += std::string(utf8, utf8Bytes);
    }
    ptr += bytes;
  }
  if (!currentLine.empty() || !word.empty()) {
    lines.push_back(currentLine + word);
  }
  return lines;
}

void UIRenderer::DrawSkillTooltip(const Font &font, entt::registry &registry,
                                  uint32_t skillId, float alpha,
                                  bool forceDraw) {
  const auto *skill = SkillRegistry::Get().GetSkill(skillId);
  if (!skill) return;

  if (forceDraw) alpha = 1.0f;

  float scale = s_uiScale;
  float padding = TT_PADDING * scale;
  float spacing = TT_SECTION_SPACING * scale;
  float maxW = TT_MAX_WIDTH * scale;

  // --- 1. Data Preparation ---
  struct Stat { std::string label; std::string value; Color color; };
  std::vector<Stat> coreStats;

  char buf[64];
  auto playerView = registry.view<PlayerTag, CombatStats>();
  const SkillDisplayPreview preview = (playerView.begin() != playerView.end())
                                          ? SkillDisplayPreviewService::Build(
                                                registry, playerView.front(), skillId)
                                          : SkillDisplayPreview{};

  if (skill->mana_cost > 0) {
    utils::FormatToBuffer(buf, "{:.0f}", skill->mana_cost);
    coreStats.push_back({"法力消耗", buf, SKYBLUE});
  }
  if (skill->cooldown > 0) {
    utils::FormatToBuffer(buf, "{:.1f}s", skill->cooldown);
    coreStats.push_back({"冷却时间", buf, WHITE});
  }
  if (preview.has_duration) {
    utils::FormatToBuffer(buf, "{:.1f}s", preview.display_duration_seconds);
    coreStats.push_back({"持续时间", buf, WHITE});
  }
  if (preview.has_estimated_damage) {
    utils::FormatToBuffer(buf, "{:.0f}", preview.estimated_damage_value);
    coreStats.push_back({ResolveDamageLabel(preview.estimated_damage_mode), buf,
                         {255, 150, 50, 255}});
  }


  // --- 2. Height Calculation ---
  float totalH = padding;
  totalH += TT_HEADER_HEIGHT * scale + spacing; // Header

  if (!coreStats.empty()) {
      totalH += coreStats.size() * (18.0f * scale) + spacing; // Stats
  }

  // Description Wrapping
  std::vector<std::string> descLines = GetWrappedLines(font, skill->desc_key.c_str(), maxW - padding * 2, 16.0f * scale);
  float descH = descLines.size() * 20.0f * scale;
  if (!descLines.empty()) totalH += descH + spacing;

  // Tags Height (Simulate Flow Layout)
  std::vector<std::string> tags;
  for (int i = 0; i < 64; ++i) {
      Tag t = static_cast<Tag>(1ULL << i);
      if (HasTag(skill->tags, t)) tags.push_back(std::string(GetTagName(t)));
  }

  std::vector<std::string> displayTags;
  displayTags.reserve(tags.size());

  const float chipPad = 16.0f * scale;
  const float chipGap = 6.0f * scale;
  const float chipRowH = 24.0f * scale;
  const float maxChipW = std::max(1.0f, maxW - padding * 2.0f);

  float tagSectionH = 0;
  if (!tags.empty()) {
      float testX = padding;
      float testY = 0;

      for (const auto& tag : tags) {
          const std::string fitTag = TruncateTextToWidth(
              font, tag, std::max(1.0f, maxChipW - chipPad), 13.0f * scale);
          displayTags.push_back(fitTag);

          const float tagW =
              std::min(maxChipW, MeasureTextEx(font, fitTag.c_str(), 13.0f * scale, 1.0f).x + chipPad);
          if (testX + tagW > maxW - padding && testX > padding) {
              testX = padding;
              testY += chipRowH;
          }
          testX += tagW + chipGap;
      }
      tagSectionH = testY + chipRowH;
      totalH += (16.0f * scale) + (6.0f * scale) + tagSectionH + spacing;
  }

  totalH += padding;

  // --- 3. Positioning (Smart Anchor & Lock) ---
  // U8 收尾: the position lock is renderer-local state (was
  // State.tooltipPos / State.tooltipInitialized); TooltipController passes
  // forceDraw = !locked so the lock resets when the hover target changes.
  if (!s_skillTooltipInitialized || forceDraw) {
      Vector2 m = GetMousePosition();
      float screenW = (float)GetScreenWidth();
      float screenH = (float)GetScreenHeight();
      float safeMargin = 12.0f * scale;

      float targetX = m.x + 20.0f * scale;
      float targetY = m.y - totalH * 0.3f;

      if (targetX + maxW > screenW - safeMargin) targetX = m.x - maxW - 20.0f * scale;
      targetX = std::clamp(targetX, safeMargin, std::max(safeMargin, screenW - maxW - safeMargin));

      if (targetY + totalH > screenH - safeMargin) targetY = screenH - totalH - safeMargin;
      targetY = std::clamp(targetY, safeMargin, std::max(safeMargin, screenH - totalH - safeMargin));

      s_skillTooltipPos = {targetX, targetY};
      s_skillTooltipInitialized = true;
  }

  float x = s_skillTooltipPos.x;
  float y = s_skillTooltipPos.y;

  // --- 4. Rendering ---
  DrawRectangleRec({x + 4, y + 4, maxW, totalH}, Fade(BLACK, 0.4f * alpha)); // Shadow
  DrawRectangleRec({x, y, maxW, totalH}, Fade(TT_COLOR_BG, alpha));
  DrawRectangleLinesEx({x, y, maxW, totalH}, 1.0f, Fade(TT_COLOR_BORDER, alpha));

  float curY = y;
  DrawTooltipHeader(font, skill->name_key.c_str(), skill->icon_id, x, curY, maxW, alpha);
  curY += (TT_HEADER_HEIGHT * scale) + spacing;

  DrawLineEx({x + padding, curY - spacing/2}, {x + maxW - padding, curY - spacing/2}, 1.0f, Fade(TT_COLOR_BORDER, 0.5f * alpha));

  for (const auto& stat : coreStats) {
      DrawTooltipStatRow(font, stat.label.c_str(), stat.value.c_str(), x, curY, maxW, stat.color, alpha);
      curY += 18.0f * scale;
  }
  if (!coreStats.empty()) curY += spacing;

  if (!descLines.empty()) {
      DrawLineEx({x + padding, curY - spacing/2}, {x + maxW - padding, curY - spacing/2}, 1.0f, Fade(TT_COLOR_BORDER, 0.3f * alpha));
      for (const auto& line : descLines) {
          DrawTextUI(font, line.c_str(), (x + padding) / scale, curY / scale, 16.0f, {220, 220, 230, 255}, alpha);
          curY += 20.0f * scale;
      }
      curY += spacing;
  }

  if (!displayTags.empty()) {
      DrawTextUI(font, "标签", (x + padding) / scale, curY / scale, 14.0f,
                 TT_COLOR_STAT_LABEL, alpha);
      curY += 22.0f * scale;

      float tagX = x + padding;
      float tagY = curY;
      for (const auto& tag : displayTags) {
          const float tagW =
              std::min(maxChipW, MeasureTextEx(font, tag.c_str(), 13.0f * scale, 1.0f).x + chipPad);
          if (tagX + tagW > x + maxW - padding && tagX > x + padding) {
              tagX = x + padding;
              tagY += chipRowH;
          }
          DrawTagChip(font, tag.c_str(), tagX, tagY, alpha);
          tagX += tagW + chipGap;
      }
  }
}

static std::string GetStatModifierDescription(const StatModifier &mod) {
  char buffer[128];
  const char *sign = mod.value >= 0 ? "+" : "";
  const char *suffix = "";
  float displayValue = mod.value;

  if (mod.mode == ModifierMode::PercentAdd) {
    suffix = "%";
  } else if (mod.mode == ModifierMode::PercentMult) {
    sign = "x";
    displayValue = 1.0f + mod.value;
    suffix = "";
  }

  const char *statName = "属性";
  switch (mod.type) {
  case StatType::Strength:
    statName = "力量";
    break;
  case StatType::Dexterity:
    statName = "敏捷";
    break;
  case StatType::Intelligence:
    statName = "智力";
    break;
  case StatType::Vitality:
    statName = "体质";
    break;
  case StatType::MaxHealth:
    statName = "最大生命值";
    break;
  case StatType::MaxMana:
    statName = "最大法力值";
    break;
  case StatType::MoveSpeed:
    statName = "移动速度";
    break;
  case StatType::Armor:
    statName = "护甲";
    break;
  case StatType::PhysicalDamage:
    statName = "物理伤害";
    break;
  case StatType::FireDamage:
    statName = "火焰伤害";
    break;
  case StatType::ColdDamage:
    statName = "冰霜伤害";
    break;
  case StatType::LightningDamage:
    statName = "闪电伤害";
    break;
  case StatType::PoisonDamage:
    statName = "毒素伤害";
    break;
  case StatType::ShadowDamage:
    statName = "暗影伤害";
    break;
  case StatType::CritChance:
    statName = "暴击率";
    break;
  case StatType::CritDamage:
    statName = "暴击伤害";
    break;
  case StatType::AttackSpeed:
    statName = "攻击速度";
    break;
  case StatType::CastSpeed:
    statName = "施法速度";
    break;
  case StatType::Accuracy:
    statName = "命中率";
    break;
  case StatType::ManaOnHit:
    statName = "击中回蓝";
    break;
  case StatType::ResistPhysical:
    statName = "物理抗性";
    break;
  case StatType::ResistFire:
    statName = "火焰抗性";
    break;
  case StatType::ResistCold:
    statName = "冰霜抗性";
    break;
  case StatType::ResistLightning:
    statName = "闪电抗性";
    break;
  case StatType::ResistPoison:
    statName = "毒素抗性";
    break;
  case StatType::ResistShadow:
    statName = "暗影抗性";
    break;
  case StatType::ResistAll:
    statName = "全抗性";
    break;
  default:
    break;
  }

  if (mod.mode == ModifierMode::PercentMult) {
    utils::FormatToBuffer(buffer, "{}{:.2f} {}", sign, displayValue,
                          statName);
  } else {
    utils::FormatToBuffer(buffer, "{}{:.0f}{} {}", sign, displayValue,
                          suffix, statName);
  }
  return std::string(buffer);
}

void UIRenderer::DrawBuffTooltip(const Font &font, const BuffEffect &effect,
                                 float alpha) {
  const auto &visual = BuffRegistry::GetVisualData(effect.type);
  if (visual.name == "Unknown")
    return;

  std::vector<TooltipLine> lines;

  Color titleColor = visual.is_debuff ? RED : GREEN;
  std::string title = effect.name.empty() ? visual.name : effect.name;
  if (effect.stacks > 1)
    title += TextFormat(" (x%d)", effect.stacks);
  lines.push_back({title, titleColor});

  const std::string description =
      effect.description.empty() ? visual.description : effect.description;
  lines.push_back({description, s_theme.textPrimary});

  for (const auto &mod : effect.modifiers) {
    lines.push_back({GetStatModifierDescription(mod), titleColor});
  }

  if (effect.duration > 0 && effect.remaining < 3600.0f) {
    char timeBuf[64];
    utils::FormatToBuffer(timeBuf, "剩余时间: {:.1f}s", effect.remaining);
    lines.push_back({timeBuf, s_theme.textSecondary});
  }

  float padding = 10.0f;
  float fontSize = 16.0f;
  float titleSize = 18.0f;

  float maxW = 220.0f;
  float h = padding * 2;
  for (size_t i = 0; i < lines.size(); ++i) {
    h += (i == 0 ? titleSize : fontSize) + 4;
  }

  Vector2 m = GetMousePosition();
  float x = m.x + 15 * s_uiScale;
  float y = m.y + 15 * s_uiScale;
  float sw = (maxW + padding * 2) * s_uiScale;
  float sh = h * s_uiScale;

  if (IsWindowReady()) {
    if (x + sw > (float)GetScreenWidth())
      x -= (sw + 20 * s_uiScale);
    if (y + sh > (float)GetScreenHeight())
      y -= (sh + 20 * s_uiScale);
  }

  DrawRectangle((int)x, (int)y, (int)sw, (int)sh,
                Fade(s_theme.panelBackground, 0.95f * alpha));
  DrawRectangleLinesEx({x, y, sw, sh}, 1.0f * s_uiScale,
                       Fade(titleColor, alpha));

  float curSY = y + padding * s_uiScale;
  for (size_t i = 0; i < lines.size(); ++i) {
    float size = (i == 0) ? titleSize : fontSize;
    DrawTextScaled(font, lines[i].text.c_str(),
                   (x + padding * s_uiScale) / s_uiScale, curSY / s_uiScale,
                   size, maxW, lines[i].color, alpha);
    curSY += (size + 4) * s_uiScale;
  }
}

// --- R8: snapshot-driven tooltips -----------------------------------------
// The tooltip render path never touches the registry: content is read from
// the bounded GameUiSnapshot display views. The legacy DrawTooltip /
// DrawSkillTooltip / DrawBuffTooltip (registry) variants stay for the
// remaining runtime paths and the UI tests.

namespace {

// Rebuilds a lightweight Affix from the snapshot view so the existing
// GetAffixDescription / GetAffixTierColor formatting (and the UI tests that
// lock those strings) stay identical.
Affix ToAffix(const GameUiAffixView& view) {
  Affix affix;
  affix.type = static_cast<AffixType>(view.type);
  affix.value = view.value;
  affix.tier = view.tier;
  affix.isPrefix = view.isPrefix;
  affix.isLegendary = view.isLegendary;
  return affix;
}

} // namespace

void UIRenderer::DrawTooltipFromSnapshot(const Font& font,
                                         const GameUiSnapshot& snapshot,
                                         uint64_t domainId, float alpha) {
  if (!IsWindowReady()) {
    return;
  }
  const GameUiItemView* itemView = nullptr;
  for (const auto& view : snapshot.displayedItems) {
    if (view.domainId == domainId) {
      itemView = &view;
      break;
    }
  }
  if (itemView == nullptr) {
    return;
  }
  const auto& item = *itemView;

  // Category string via a minimal ItemComponent (type/slot/isTwoHanded only).
  ItemComponent categoryProbe;
  categoryProbe.type = static_cast<ItemType>(item.itemType);
  categoryProbe.slot = static_cast<EquipmentSlot>(item.slot);
  categoryProbe.isTwoHanded = item.isTwoHanded;

  std::vector<TooltipLine> lines;
  lines.push_back({GetItemCategoryString(categoryProbe), s_theme.textSecondary});

  const int playerLevel = snapshot.player.hasPlayer ? snapshot.player.level : 1;
  char lvlBuf[64];
  utils::FormatToBuffer(lvlBuf, "物品等级: {}", item.itemLevel);
  Color lvlColor = (playerLevel >= item.itemLevel) ? GREEN : RED;
  if (playerLevel < item.itemLevel) {
    utils::FormatToBuffer(lvlBuf, "物品等级: {} (需要 Lv.{})", item.itemLevel,
                          item.itemLevel);
  }
  lines.push_back({lvlBuf, lvlColor});

  // Rune sequence (names were captured by the builder into the view).
  bool hasRunes = false;
  for (const auto& runeName : item.socketRuneNames) {
    if (runeName[0] != '\0') {
      hasRunes = true;
      break;
    }
  }
  if (hasRunes) {
    std::string runeStr = "Runes: ";
    bool first = true;
    for (const auto& runeName : item.socketRuneNames) {
      if (runeName[0] == '\0') {
        continue;
      }
      std::string name(runeName.data());
      const std::string prefix = "Rune of ";
      if (name.rfind(prefix, 0) == 0) {
        name = name.substr(prefix.length());
      }
      if (!first) {
        runeStr += " • ";
      }
      runeStr += name;
      first = false;
    }
    lines.push_back({runeStr, GOLD});
  }

  if (item.legendaryPotential > 0) {
    char lpBuf[64];
    utils::FormatToBuffer(lpBuf, "传奇潜力: {}", item.legendaryPotential);
    lines.push_back({lpBuf, VIOLET});
  }

  char buffer[128];
  if (item.attack > 0) {
    utils::FormatToBuffer(buffer, "攻击力: {:.0f}", item.attack);
    lines.push_back({buffer, s_theme.textPrimary});
  }
  if (item.defense > 0) {
    utils::FormatToBuffer(buffer, "护甲: {:.0f}", item.defense);
    lines.push_back({buffer, s_theme.textPrimary});
  }
  if (item.bagCapacity > 0) {
    utils::FormatToBuffer(buffer, "容量: {} 格", item.bagCapacity);
    lines.push_back({buffer, s_theme.textPrimary});
  }

  for (const auto& view : item.implicits) {
    const Affix affix = ToAffix(view);
    lines.push_back({GetAffixDescription(affix, false),
                     GetAffixTierColor(affix.tier)});
  }

  if ((item.attack > 0 || item.defense > 0 || !item.implicits.empty()) &&
      !item.affixes.empty()) {
    lines.push_back({"---", WHITE, true});
  }

  for (const auto& view : item.affixes) {
    const Affix affix = ToAffix(view);
    Color color = GetAffixTierColor(affix.tier);
    if (affix.isLegendary) {
      color = RED;
    }
    lines.push_back({GetAffixDescription(affix, true), color});
  }

  if (item.socketCount > 0) {
    char sockBuf[64];
    utils::FormatToBuffer(sockBuf, "插槽数量: {}", item.socketCount);
    lines.push_back(
        {sockBuf, NoMoreDay::components::Colors::COLOR_SOCKET_INFO});
  }

  if (!item.description.empty()) {
    lines.push_back({" ", WHITE});
    std::stringstream ss(item.description);
    std::string line;
    while (std::getline(ss, line, '\n')) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (!line.empty()) {
        lines.push_back({line, s_theme.textPrimary});
      }
    }
  }

  float fontSize = 18.0f;
  float titleSize = 22.0f;
  float padding = 10.0f;
  float lineHeight = fontSize + 4.0f;

  float maxW = 0.0f;
  Vector2 titleDim =
      IsFontValid(font)
          ? MeasureTextEx(font, item.name.c_str(), titleSize, 1.0f)
          : Vector2{(float)MeasureText(item.name.c_str(), (int)titleSize),
                    titleSize};
  maxW = std::max(maxW, titleDim.x);
  for (const auto& line : lines) {
    if (line.isSeparator || line.text == " ") {
      continue;
    }
    float w = IsFontValid(font)
                  ? MeasureTextEx(font, line.text.c_str(), fontSize, 1.0f).x
                  : (float)MeasureText(line.text.c_str(), (int)fontSize);
    maxW = std::max(maxW, w);
  }

  float iconSectionHeight = 0.0f;
  Texture2D iconTex = {0};
  if (item.textureId != 0) {
    iconTex = AssetLoadingSystem::GetTexture(item.textureId);
    if (iconTex.id > 0) {
      iconSectionHeight = 64.0f + 10.0f;
    }
  }

  float w = std::max(maxW + padding * 2, 64.0f + padding * 2);
  float h = padding * 2 + titleSize + 5.0f + iconSectionHeight +
            lines.size() * lineHeight;

  Vector2 m = GetMousePosition();
  float x = m.x + 15 * s_uiScale;
  float y = m.y + 15 * s_uiScale;
  float sW = w * s_uiScale;
  float sH = h * s_uiScale;

  if (IsWindowReady()) {
    if (x + sW > (float)GetScreenWidth()) {
      x -= (sW + 20 * s_uiScale);
    }
    if (y + sH > (float)GetScreenHeight()) {
      y -= (sH + 20 * s_uiScale);
    }
  }

  const Color rarityColor = GetRarityColor(static_cast<Rarity>(item.rarity));
  DrawRectangle((int)x, (int)y, (int)sW, (int)sH,
                Fade(s_theme.panelBackground, 0.95f * alpha));
  DrawRectangleLinesEx({x, y, sW, sH}, 1.0f * s_uiScale,
                       Fade(rarityColor, alpha));

  DrawTextUI(font, item.name.c_str(),
             (x + padding * s_uiScale) / s_uiScale,
             (y + padding * s_uiScale) / s_uiScale, titleSize, rarityColor,
             alpha);

  float curSY = y + (padding + titleSize + 5.0f) * s_uiScale;

  if (iconTex.id > 0) {
    float sIconSize = 64.0f * s_uiScale;
    Rectangle source = {0, 0, (float)iconTex.width, (float)iconTex.height};
    Rectangle dest = {x + (sW - sIconSize) / 2.0f, curSY, sIconSize, sIconSize};
    DrawTexturePro(iconTex, source, dest, {0, 0}, 0.0f, Fade(WHITE, alpha));
    curSY += (64.0f + 10.0f) * s_uiScale;
  }

  for (const auto& line : lines) {
    if (line.isSeparator) {
      DrawLineEx(
          {x + padding * s_uiScale, curSY + lineHeight * s_uiScale / 2},
          {x + sW - padding * s_uiScale, curSY + lineHeight * s_uiScale / 2},
          1.0f * s_uiScale, Fade(s_theme.panelBorder, alpha));
    } else if (line.text != " ") {
      DrawTextUI(font, line.text.c_str(), (x + padding * s_uiScale) / s_uiScale,
                 curSY / s_uiScale, fontSize, line.color, alpha);
    }
    curSY += lineHeight * s_uiScale;
  }
}

void UIRenderer::DrawSkillTooltipFromSnapshot(const Font& font,
                                              const GameUiSnapshot& snapshot,
                                              uint32_t skillId, float alpha,
                                              bool forceDraw) {
  const auto* skill = SkillRegistry::Get().GetSkill(skillId);
  if (!skill) {
    return;
  }
  if (forceDraw) {
    alpha = 1.0f;
  }

  float scale = s_uiScale;
  float padding = TT_PADDING * scale;
  float spacing = TT_SECTION_SPACING * scale;
  float maxW = TT_MAX_WIDTH * scale;

  // R8: the estimated-damage / duration preview rows are skipped in the
  // snapshot path (SkillDisplayPreviewService requires registry + CombatStats,
  // which the render path must not touch). Mana cost / cooldown / tags /
  // description are static skill data and stay.
  struct Stat {
    std::string label;
    std::string value;
    Color color;
  };
  std::vector<Stat> coreStats;

  char buf[64];
  if (skill->mana_cost > 0) {
    utils::FormatToBuffer(buf, "{:.0f}", skill->mana_cost);
    coreStats.push_back({"法力消耗", buf, SKYBLUE});
  }
  if (skill->cooldown > 0) {
    utils::FormatToBuffer(buf, "{:.1f}s", skill->cooldown);
    coreStats.push_back({"冷却时间", buf, WHITE});
  }

  float totalH = padding;
  totalH += TT_HEADER_HEIGHT * scale + spacing;

  if (!coreStats.empty()) {
    totalH += coreStats.size() * (18.0f * scale) + spacing;
  }

  std::vector<std::string> descLines =
      GetWrappedLines(font, skill->desc_key.c_str(), maxW - padding * 2,
                      16.0f * scale);
  float descH = descLines.size() * 20.0f * scale;
  if (!descLines.empty()) {
    totalH += descH + spacing;
  }

  std::vector<std::string> tags;
  for (int i = 0; i < 64; ++i) {
    Tag t = static_cast<Tag>(1ULL << i);
    if (HasTag(skill->tags, t)) {
      tags.push_back(std::string(GetTagName(t)));
    }
  }

  std::vector<std::string> displayTags;
  displayTags.reserve(tags.size());

  const float chipPad = 16.0f * scale;
  const float chipGap = 6.0f * scale;
  const float chipRowH = 24.0f * scale;
  const float maxChipW = std::max(1.0f, maxW - padding * 2.0f);

  float tagSectionH = 0;
  if (!tags.empty()) {
    float testX = padding;
    float testY = 0;
    for (const auto& tag : tags) {
      const std::string fitTag =
          TruncateTextToWidth(font, tag, std::max(1.0f, maxChipW - chipPad),
                              13.0f * scale);
      displayTags.push_back(fitTag);
      const float tagW = std::min(
          maxChipW,
          MeasureTextEx(font, fitTag.c_str(), 13.0f * scale, 1.0f).x + chipPad);
      if (testX + tagW > maxW - padding && testX > padding) {
        testX = padding;
        testY += chipRowH;
      }
      testX += tagW + chipGap;
    }
    tagSectionH = testY + chipRowH;
    totalH += (16.0f * scale) + (6.0f * scale) + tagSectionH + spacing;
  }

  totalH += padding;

  if (!s_skillTooltipInitialized || forceDraw) {
    Vector2 m = GetMousePosition();
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float safeMargin = 12.0f * scale;

    float targetX = m.x + 20.0f * scale;
    float targetY = m.y - totalH * 0.3f;

    if (targetX + maxW > screenW - safeMargin) {
      targetX = m.x - maxW - 20.0f * scale;
    }
    targetX = std::clamp(targetX, safeMargin,
                         std::max(safeMargin, screenW - maxW - safeMargin));

    if (targetY + totalH > screenH - safeMargin) {
      targetY = screenH - totalH - safeMargin;
    }
    targetY = std::clamp(targetY, safeMargin,
                         std::max(safeMargin, screenH - totalH - safeMargin));

    s_skillTooltipPos = {targetX, targetY};
    s_skillTooltipInitialized = true;
  }

  float x = s_skillTooltipPos.x;
  float y = s_skillTooltipPos.y;

  DrawRectangleRec({x + 4, y + 4, maxW, totalH}, Fade(BLACK, 0.4f * alpha));
  DrawRectangleRec({x, y, maxW, totalH}, Fade(TT_COLOR_BG, alpha));
  DrawRectangleLinesEx({x, y, maxW, totalH}, 1.0f, Fade(TT_COLOR_BORDER, alpha));

  float curY = y;
  DrawTooltipHeader(font, skill->name_key.c_str(), skill->icon_id, x, curY,
                    maxW, alpha);
  curY += (TT_HEADER_HEIGHT * scale) + spacing;

  DrawLineEx({x + padding, curY - spacing / 2},
             {x + maxW - padding, curY - spacing / 2}, 1.0f,
             Fade(TT_COLOR_BORDER, 0.5f * alpha));

  for (const auto& stat : coreStats) {
    DrawTooltipStatRow(font, stat.label.c_str(), stat.value.c_str(), x, curY,
                       maxW, stat.color, alpha);
    curY += 18.0f * scale;
  }
  if (!coreStats.empty()) {
    curY += spacing;
  }

  if (!descLines.empty()) {
    DrawLineEx({x + padding, curY - spacing / 2},
               {x + maxW - padding, curY - spacing / 2}, 1.0f,
               Fade(TT_COLOR_BORDER, 0.3f * alpha));
    for (const auto& line : descLines) {
      DrawTextUI(font, line.c_str(), (x + padding) / scale, curY / scale,
                 16.0f, {220, 220, 230, 255}, alpha);
      curY += 20.0f * scale;
    }
    curY += spacing;
  }

  if (!displayTags.empty()) {
    DrawTextUI(font, "标签", (x + padding) / scale, curY / scale, 14.0f,
               TT_COLOR_STAT_LABEL, alpha);
    curY += 22.0f * scale;

    float tagX = x + padding;
    float tagY = curY;
    for (const auto& tag : displayTags) {
      const float tagW =
          std::min(maxChipW,
                   MeasureTextEx(font, tag.c_str(), 13.0f * scale, 1.0f).x +
                       chipPad);
      if (tagX + tagW > x + maxW - padding && tagX > x + padding) {
        tagX = x + padding;
        tagY += chipRowH;
      }
      DrawTagChip(font, tag.c_str(), tagX, tagY, alpha);
      tagX += tagW + chipGap;
    }
  }
}

void UIRenderer::DrawBuffTooltipFromView(const Font& font,
                                         const GameUiBuffView& buff,
                                         float alpha) {
  if (buff.name[0] == '\0') {
    return;
  }

  std::vector<TooltipLine> lines;

  Color titleColor = buff.isDebuff ? RED : GREEN;
  std::string title(buff.name.data());
  if (buff.stacks > 1) {
    title += TextFormat(" (x%d)", buff.stacks);
  }
  lines.push_back({title, titleColor});

  if (buff.description[0] != '\0') {
    lines.push_back({std::string(buff.description.data()),
                     s_theme.textPrimary});
  }

  if (buff.duration > 0 && buff.remaining < 3600.0f) {
    char timeBuf[64];
    utils::FormatToBuffer(timeBuf, "剩余时间: {:.1f}s", buff.remaining);
    lines.push_back({timeBuf, s_theme.textSecondary});
  }

  float padding = 10.0f;
  float fontSize = 16.0f;
  float titleSize = 18.0f;

  float maxW = 220.0f;
  float h = padding * 2;
  for (size_t i = 0; i < lines.size(); ++i) {
    h += (i == 0 ? titleSize : fontSize) + 4;
  }

  Vector2 m = GetMousePosition();
  float x = m.x + 15 * s_uiScale;
  float y = m.y + 15 * s_uiScale;
  float sw = (maxW + padding * 2) * s_uiScale;
  float sh = h * s_uiScale;

  if (IsWindowReady()) {
    if (x + sw > (float)GetScreenWidth()) {
      x -= (sw + 20 * s_uiScale);
    }
    if (y + sh > (float)GetScreenHeight()) {
      y -= (sh + 20 * s_uiScale);
    }
  }

  DrawRectangle((int)x, (int)y, (int)sw, (int)sh,
                Fade(s_theme.panelBackground, 0.95f * alpha));
  DrawRectangleLinesEx({x, y, sw, sh}, 1.0f * s_uiScale,
                       Fade(titleColor, alpha));

  float curSY = y + padding * s_uiScale;
  for (size_t i = 0; i < lines.size(); ++i) {
    float size = (i == 0) ? titleSize : fontSize;
    DrawTextScaled(font, lines[i].text.c_str(),
                   (x + padding * s_uiScale) / s_uiScale, curSY / s_uiScale,
                   size, maxW, lines[i].color, alpha);
    curSY += (size + 4) * s_uiScale;
  }
}


} // namespace NoMoreDay
