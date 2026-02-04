#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/data/TalentLoader.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "engine/render/UIRenderer.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/Progression.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/stats/AttributePipeline.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay {

AstrolabeView UIAstrolabe::s_view;
bool UIAstrolabe::s_loaded = false;
bool UIAstrolabe::s_visible = false;
float UIAstrolabe::s_alpha = 0.0f;

std::string UIAstrolabe::s_failMessage = "";
float UIAstrolabe::s_failMessageTimer = 0.0f;

ProfessionID UIAstrolabe::s_pendingVowProfession = ProfessionID::BladeAscendant;
float UIAstrolabe::s_vowHoldProgress = 0.0f;
bool UIAstrolabe::s_showVowDialog = false;

void UIAstrolabe::Initialize() {
    if (s_loaded) return;
    
    // Load data via Registry
    if (!AstrolabeRegistry::Get().Load("assets/data/profession_talents.json")) {
        LOG_WARN("UIAstrolabe: Failed to load profession talents.");
    }
    
    // Initialize Renderer
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
    
    // Camera Interaction
    if (s_alpha > 0.1f) {
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            Vector2 delta = GetMouseDelta();
            s_view.camera.target = Vector2Add(s_view.camera.target, Vector2Scale(delta, -1.0f / s_view.camera.zoom));
        }
        
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            using namespace Constants::Astrolabe;
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), s_view.camera);
            s_view.camera.offset = GetMousePosition();
            s_view.camera.target = mouseWorldPos;
            s_view.camera.zoom += wheel * ZOOM_SPEED * s_view.camera.zoom;
            s_view.camera.zoom = std::clamp(s_view.camera.zoom, MIN_ZOOM, MAX_ZOOM);
        }

        if (IsKeyPressed(KEY_N)) ResetView();
    }
    
    const auto& graph = AstrolabeRegistry::Get().GetGraph();
    auto* astroComp = registry.try_get<AstrolabeComponent>(player);
    
    // Hit Test
    Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), s_view.camera);
    uint32_t hoverId = 0;
    const AstrolabeTalentNode* hoveredNode = nullptr;

    // 1. Check Nodes
    for (const auto& [id, node] : graph.nodes) {
        float r = Constants::Astrolabe::NODE_RADIUS_MINOR * 1.2f; 
        if (node.type == TalentNodeType::Major) r = Constants::Astrolabe::NODE_RADIUS_MAJOR * 1.2f;
        else if (node.type == TalentNodeType::Core) r = Constants::Astrolabe::NODE_RADIUS_CORE * 1.2f;
        
        if (CheckCollisionPointCircle(mouseWorld, {node.x, node.y}, r)) {
            hoverId = id;
            hoveredNode = &node;
            break;
        }
    }
    
    // 2. Check Profession Stars
    const ProfessionStar* hoveredStar = nullptr;
    if (!hoveredNode) { // Prioritize nodes
        for (const auto& star : graph.professionStars) {
            float r = Constants::Astrolabe::PROFESSION_STAR_RADIUS * 1.2f;
            if (CheckCollisionPointCircle(mouseWorld, {star.x, star.y}, r)) {
                hoveredStar = &star;
                break;
            }
        }
    }

    AstrolabeRenderer::Draw(graph, s_view, astroComp, hoverId);
    
    // Draw UI Overlay
    float scale = UISystem::State.scaleFactor;
    if (astroComp) {
        UISystem::DrawTextUI(TextFormat("可用星尘: %d", astroComp->available_points), 50, 50, 30, GOLD, s_alpha);
        
        // Debug Affinity display
        UISystem::DrawTextUI(TextFormat("剑修: %d", astroComp->getAffinity(ProfessionID::BladeAscendant)), 50, 100, 20, WHITE, s_alpha);
    }
    
    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
    Rectangle backRect = { (float)GetScreenWidth() - 180 * scale, 40 * scale, 140 * scale, 50 * scale };
    bool backHover = CheckCollisionPointRec(GetMousePosition(), backRect);
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, backRect, "返回 [ESC]", 20, WHITE, WHITE, backHover, backHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), s_alpha);
    
    if (backHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Hide();
    }
    
    // Interaction
    if (!s_showVowDialog) {
        // Interaction - Node
        if (hoveredNode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && astroComp && s_alpha > 0.9f) {
            int required = 0;
            auto reason = AstrolabeSystem::tryUnlockNode(graph, *astroComp, hoverId, &required);
            
            if (reason == AstrolabeSystem::UnlockFailReason::Success) {
                AstrolabeSystem::addPointToNode(registry, player, graph, hoverId);
            } else {
                switch(reason) {
                    case AstrolabeSystem::UnlockFailReason::NoPoints:
                        s_failMessage = "星尘不足!";
                        break;
                    case AstrolabeSystem::UnlockFailReason::TierLocked:
                        s_failMessage = TextFormat("需要 %d 点亲和度 (当前: %d)", 
                            required, astroComp->getAffinity(hoveredNode->profession));
                        break;
                    case AstrolabeSystem::UnlockFailReason::CoreSealed:
                        s_failMessage = "核心节点需先立下誓约!";
                        break;
                    case AstrolabeSystem::UnlockFailReason::MaxPointsReached:
                        s_failMessage = "节点已达上限!";
                        break;
                    default:
                        s_failMessage = "";
                }
                s_failMessageTimer = 2.0f;
            }
        }
        
        // Interaction - Profession Star (Take Vow)
        if (hoveredStar && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && astroComp && s_alpha > 0.9f) {
            if (!astroComp->hasVow()) {
                s_pendingVowProfession = hoveredStar->profession;
                s_showVowDialog = true;
                s_vowHoldProgress = 0.0f;
            }
        }
    }

    // Failure Message Rendering
    if (s_failMessageTimer > 0.0f) {
        s_failMessageTimer -= dt;
        float msgAlpha = std::min(1.0f, s_failMessageTimer);
        Font font = UISystem::GetFont();
        
        Vector2 textSize = MeasureTextEx(font, s_failMessage.c_str(), 24 * scale, 1);
        float x = ((float)GetScreenWidth() - textSize.x) / 2;
        float y = (float)GetScreenHeight() * 0.75f;
        
        DrawRectangleRec({x - 20 * scale, y - 10 * scale, textSize.x + 40 * scale, textSize.y + 20 * scale}, 
                         Fade(MAROON, 0.8f * msgAlpha * s_alpha));
        UIRenderer::DrawTextUI(font, s_failMessage.c_str(), x, y, 24 * scale, Fade(WHITE, msgAlpha * s_alpha), s_alpha);
    }
    
    // Tooltip - Node
    Vector2 mousePos = GetMousePosition();
    float tw = 350 * scale;
    float th = 120 * scale;
    
    if (mousePos.x + tw + 20 > GetScreenWidth()) mousePos.x -= (tw + 40);
    if (mousePos.y + th + 20 > GetScreenHeight()) mousePos.y -= (th + 40);

    if (hoveredNode && !s_showVowDialog) {
        DrawRectangleRec({mousePos.x + 20, mousePos.y + 20, tw, th}, Fade(BLACK, 0.9f * s_alpha));
        DrawRectangleLinesEx({mousePos.x + 20, mousePos.y + 20, tw, th}, 1.0f, Fade(GOLD, s_alpha));
        
        // Use generic text for now
        const char* name = hoveredNode->name_key.c_str(); // Assume it's literal for prototype
        
        UIRenderer::DrawTextUI(UISystem::GetFont(), name, mousePos.x + 40, mousePos.y + 40, 24 * scale, GOLD, s_alpha);
        
        auto status = AstrolabeSystem::getNodeStatus(graph, *astroComp, hoverId);
        const char* statusText = "Unknown";
        Color statusColor = GRAY;
        switch(status) {
            case AstrolabeSystem::NodeStatus::Locked: statusText = "Locked"; break;
            case AstrolabeSystem::NodeStatus::Available: statusText = "Available"; statusColor = GREEN; break;
            case AstrolabeSystem::NodeStatus::Activated: statusText = "Activated"; statusColor = SKYBLUE; break;
            case AstrolabeSystem::NodeStatus::FullyActivated: statusText = "Maxed"; statusColor = GOLD; break;
            case AstrolabeSystem::NodeStatus::Sealed: statusText = "Sealed"; statusColor = PURPLE; break;
        }
        
        UIRenderer::DrawTextUI(UISystem::GetFont(), statusText, mousePos.x + 40, mousePos.y + 70, 18 * scale, statusColor, s_alpha);
        
        int pts = astroComp->getNodePoints(hoverId);
        UIRenderer::DrawTextUI(UISystem::GetFont(), TextFormat("Points: %d / %d", pts, hoveredNode->maxPoints), mousePos.x + 250, mousePos.y + 40, 20 * scale, WHITE, s_alpha);
    }
    // Tooltip - Profession Star
    else if (hoveredStar && !s_showVowDialog) {
        DrawRectangleRec({mousePos.x + 20, mousePos.y + 20, tw, th}, Fade(BLACK, 0.9f * s_alpha));
        DrawRectangleLinesEx({mousePos.x + 20, mousePos.y + 20, tw, th}, 1.0f, Fade(SKYBLUE, s_alpha));
        
        UIRenderer::DrawTextUI(UISystem::GetFont(), hoveredStar->name_key.c_str(), mousePos.x + 40, mousePos.y + 40, 24 * scale, SKYBLUE, s_alpha);
        UIRenderer::DrawTextUI(UISystem::GetFont(), hoveredStar->desc_key.c_str(), mousePos.x + 40, mousePos.y + 70, 16 * scale, WHITE, s_alpha);
        
        const char* statusText = "Available";
        Color statusColor = GREEN;
        
        if (astroComp && astroComp->hasVow()) {
            if (astroComp->isMainProfession(hoveredStar->profession)) {
                statusText = "Main Profession";
                statusColor = GOLD;
            } else {
                statusText = "Sealed (Only one Main)";
                statusColor = PURPLE;
            }
        } else {
            statusText = "Click to Vow (Irreversible)";
            statusColor = YELLOW;
        }
        
        UIRenderer::DrawTextUI(UISystem::GetFont(), statusText, mousePos.x + 40, mousePos.y + 95, 16 * scale, statusColor, s_alpha);
    }

    // Vow Dialog
    if (s_showVowDialog && astroComp && !astroComp->hasVow()) {
        const auto& star = graph.professionStars[(int)s_pendingVowProfession];
        DrawVowDialog(registry, player, star);
    }
}

void UIAstrolabe::DrawVowDialog(entt::registry& registry, entt::entity player, const ProfessionStar& star) {
    float scale = UISystem::State.scaleFactor;
    Font font = UISystem::GetFont();
    float dt = GetFrameTime();
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();

    // Dialog size
    float w = 550.0f * scale;
    float h = 320.0f * scale;
    float x = (screenWidth - w) / 2;
    float y = (screenHeight - h) / 2;
    
    // Background
    DrawRectangleRec({x, y, w, h}, Fade(BLACK, 0.95f * s_alpha));
    DrawRectangleLinesEx({x, y, w, h}, 2.0f, Fade(GOLD, s_alpha));
    
    // Title
    UIRenderer::DrawTextUI(font, "⚠️ 深渊凝视 (The Vow)", x + 30 * scale, y + 30 * scale, 32 * scale, GOLD, s_alpha);
    
    // Profession Info
    UIRenderer::DrawTextUI(font, TextFormat("你即将与 [%s] 职业建立不可逆转的誓约。", star.name_key.c_str()), 
               x + 30 * scale, y + 85 * scale, 22 * scale, WHITE, s_alpha);
    
    // Warning text
    UIRenderer::DrawTextUI(font, "• 解锁所有该职业的核心天赋 (Core Nodes)", x + 50 * scale, y + 130 * scale, 18 * scale, GREEN, s_alpha);
    UIRenderer::DrawTextUI(font, "• 其他职业的核心天赋将被永久封印", x + 50 * scale, y + 160 * scale, 18 * scale, RED, s_alpha);
    UIRenderer::DrawTextUI(font, "• 誓约一旦立下，不可更改或撤销", x + 50 * scale, y + 190 * scale, 18 * scale, ORANGE, s_alpha);
    
    // Hold to confirm button
    Rectangle confirmBtn = {x + w/2 - 180*scale, y + h - 100*scale, 360*scale, 60*scale};
    bool hover = CheckCollisionPointRec(GetMousePosition(), confirmBtn);
    
    // Progress bar
    DrawRectangleRec(confirmBtn, Fade(DARKGRAY, 0.8f * s_alpha));
    DrawRectangleRec({confirmBtn.x, confirmBtn.y, confirmBtn.width * s_vowHoldProgress, confirmBtn.height}, 
                     Fade(GOLD, s_alpha));
    DrawRectangleLinesEx(confirmBtn, 2.0f, Fade(hover ? GOLD : GRAY, s_alpha));
    
    const char* btnText = s_vowHoldProgress > 0.01f 
        ? TextFormat("确认中... (%.1fs)", VOW_HOLD_DURATION * (1.0f - s_vowHoldProgress))
        : "长按此处确认誓约 (2秒)";
    
    Vector2 textSize = MeasureTextEx(font, btnText, 20 * scale, 1);
    UIRenderer::DrawTextUI(font, btnText, confirmBtn.x + (confirmBtn.width - textSize.x)/2, confirmBtn.y + (confirmBtn.height - textSize.y)/2, 20 * scale, WHITE, s_alpha);
    
    // Cancel button
    Rectangle cancelBtn = {x + w/2 - 60*scale, y + h - 35*scale, 120*scale, 30*scale};
    bool cancelHover = CheckCollisionPointRec(GetMousePosition(), cancelBtn);
    
    const char* cancelText = "[ 取消 ]";
    Vector2 cancelSize = MeasureTextEx(font, cancelText, 18 * scale, 1);
    UIRenderer::DrawTextUI(font, cancelText, cancelBtn.x + (cancelBtn.width - cancelSize.x)/2, cancelBtn.y, 18 * scale, cancelHover ? YELLOW : GRAY, s_alpha);
    
    if (cancelHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        s_showVowDialog = false;
        s_vowHoldProgress = 0.0f;
    }
    
    // Hold logic
    if (hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        s_vowHoldProgress += dt / VOW_HOLD_DURATION;
        if (s_vowHoldProgress >= 1.0f) {
            AstrolabeSystem::takeVow(registry, player, star.profession);
            s_showVowDialog = false;
            s_vowHoldProgress = 0.0f;
        }
    } else {
        s_vowHoldProgress = std::max(0.0f, s_vowHoldProgress - dt * 2.0f);  // Fast decay
    }
}

void UIAstrolabe::Show() { s_visible = true; }
void UIAstrolabe::Hide() { s_visible = false; }

} // namespace NoMoreDay