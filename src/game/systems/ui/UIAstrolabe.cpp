#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/data/TalentLoader.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/resource/AssetRegistry.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/Progression.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/stats/AttributePipeline.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay {

AstrolabeMap UIAstrolabe::s_map;
AstrolabeView UIAstrolabe::s_view;
bool UIAstrolabe::s_loaded = false;
bool UIAstrolabe::s_visible = false;
float UIAstrolabe::s_alpha = 0.0f;

void UIAstrolabe::Initialize() {
    if (s_loaded) return;
    
    // Load data
    if (!TalentLoader::LoadAstrolabe("assets/data/astrolabe.json", s_map)) {
        LOG_WARN("UIAstrolabe: assets/data/astrolabe.json not found, creating default map.");
        TalentLoader::CreateDefaultMap(s_map);
    }
    
    // Sync with Registry for stat application
    AstrolabeRegistry::Get().SetMap(s_map);
    LOG_INFO("UIAstrolabe: Initialization complete. Stars: {}, First Star: ({}, {})", 
             s_map.stars.size(), s_map.stars.empty() ? 0 : s_map.stars.begin()->second.x, 
             s_map.stars.empty() ? 0 : s_map.stars.begin()->second.y);
    
    // Initialize Renderer with galaxy shader
    Shader galaxyShader = AssetLoadingSystem::GetShader(assets::shaders::Galaxy_Procedural.id);
    AstrolabeRenderer::Init(galaxyShader);
    
    // Initialize View
    s_view.resolution = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    s_view.camera.offset = { s_view.resolution.x / 2.0f, s_view.resolution.y / 2.0f };
    s_view.camera.rotation = 0.0f;
    ResetView();
    
    s_loaded = true;
}

void UIAstrolabe::EnsureLoaded() {
    if (!s_loaded) Initialize();
}

void UIAstrolabe::ResetView() {
    using namespace Constants::Astrolabe;
    // Camera looks at origin (where talent nodes are centered)
    // Galaxy center is offset to align with tree visual center
    s_view.camera.target = { 0, 0 };
    s_view.camera.zoom = INITIAL_ZOOM;
}

void UIAstrolabe::Update(entt::registry& registry) {
}

void UIAstrolabe::Toggle(entt::registry& registry, entt::entity player) {
    s_visible = !s_visible;
    if (s_visible) {
        EnsureLoaded();
        ResetView();
    }
}

bool UIAstrolabe::IsVisible(entt::registry& registry, entt::entity player) {
    return s_visible || s_alpha > 0.0f;
}

void UIAstrolabe::Draw(entt::registry& registry) {
    if (!s_visible && s_alpha <= 0.0f) return;
    
    auto view = registry.view<PlayerTag>();
    if (view.begin() != view.end()) {
        DrawInternal(registry, view.front());
    }
}

void UIAstrolabe::DrawInternal(entt::registry& registry, entt::entity player) {
    EnsureLoaded();
    
    // Update alpha
    float dt = GetFrameTime();
    if (s_visible) s_alpha = std::min(1.0f, s_alpha + dt * 5.0f);
    else s_alpha = std::max(0.0f, s_alpha - dt * 5.0f);
    
    if (s_alpha <= 0.0f) return;

    s_view.alpha = s_alpha;
    s_view.time += dt;
    s_view.resolution = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    s_view.camera.offset = { s_view.resolution.x / 2.0f, s_view.resolution.y / 2.0f };
    
    // Update Camera Interaction (only if fully visible or fading in)
    if (s_alpha > 0.1f) {
        // Pan with Right Mouse Button
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            Vector2 delta = GetMouseDelta();
            s_view.camera.target = Vector2Add(s_view.camera.target, Vector2Scale(delta, -1.0f / s_view.camera.zoom));
        }
        
        // Zoom with Mouse Wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            using namespace Constants::Astrolabe;
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), s_view.camera);
            s_view.camera.offset = GetMousePosition();
            s_view.camera.target = mouseWorldPos;
            s_view.camera.zoom += wheel * ZOOM_SPEED * s_view.camera.zoom;
            s_view.camera.zoom = std::clamp(s_view.camera.zoom, MIN_ZOOM, MAX_ZOOM);
        }

        // Center view with 'N' key if already open
        if (IsKeyPressed(KEY_N)) {
            ResetView();
        } 

        // --- Galaxy Debug ---
        // (Removed F5-F7 debug switches)
    }
    
    // Hit Test (Pre-Calculation for Draw)
    Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), s_view.camera);
    const StarNode* hoveredNode = nullptr;
    uint32_t hoverId = 0;

    for (const auto& [id, star] : s_map.stars) {
        float r = 12.0f; 
        if (star.type == StarNodeType::Major) r = 18.0f;
        else if (star.type == StarNodeType::Keystone) r = 28.0f;
        
        if (CheckCollisionPointCircle(mouseWorld, {star.x, star.y}, r)) {
            hoveredNode = &star;
            hoverId = id;
            break;
        }
    }

    // Draw Background and Stars
    static const std::set<uint32_t> emptySet;
    auto* astroComp = registry.try_get<AstrolabeComponent>(player);
    AstrolabeRenderer::Draw(s_map, s_view, astroComp ? astroComp->activated_nodes : emptySet, hoverId);
    
    // Draw UI Overlay
    float scale = UISystem::State.scaleFactor;
    if (astroComp) {
        UISystem::DrawTextUI(TextFormat("可用星尘: %d", astroComp->available_points), 50, 50, 30, GOLD, s_alpha);
    }
    
    // Back Button
    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
    Rectangle backRect = { (float)GetScreenWidth() - 180 * scale, 40 * scale, 140 * scale, 50 * scale };
    bool backHover = CheckCollisionPointRec(GetMousePosition(), backRect);
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, backRect, "返回 [ESC]", 20, WHITE, WHITE, backHover, backHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), s_alpha);
    
    if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Hide();
    }
    

    
    if (hoveredNode) {
        const auto& star = *hoveredNode;
        bool isActivated = astroComp && astroComp->activated_nodes.contains(hoverId);
        
        // Tooltip
        Vector2 mousePos = GetMousePosition();
        float tw = 350 * scale;
        float th = 120 * scale;
        
        if (mousePos.x + tw + 20 > GetScreenWidth()) mousePos.x -= (tw + 40);
        if (mousePos.y + th + 20 > GetScreenHeight()) mousePos.y -= (th + 40);

        DrawRectangleRec({mousePos.x + 20, mousePos.y + 20, tw, th}, Fade(BLACK, 0.9f * s_alpha));
        DrawRectangleLinesEx({mousePos.x + 20, mousePos.y + 20, tw, th}, 1.0f, Fade(GOLD, s_alpha));
        
        UIRenderer::DrawTextUI(UISystem::GetFont(), star.name_key.c_str(), mousePos.x + 40, mousePos.y + 40, 24 * scale, isActivated ? SKYBLUE : GOLD, s_alpha);
        UIRenderer::DrawTextScaled(UISystem::GetFont(), star.desc_key.c_str(), mousePos.x + 40, mousePos.y + 75, 18 * scale, tw - 40, WHITE, s_alpha);
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && astroComp && !isActivated) {
            if (astroComp->available_points > 0) {
                bool canUnlock = true;
                for (uint32_t preId : star.prerequisites) {
                    if (!astroComp->activated_nodes.contains(preId)) {
                        canUnlock = false; 
                        break;
                    }
                }
                
                if (canUnlock) {
                    astroComp->activated_nodes.insert(hoverId);
                    astroComp->available_points--;
                    AttributePipeline::Calculate(registry, player); 
                    LOG_INFO("Activated Astrolabe Node: {}", star.name_key);
                }
            }
        }
    }
}

void UIAstrolabe::Show() { s_visible = true; }
void UIAstrolabe::Hide() { s_visible = false; }

} // namespace NoMoreDay