#include "UIAstrolabe.hpp"
#include "../components/AstrolabeUIComponent.hpp"
#include "../components/PlayerState.hpp"
#include "../components/Common.hpp" // For PlayerTag
#include "../core/UIRenderer.hpp"
#include "../systems/UISystem.hpp"
#include "../core/AstrolabeRegistry.hpp" // Include Registry
#include "raylib.h"
#include "raymath.h" // For Vector2 operations

using namespace NoMoreDay;

void UIAstrolabe::Update(entt::registry& registry) {
    auto view = registry.view<PlayerTag, AstrolabeUIComponent>();
    for (auto entity : view) {
        auto& ui = view.get<AstrolabeUIComponent>(entity);
        
        float dt = GetFrameTime();
        if (ui.isOpen) {
            ui.alpha += 10.0f * dt; // Fast fade in
            if (ui.alpha > 1.0f) ui.alpha = 1.0f;
        } else {
            ui.alpha -= 10.0f * dt; // Fast fade out
            if (ui.alpha < 0.0f) ui.alpha = 0.0f;
        }

        if (ui.alpha <= 0.001f && !ui.isOpen) continue;
        
        // Input Handling (Only if fully open or mostly open)
        if (ui.isOpen) {
            // Zoom
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                float zoomSpeed = 0.1f;
                ui.zoom += wheel * zoomSpeed;
                if (ui.zoom < 0.2f) ui.zoom = 0.2f;
                if (ui.zoom > 3.0f) ui.zoom = 3.0f;
            }

            // Pan (Right Mouse or Middle Mouse)
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                Vector2 delta = GetMouseDelta();
                // Apply delta inversely scaled by zoom? Or just direct? 
                // Usually we move camera, so offset moves opposite to mouse drag.
                // But here ui.offset is "camera position". 
                // If I drag mouse Left, I want to move camera Right (so world moves Left).
                // So offset += delta / zoom * -1.
                // Let's stick to "offset is the center point in World Space".
                ui.offset.x -= delta.x / ui.zoom;
                ui.offset.y -= delta.y / ui.zoom;
            }
        }

        // Allow closing with ESC if open
        // (Handled by UISystem generally, but extra safety here is fine or redundant)
    }
}

void UIAstrolabe::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag, AstrolabeUIComponent>();
    for (auto entity : view) {
        auto& ui = view.get<AstrolabeUIComponent>(entity);
        
        if (ui.alpha <= 0.001f) continue;

        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        Vector2 screenCenter = { screenW / 2.0f, screenH / 2.0f };

        // 1. Background
        DrawRectangle(0, 0, (int)screenW, (int)screenH, Fade(Color{15, 15, 20, 255}, 0.95f * ui.alpha));
        
        // Grid (Optional)
        // Draw some concentric circles or grid lines based on offset
        
        // 2. Nodes
        const auto& nodes = AstrolabeRegistry::Get().GetAllNodes();
        
        // Helper to transform World -> Screen
        auto WorldToScreen = [&](Vector2 worldPos) -> Vector2 {
            return Vector2Add(Vector2Scale(Vector2Subtract(worldPos, ui.offset), ui.zoom), screenCenter);
        };

        // Draw Connections first (Lines)
        for (const auto& pair : nodes) {
            const auto& node = pair.second;
            Vector2 startPos = { node.x, node.y };
            Vector2 screenStart = WorldToScreen(startPos);
            
            // Check visibility (Frustum cull roughly)
            if (screenStart.x < -100 || screenStart.x > screenW + 100 || screenStart.y < -100 || screenStart.y > screenH + 100) {
                 // Optimization: Skip if parent is also out? 
                 // For now, just draw.
            }

            for (uint32_t parentId : node.prerequisites) {
                // Find parent
                // Note: GetAllNodes is map<id, node>.
                auto it = nodes.find(parentId);
                if (it != nodes.end()) {
                    Vector2 endPos = { it->second.x, it->second.y };
                    Vector2 screenEnd = WorldToScreen(endPos);
                    DrawLineEx(screenStart, screenEnd, 2.0f * ui.zoom, Fade(DARKGRAY, ui.alpha));
                }
            }
        }

        // Draw Nodes
        for (const auto& pair : nodes) {
            const auto& node = pair.second;
            Vector2 worldPos = { node.x, node.y };
            Vector2 screenPos = WorldToScreen(worldPos);

            // Frustum Culling
            if (screenPos.x < -50 || screenPos.x > screenW + 50 || screenPos.y < -50 || screenPos.y > screenH + 50) continue;

            float baseSize = 10.0f;
            Color color = GRAY;

            switch (node.type) {
                case AstrolabeNodeType::Minor: 
                    baseSize = 12.0f; 
                    color = SKYBLUE;
                    break;
                case AstrolabeNodeType::Major: 
                    baseSize = 18.0f; 
                    color = GOLD;
                    break;
                case AstrolabeNodeType::Keystone: 
                    baseSize = 28.0f; 
                    color = PURPLE;
                    break;
            }

            float size = baseSize * ui.zoom;

            // Draw
            DrawCircleV(screenPos, size, Fade(color, ui.alpha));
            DrawCircleLines(screenPos.x, screenPos.y, size, Fade(WHITE, ui.alpha));
            
            // Text for debugging (ID)
            if (ui.zoom > 0.8f) {
                DrawText(TextFormat("%d", node.id), (int)screenPos.x - 5, (int)screenPos.y - 5, 10, WHITE);
            }
        }

        // 3. UI Overlay (Points, Close Button)
        UISystem::DrawTextUI("Astrolabe", 20, 20, 30, WHITE, ui.alpha);
        
        auto* astroComp = registry.try_get<AstrolabeComponent>(entity);
        if (astroComp) {
            UISystem::DrawTextUI(TextFormat("Points: %d", astroComp->available_points), 20, 60, 24, GOLD, ui.alpha);
        }
    }
}
