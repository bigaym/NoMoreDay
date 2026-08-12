#include "game/application/ui/UISystem.hpp"
#include "core/logging/Logger.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/components/Common.hpp"

#include <string>
#include <vector>


using namespace NoMoreDay;

// --- Static Member Initialization ---
// U8 收尾: 字体为渲染资源类静态（非可变 UI 状态）。原先 UISystem::State
// （UIContext 静态对象）整体删除，面板/编排全部迁入 GameUiHost 实例控制器。
Font UISystem::s_globalFont = {};
Font UISystem::s_emojiFont = {};

// --- Lifecycle ---

void UISystem::Initialize(ResourceManager &resourceManager) {
  AssetLoadingSystem::Initialize(resourceManager);
  AssetLoadingSystem::LoadAllUI(); // Load core UI textures (buttons, panels, etc.)
  AssetLoadingSystem::LoadAllEquipment(); // Ensure all equipment textures are
                                          // registered

#ifdef TEST_HEADLESS
  LOG_INFO("UISystem: Headless mode, skipping font loading.");
  s_globalFont = GetFontDefault();
  return;
#endif

  const auto &mainFont = assets::ui::fonts::Main_Chinese;
  s_emojiFont = {0};

  auto LoadEmojiFallbackFont = [&]() {
    // Explicitly load the emoji set currently used by UI labels.
    std::vector<int> emojiCodepoints = {
        0x2694, // ⚔
        0x1F392, // 🎒
        0x1F451, // 👑
        0x1F455, // 👕
        0x1F48D, // 💍
        0x1F4FF, // 📿
        0x1F6E1, // 🛡
        0x1F97E, // 🥾
        0x1F9BF, // 🦿
        0x1F9E4, // 🧤
        0x1F9E9, // 🧩
        0x1F9EA, // 🧪
        0x1F9F9, // 🧹
        0x1FA96, // 🪖
        0x1FA99  // 🪙
    };

    std::vector<std::string> emojiFontCandidates = {
        "C:/Windows/Fonts/seguiemj.ttf", // Segoe UI Emoji
        "C:/Windows/Fonts/seguisym.ttf"  // Segoe UI Symbol
    };

    const entt::id_type emojiFontId = entt::hashed_string("ui_font_emoji");
    for (const auto &emojiPath : emojiFontCandidates) {
      if (!FileExists(emojiPath.c_str())) {
        continue;
      }
      s_emojiFont = resourceManager.loadFont(
          emojiFontId, emojiPath, mainFont.defaultSize, emojiCodepoints.data(),
          (int)emojiCodepoints.size());
      if (s_emojiFont.texture.id != 0) {
        SetTextureFilter(s_emojiFont.texture, TEXTURE_FILTER_BILINEAR);
        LOG_INFO("UISystem: Loaded emoji fallback font from '{}'", emojiPath);
        return;
      }
    }

    LOG_WARN("UISystem: Emoji fallback font unavailable, emoji will degrade to '?'");
    s_emojiFont = {0};
  };

  std::vector<int> codepoints;
  for (int i = 32; i <= 126; ++i)
    codepoints.push_back(i);
  codepoints.push_back(0x2022); // •
  codepoints.push_back(0x00B7); // ·
  codepoints.push_back(0x2605); // ★
  codepoints.push_back(0x26A0); // ⚠️
  for (int i = 0x3000; i <= 0x303F; ++i)
    codepoints.push_back(i);
  for (int i = 0x4E00; i <= 0x9FFF; ++i)
    codepoints.push_back(i);
  for (int i = 0xFF00; i <= 0xFFEF; ++i)
    codepoints.push_back(i);

  std::vector<std::string> fontCandidates;
  fontCandidates.push_back("C:/Windows/Fonts/simhei.ttf");
  fontCandidates.push_back("C:/Windows/Fonts/msyh.ttc");
  fontCandidates.push_back("C:/Windows/Fonts/simsun.ttc");

  for (const auto &path : fontCandidates) {
    if (FileExists(path.c_str())) {
      LOG_INFO("UISystem: Attempting to load font from '{}'...", path);
      s_globalFont =
          resourceManager.loadFont(mainFont.id, path, mainFont.defaultSize,
                                   codepoints.data(), (int)codepoints.size());

      if (s_globalFont.texture.id != 0) {
        SetTextureFilter(s_globalFont.texture, TEXTURE_FILTER_BILINEAR);
        LoadEmojiFallbackFont();
        LOG_INFO("UISystem: Successfully loaded Chinese font from '{}'", path);
        return;
      } else {
        LOG_WARN(
            "UISystem: Failed to load font from '{}', trying next candidate...",
            path);
      }
    }
  }

  LOG_ERROR("UISystem: All Chinese font candidates failed. Falling back to "
            "default font (??? for Chinese).");
  if (s_globalFont.texture.id == 0)
    s_globalFont = GetFontDefault();
  LoadEmojiFallbackFont();
}

void UISystem::Shutdown() {
  s_globalFont = {0};
  s_emojiFont = {0};
  AssetLoadingSystem::Shutdown();
}

// --- Helper ---

entt::entity UISystem::GetPlayerEntity(entt::registry &registry) {
  auto view = registry.view<PlayerTag>();
  if (view.begin() == view.end())
    return entt::null;
  return view.front();
}

float UISystem::GetScaleFactor() { return UIRenderer::GetScale(); }

Vector2 UISystem::GetMousePositionLogic() {
  Vector2 m = GetMousePosition();
  float s = UIRenderer::GetScale();
  if (s <= 0.0001f)
    s = 1.0f;
  return {m.x / s, m.y / s};
}

// --- Delegate to UIRenderer ---

void UISystem::DrawSlot(entt::registry &registry, float x, float y, float size,
                        entt::entity item, const char *defaultLabel,
                        bool highlighted, bool isLocked, float alpha) {
  UIRenderer::DrawSlot(s_globalFont, registry, x, y, size, item,
                       defaultLabel, highlighted, isLocked, alpha);
}

void UISystem::DrawTextUI(const char *text, float x, float y, float fontSize,
                          Color color, float alpha) {
  UIRenderer::DrawTextUI(s_globalFont, text, x, y, fontSize, color, alpha);
}

void UISystem::DrawTextScaled(const char *text, float x, float y,
                              float fontSize, float maxWidth, Color color,
                              float alpha) {
  UIRenderer::DrawTextScaled(s_globalFont, text, x, y, fontSize, maxWidth,
                             color, alpha);
}
