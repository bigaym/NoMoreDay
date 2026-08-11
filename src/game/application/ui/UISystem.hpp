#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "game/foundation/components/ItemComponent.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/ui_shared/UiShared.hpp"

class LevelManager; 
namespace NoMoreDay { namespace systems { class SpatialHashGrid; } }
namespace NoMoreDay::ui {
class SkillHotbarController;
class SkillTreeController;
class UIStashController;
class UICraftingController;
class AstrolabeController;
class OverlayController;
class TooltipController;
}

class UISystem {
public:
    // --- Public State ---
    static NoMoreDay::UIContext State;

    // --- Lifecycle ---
    static void Initialize(ResourceManager& resourceManager);
    static void Shutdown();
    // Clears gameplay-scoped session state (panels, drag, tooltip, message
    // box, quantity popup). Invoked by GameUiHost on Enter/LeaveGameplay so
    // no session data leaks into the next run (design §4.2).
    static void ResetSessionState();
    static void Update(entt::registry& registry, const LevelManager& levelManager, NoMoreDay::ui::UIStashController* stashController = nullptr, NoMoreDay::ui::UICraftingController* craftingController = nullptr, NoMoreDay::ui::SkillTreeController* skillTreeController = nullptr, NoMoreDay::ui::AstrolabeController* astrolabeController = nullptr, NoMoreDay::ui::OverlayController* overlayController = nullptr);
        static void Draw(entt::registry& registry, const LevelManager& levelManager, const Camera2D& camera, NoMoreDay::systems::SpatialHashGrid* spatialGrid = nullptr, NoMoreDay::ui::SkillHotbarController* hotbarController = nullptr, NoMoreDay::ui::UIStashController* stashController = nullptr, NoMoreDay::ui::UICraftingController* craftingController = nullptr, NoMoreDay::ui::SkillTreeController* skillTreeController = nullptr, NoMoreDay::ui::AstrolabeController* astrolabeController = nullptr, NoMoreDay::ui::OverlayController* overlayController = nullptr);
        static void Benchmark(entt::registry& registry, const LevelManager& levelManager, int frames);
        // Draws the drag phantom (item/skill) and, when a tooltip controller
        // is supplied, routes the top-most tooltip through it. The legacy
        // State-based tooltip block remains as the null-controller fallback
        // (U7 group 6-B; null-host render path).
        static void DrawDraggingPhantom(entt::registry& registry, NoMoreDay::ui::TooltipController* tooltipController = nullptr);
    
    // --- Resource Access ---
    // Font/Rarity lookups are delegated to NoMoreDayGameUiShared so the render
    // adapter can read them without depending on the UI target (design §5.3).
    static Font GetFont() { return NoMoreDay::UiShared::GlobalFont(); }
    static Font GetEmojiFont() { return State.emojiFont; }
    static Color GetRarityColor(NoMoreDay::Rarity rarity) { return NoMoreDay::UiShared::GetRarityColor(rarity); }

    // --- Drawing Helpers (Delegated to UIRenderer) ---
    static void DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false, bool isLocked = false, float alpha = 1.0f);
    static void DrawTextUI(const char* text, float x, float y, float fontSize, Color color, float alpha = 1.0f);
    static void DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha = 1.0f);
    
    // --- Interaction ---
    static void OpenContextMenu(entt::entity item, bool fromInv, int invIdx, NoMoreDay::EquipmentSlot slot);
    static void DrawQuantityPopup(entt::registry& registry);

    // --- Helpers ---
    static entt::entity GetPlayerEntity(entt::registry& registry);
    static Vector2 GetMousePositionLogic();
    static bool IsModalInputCaptured();
    static void UpdatePanelDrag(NoMoreDay::UIPanelID id, float& x, float& y, float w, float h, float headerHeight);

private:

    // Internal helpers

    static void DrawTooltip(entt::registry& registry, entt::entity item);

    static void DrawContextMenu(entt::registry& registry);

    static void DrawMessageBox();



    static inline bool s_hasGivenTestItems = false;

};
