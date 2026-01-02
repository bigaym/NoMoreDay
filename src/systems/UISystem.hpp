#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "../components/ItemComponent.hpp"
#include "../core/ResourceManager.hpp"
#include "UICommon.hpp"
#include "../core/UIRenderer.hpp"

class LevelManager; 

class UISystem {
public:
    // --- Public State ---
    static NoMoreDay::UIContext State;

    // --- Lifecycle ---
    static void Initialize(ResourceManager& resourceManager);
    static void Shutdown();
    static void Update(entt::registry& registry, const LevelManager& levelManager);
        static void Draw(entt::registry& registry, const LevelManager& levelManager, const Camera2D& camera);
        static void DrawSkillHotbar(entt::registry& registry);
        static void DrawBuffs(entt::registry& registry);
        static void Benchmark(entt::registry& registry, const LevelManager& levelManager, int frames);
    
    // --- Resource Access ---
    static Font GetFont() { return State.globalFont; }
    static Color GetRarityColor(NoMoreDay::Rarity rarity) { return NoMoreDay::UIRenderer::GetRarityColor(rarity); }

    // --- Drawing Helpers (Delegated to UIRenderer) ---
    static void DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel = nullptr, bool highlighted = false, bool isLocked = false, float alpha = 1.0f);
    static void DrawTextUI(const char* text, float x, float y, float fontSize, Color color, float alpha = 1.0f);
    static void DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha = 1.0f);
    
    // --- Interaction ---
    static void OpenContextMenu(entt::entity item, bool fromInv, int invIdx, NoMoreDay::EquipmentSlot slot);
    static void DrawQuantityPopup(entt::registry& registry);

    // --- Helpers ---
    static Vector2 GetMousePositionLogic();

private:
    // Internal helpers
    static void DrawTooltip(entt::registry& registry, entt::entity item);
    static void DrawContextMenu(entt::registry& registry);
    static void DrawMessageBox();
};