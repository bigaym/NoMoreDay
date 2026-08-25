#include "game/application/ui/AstrolabeController.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/foundation/data/AstrolabeConstants.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/UIAssetRegistry.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "engine/render/CoordSystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay::ui {

AstrolabeController::AstrolabeController(UiRuntime& runtime) : m_runtime(runtime) {
  UiNodeDesc desc;
  desc.id = entt::hashed_string("ui_astrolabe");
  desc.parent = kRootUiId;
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.visible = true;
  desc.hitTestVisible = false;
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Panels);
  desc.customPainter = kInvalidUiResourceId;
  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
    // Hidden until Toggle/EnterGameplay; mirrors the panel default.
    (void)m_runtime.SetNodeVisible(m_rootNodeId, false);
  }
}

void AstrolabeController::SetHost(GameUiHost* host) noexcept {
  m_uiHost = host;
}

void AstrolabeController::Initialize() {
    if (m_loaded) return;

    // Load data via Registry
    if (!AstrolabeRegistry::Get().Load()) {
        LOG_WARN("AstrolabeController: Failed to load profession talents.");
    }

    // Initialize Renderer
    Shader galaxyShader = AssetLoadingSystem::GetShader(assets::shaders::Galaxy_Procedural.id);
    Shader nodeShader = AssetLoadingSystem::GetShader(assets::shaders::Talent_Node.id);
    m_renderer.Init(galaxyShader, nodeShader);

    // Initialize View
    m_view.resolution = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    m_view.camera.offset = { m_view.resolution.x / 2.0f, m_view.resolution.y / 2.0f };
    m_view.camera.rotation = 0.0f;
    ResetView();

    m_loaded = true;
}

void AstrolabeController::EnsureLoaded() {
    if (!m_loaded) Initialize();
}

void AstrolabeController::ResetView() {
    using namespace Constants::Astrolabe;
    m_view.camera.target = { 0, 0 };
    m_view.camera.zoom = INITIAL_ZOOM;
}

// R8: interaction phase. Reads only the frame snapshot + UiInputFrame; the
// interaction pre-checks rebuild a throwaway component copy from the snapshot
// and enqueue intents for the authoritative writes.
void AstrolabeController::Update(const GameUiSnapshot& snapshot,
                                 const UiInputFrame& input) {
    if (!m_visible && m_alpha <= 0.0f) return;

    EnsureLoaded();

    const float dt = input.deltaSeconds;
    if (m_visible) m_alpha = std::min(1.0f, m_alpha + dt * 5.0f);
    else m_alpha = std::max(0.0f, m_alpha - dt * 5.0f);

    if (m_alpha <= 0.0f) return;

    m_view.alpha = m_alpha;
    m_view.time += dt;
    m_view.resolution = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    // FIX: Update camera offset on resize
    m_view.camera.offset = { m_view.resolution.x / 2.0f, m_view.resolution.y / 2.0f };

    // R8: painter payload refreshed each frame (snapshot is frame-scoped).
    m_paintState.snapshot = &snapshot;

    // Input Handling
    if (m_alpha > 0.1f) {
        HandleCameraInput(input);
    }

    HandleInteraction(snapshot, input);
}

void AstrolabeController::HandleCameraInput(const UiInputFrame& input) {
    using namespace Constants::Astrolabe;
    const UiVec2 mouseLogical = input.pointer.logicalPosition;
    const float scale = UISystem::GetScaleFactor();

    // Camera pan: right button held. The legacy GetMouseDelta is replaced by
    // the logical-mouse delta (input already in the 2K reference space).
    static UiVec2 s_lastLogical = {};
    if (input.pointer.rightDown) {
        UiVec2 delta = { mouseLogical.x - s_lastLogical.x,
                         mouseLogical.y - s_lastLogical.y };
        m_view.camera.target = Vector2Add(
            m_view.camera.target,
            Vector2Scale({ delta.x * scale, delta.y * scale },
                         -1.0f / m_view.camera.zoom));
    }
    s_lastLogical = mouseLogical;

    float wheel = input.pointer.mouseWheel;
    if (wheel != 0.0f) {
        Vector2 screenPos = { mouseLogical.x * scale, mouseLogical.y * scale };
        Vector2 mouseWorldPos = NoMoreDay::render::coord::ScenePixelToWorld(
            NoMoreDay::render::coord::Camera2DTransform::From(m_view.camera),
            screenPos);
        m_view.camera.offset = screenPos;
        m_view.camera.target = mouseWorldPos;
        m_view.camera.zoom += wheel * ZOOM_SPEED * m_view.camera.zoom;
        m_view.camera.zoom = std::clamp(m_view.camera.zoom, MIN_ZOOM, MAX_ZOOM);
    }

    // R8: the in-panel N reset is handled by the host KEY_N handler.
}

// R8: hit test + interaction. Node/star clicks enqueue intents; failure
// messages are UI-local session state kept in the controller.
void AstrolabeController::HandleInteraction(const GameUiSnapshot& snapshot,
                                            const UiInputFrame& input) {
    const float scale = UISystem::GetScaleFactor();
    const UiVec2 mouseLogical = input.pointer.logicalPosition;

    const auto& graph = AstrolabeRegistry::Get().GetGraph();
    AstrolabeComponent temp{};
    const auto& astro = snapshot.astrolabe;
    temp.available_points = astro.availablePoints;
    temp.mainProfession = astro.mainProfession;
    for (int i = 0; i < 6; ++i) temp.professionAffinity[i] = astro.professionAffinity[i];
    for (uint32_t id : astro.activatedNodes) temp.activated_nodes.insert(id);
    for (const auto& [id, pts] : astro.nodePoints) temp.nodePoints[id] = pts;

    // Fail message / vow timers tick in the interaction phase (UI-local state).
    if (m_failMessageTimer > 0.0f) {
        m_failMessageTimer -= input.deltaSeconds;
        if (m_failMessageTimer <= 0.0f) m_failMessage.clear();
    }

    // R8: the "返回 [ESC]" button interaction moved here from DrawOverlay
    // (paint path must not consume input). ESC is handled by the host Escape
    // chain; the button click hides the panel directly.
    {
        const float scale = UISystem::GetScaleFactor();
        Rectangle backRect = { (float)GetScreenWidth() - 180 * scale, 40 * scale, 140 * scale, 50 * scale };
        bool backHover = CheckCollisionPointRec({mouseLogical.x * scale, mouseLogical.y * scale}, backRect);
        if (backHover && input.pointer.pressed) {
            Hide();
            return;
        }
    }

    if (m_showVowDialog) {
        UpdateVowDialog(snapshot, input);
        return;
    }

    if (m_alpha < 0.9f) return;

    // Hit Test (world-space mouse from the logical input + current camera).
    Vector2 mouseScreen = { mouseLogical.x * scale, mouseLogical.y * scale };
    Vector2 mouseWorld = NoMoreDay::render::coord::ScenePixelToWorld(
        NoMoreDay::render::coord::Camera2DTransform::From(m_view.camera),
        mouseScreen);
    uint32_t hoverId = 0;
    const AstrolabeTalentNode* hoveredNode = nullptr;

    for (const auto& [id, node] : graph.nodes) {
        float r = m_renderer.getNodeRadius(node.type) * 1.2f;
        if (CheckCollisionPointCircle(mouseWorld, {node.x, node.y}, r)) {
            hoverId = id;
            hoveredNode = &node;
            break;
        }
    }

    const ProfessionStar* hoveredStar = nullptr;
    int hoveredStarIndex = -1;
    if (!hoveredNode) {
        for (int i = 0; i < (int)graph.professionStars.size(); ++i) {
            const auto& star = graph.professionStars[i];
            float r = Constants::Astrolabe::PROFESSION_STAR_RADIUS * 1.2f;
            if (CheckCollisionPointCircle(mouseWorld, {star.x, star.y}, r)) {
                hoveredStar = &star;
                hoveredStarIndex = i;
                break;
            }
        }
    }

    m_paintState.hoverId = hoverId;
    m_paintState.hoveredStarIndex = hoveredStarIndex;

    // Node Interaction: enqueue AstrolabeAddPoint (handler re-validates and
    // performs the authoritative write + particle effects).
    if (hoveredNode && input.pointer.pressed) {
        int required = 0;
        auto reason = AstrolabeSystem::tryUnlockNode(graph, temp, hoverId, &required);
        if (reason == AstrolabeSystem::UnlockFailReason::Success) {
            if (m_uiHost) {
                GameUiIntent intent;
                intent.sourceNode = m_rootNodeId;
                intent.kind = GameUiIntentKind::AstrolabeAddPoint;
                intent.payload.astrolabeNodeId = hoverId;
                m_uiHost->EnqueueIntent(std::move(intent));
            }
        } else {
            switch (reason) {
                case AstrolabeSystem::UnlockFailReason::NoPoints:
                    m_failMessage = "星尘不足!";
                    break;
                case AstrolabeSystem::UnlockFailReason::TierLocked:
                    m_failMessage = TextFormat("需要 %d 点亲和度 (当前: %d)",
                        required, temp.getAffinity(hoveredNode->profession));
                    break;
                case AstrolabeSystem::UnlockFailReason::CoreSealed:
                    m_failMessage = "核心节点需先立下誓约!";
                    break;
                case AstrolabeSystem::UnlockFailReason::MaxPointsReached:
                    m_failMessage = "节点已达上限!";
                    break;
                default:
                    m_failMessage = "";
            }
            m_failMessageTimer = 2.0f;
        }
    }

    // Vow Interaction: open the (UI-local) confirmation dialog.
    if (hoveredStar && input.pointer.pressed && !temp.hasVow()) {
        m_pendingVowProfession = hoveredStar->profession;
        m_showVowDialog = true;
        m_vowHoldProgress = 0.0f;
    }
}

// R8: hold-to-confirm state machine (UI-local). Completion enqueues
// AstrolabeTakeVow; the authoritative write happens in the command handler.
void AstrolabeController::UpdateVowDialog(const GameUiSnapshot& snapshot,
                                          const UiInputFrame& input) {
    (void)snapshot;
    const float scale = UISystem::GetScaleFactor();
    const UiVec2 mouseLogical = input.pointer.logicalPosition;
    Vector2 mouseScreen = { mouseLogical.x * scale, mouseLogical.y * scale };

    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();

    // Dialog size
    float w = 550.0f * scale;
    float h = 320.0f * scale;
    float x = (screenWidth - w) / 2;
    float y = (screenHeight - h) / 2;

    // Hold to confirm button
    Rectangle confirmBtn = {x + w / 2 - 180 * scale, y + h - 100 * scale, 360 * scale, 60 * scale};
    bool hover = CheckCollisionPointRec(mouseScreen, confirmBtn);

    // Cancel button
    Rectangle cancelBtn = {x + w / 2 - 60 * scale, y + h - 35 * scale, 120 * scale, 30 * scale};
    bool cancelHover = CheckCollisionPointRec(mouseScreen, cancelBtn);

    if (cancelHover && input.pointer.pressed) {
        m_showVowDialog = false;
        m_vowHoldProgress = 0.0f;
        return;
    }

    // Hold logic
    if (hover && input.pointer.down) {
        m_vowHoldProgress += input.deltaSeconds / VOW_HOLD_DURATION;
        if (m_vowHoldProgress >= 1.0f) {
            if (m_uiHost) {
                GameUiIntent intent;
                intent.sourceNode = m_rootNodeId;
                intent.kind = GameUiIntentKind::AstrolabeTakeVow;
                intent.payload.professionId = static_cast<uint8_t>(m_pendingVowProfession);
                m_uiHost->EnqueueIntent(std::move(intent));
            }
            m_showVowDialog = false;
            m_vowHoldProgress = 0.0f;
        }
    } else {
        m_vowHoldProgress = std::max(0.0f, m_vowHoldProgress - input.deltaSeconds * 2.0f);  // Fast decay
    }
}

void AstrolabeController::Paint(UiDrawList& drawList, const UiViewport& viewport) {
    if (!m_visible && m_alpha <= 0.0f) return;
    drawList.Custom(UiDrawLayer::Panels, m_rootNodeId,
                    {0.0f, 0.0f, viewport.LogicalSize().x, viewport.LogicalSize().y},
                    kAstrolabePainterResourceId);
}

// R8: registered custom-painter target. The painter callback and tech tests
// reach the full canvas render through this public entry (the private
// DrawInternal stays the implementation detail).
void AstrolabeController::PaintCanvas(UiRect nativeBounds) {
    (void)nativeBounds;
    DrawInternal();
}

// Backend painter callback: userData is the controller. Draws the full panel
// through the AstrolabeRenderer (raylib confined to the painter, per design
// §3.4). The component snapshot copy is rebuilt from m_paintState.
void AstrolabeController::DrawInternal() {
    EnsureLoaded();
    if (m_alpha <= 0.0f) return;

    const auto& graph = AstrolabeRegistry::Get().GetGraph();
    AstrolabeComponent temp{};
    if (m_paintState.snapshot) {
        const auto& astro = m_paintState.snapshot->astrolabe;
        temp.available_points = astro.availablePoints;
        temp.mainProfession = astro.mainProfession;
        for (int i = 0; i < 6; ++i) temp.professionAffinity[i] = astro.professionAffinity[i];
        for (uint32_t id : astro.activatedNodes) temp.activated_nodes.insert(id);
        for (const auto& [id, pts] : astro.nodePoints) temp.nodePoints[id] = pts;
    }

    // Layer 1: Base Rendering
    m_renderer.Draw(graph, m_view, &temp, m_paintState.hoverId);

    // Layer 3: UI Overlay
    float scale = UISystem::GetScaleFactor();
    DrawOverlay(scale);

    // Layer 4: Tooltips
    DrawTooltips(scale);

    // Vow Dialog (Modal)
    if (m_showVowDialog && !temp.hasVow()) {
        DrawVowDialog(scale);
    }
}

void AstrolabeController::DrawOverlay(float scale) {
    if (m_paintState.snapshot) {
        const auto& astro = m_paintState.snapshot->astrolabe;
        UISystem::DrawTextUI(TextFormat("可用星尘: %d", astro.availablePoints), 50, 50, 30, GOLD, m_alpha);
        UISystem::DrawTextUI(TextFormat("剑修亲和: %d",
            astro.professionAffinity[(int)ProfessionID::BladeAscendant]), 50, 100, 20, WHITE, m_alpha);
    }

    Texture2D rectTex = AssetLoadingSystem::GetTexture(assets::ui::textures::Button_Frost_Rect.id);
    Rectangle backRect = { (float)GetScreenWidth() - 180 * scale, 40 * scale, 140 * scale, 50 * scale };
    bool backHover = CheckCollisionPointRec(GetMousePosition(), backRect);
    UIRenderer::DrawButton(UISystem::GetFont(), rectTex, backRect, "返回 [ESC]", 20, WHITE, WHITE, backHover, backHover && IsMouseButtonDown(MOUSE_LEFT_BUTTON), m_alpha);

    // R8: the back click-to-hide moved to the interaction phase (host ESC
    // already closes the panel; this button is a paint-time affordance only).

    // Fail Message
    if (m_failMessageTimer > 0.0f) {
        float msgAlpha = std::min(1.0f, m_failMessageTimer);
        Font font = UISystem::GetFont();
        Vector2 textSize = MeasureTextEx(font, m_failMessage.c_str(), 24 * scale, 1);
        float x = ((float)GetScreenWidth() - textSize.x) / 2;
        float y = (float)GetScreenHeight() * 0.75f;

        DrawRectangleRec({x - 20 * scale, y - 10 * scale, textSize.x + 40 * scale, textSize.y + 20 * scale},
                         Fade(MAROON, 0.8f * msgAlpha * m_alpha));
        UIRenderer::DrawTextUI(font, m_failMessage.c_str(), x, y, 24 * scale, Fade(WHITE, msgAlpha * m_alpha), m_alpha);
    }
}

void AstrolabeController::DrawTooltips(float scale) {
    if (m_showVowDialog) return;
    if (!m_paintState.snapshot) return;

    const auto& graph = AstrolabeRegistry::Get().GetGraph();
    AstrolabeComponent temp{};
    const auto& astro = m_paintState.snapshot->astrolabe;
    temp.available_points = astro.availablePoints;
    temp.mainProfession = astro.mainProfession;
    for (int i = 0; i < 6; ++i) temp.professionAffinity[i] = astro.professionAffinity[i];
    for (uint32_t id : astro.activatedNodes) temp.activated_nodes.insert(id);
    for (const auto& [id, pts] : astro.nodePoints) temp.nodePoints[id] = pts;

    const AstrolabeTalentNode* hoveredNode = nullptr;
    const ProfessionStar* hoveredStar = nullptr;
    uint32_t hoverId = m_paintState.hoverId;
    if (hoverId != 0) {
        auto it = graph.nodes.find(hoverId);
        if (it != graph.nodes.end()) hoveredNode = &it->second;
    }
    if (!hoveredNode && m_paintState.hoveredStarIndex >= 0 &&
        m_paintState.hoveredStarIndex < (int)graph.professionStars.size()) {
        hoveredStar = &graph.professionStars[m_paintState.hoveredStarIndex];
    }
    if (!hoveredNode && !hoveredStar) return;

    Vector2 mousePos = GetMousePosition();
    float tw = 350 * scale;
    float th = 160 * scale; // Increased height for vertical layout

    if (mousePos.x + tw + 20 > GetScreenWidth()) mousePos.x -= (tw + 40);
    if (mousePos.y + th + 20 > GetScreenHeight()) mousePos.y -= (th + 40);

    float padding = 20;
    float startX = mousePos.x + padding;
    float startY = mousePos.y + padding;
    float contentX = startX + 20 * scale;
    float currentY = startY + 20 * scale;

    if (hoveredNode) {
        DrawRectangleRec({startX, startY, tw, th}, Fade(BLACK, 0.95f * m_alpha));
        DrawRectangleLinesEx({startX, startY, tw, th}, 1.0f, Fade(GOLD, m_alpha));

        // 1. Name
        UIRenderer::DrawTextUI(UISystem::GetFont(), hoveredNode->name_key.c_str(), contentX, currentY, 24 * scale, GOLD, m_alpha);
        currentY += 35 * scale;

        // 2. Description
        UIRenderer::DrawTextUI(UISystem::GetFont(), hoveredNode->desc_key.c_str(), contentX, currentY, 16 * scale, LIGHTGRAY, m_alpha);
        currentY += 30 * scale;

        // 3. Status
        auto status = AstrolabeSystem::getNodeStatus(graph, temp, hoverId);
        const char* statusText = "未解锁";
        Color statusColor = GRAY;
        switch (status) {
            case AstrolabeSystem::NodeStatus::Available: statusText = "可解锁"; statusColor = GREEN; break;
            case AstrolabeSystem::NodeStatus::Activated: statusText = "已激活"; statusColor = SKYBLUE; break;
            case AstrolabeSystem::NodeStatus::FullyActivated: statusText = "已满级"; statusColor = GOLD; break;
            case AstrolabeSystem::NodeStatus::Sealed: statusText = "被封印"; statusColor = PURPLE; break;
            default: break;
        }

        UIRenderer::DrawTextUI(UISystem::GetFont(), statusText, contentX, currentY, 18 * scale, statusColor, m_alpha);
        currentY += 25 * scale;

        // 4. Points
        int pts = temp.getNodePoints(hoverId);
        UIRenderer::DrawTextUI(UISystem::GetFont(), TextFormat("投入点数: %d / %d", pts, hoveredNode->maxPoints), contentX, currentY, 18 * scale, WHITE, m_alpha);
    }
    else if (hoveredStar) {
        DrawRectangleRec({startX, startY, tw, th}, Fade(BLACK, 0.95f * m_alpha));
        DrawRectangleLinesEx({startX, startY, tw, th}, 1.0f, Fade(SKYBLUE, m_alpha));

        UIRenderer::DrawTextUI(UISystem::GetFont(), hoveredStar->name_key.c_str(), contentX, currentY, 24 * scale, SKYBLUE, m_alpha);
        currentY += 35 * scale;

        UIRenderer::DrawTextUI(UISystem::GetFont(), hoveredStar->desc_key.c_str(), contentX, currentY, 16 * scale, WHITE, m_alpha);
        currentY += 30 * scale;

        const char* statusText = "可解锁";
        Color statusColor = GREEN;
        if (temp.hasVow()) {
            if (temp.isMainProfession(hoveredStar->profession)) {
                statusText = "主修职业";
                statusColor = GOLD;
            } else {
                statusText = "被封印";
                statusColor = PURPLE;
            }
        } else {
            statusText = "点击立誓";
            statusColor = YELLOW;
        }
        UIRenderer::DrawTextUI(UISystem::GetFont(), statusText, contentX, currentY, 16 * scale, statusColor, m_alpha);
    }
}

void AstrolabeController::DrawVowDialog(float scale) {
    Font font = UISystem::GetFont();
    float screenWidth = (float)GetScreenWidth();
    float screenHeight = (float)GetScreenHeight();

    // Dialog size
    float w = 550.0f * scale;
    float h = 320.0f * scale;
    float x = (screenWidth - w) / 2;
    float y = (screenHeight - h) / 2;

    const auto& graph = AstrolabeRegistry::Get().GetGraph();
    const ProfessionStar& star = graph.professionStars[(int)m_pendingVowProfession];

    // Background
    DrawRectangleRec({x, y, w, h}, Fade(BLACK, 0.95f * m_alpha));
    DrawRectangleLinesEx({x, y, w, h}, 2.0f, Fade(GOLD, m_alpha));

    // Title
    UIRenderer::DrawTextUI(font, "[!] 深渊誓约", x + 30 * scale, y + 30 * scale, 32 * scale, GOLD, m_alpha);

    // Profession Info
    UIRenderer::DrawTextUI(font, TextFormat("你即将与 [%s] 职业建立不可逆转的誓约。", star.name_key.c_str()),
               x + 30 * scale, y + 85 * scale, 22 * scale, WHITE, m_alpha);

    // Warning text
    UIRenderer::DrawTextUI(font, "• 解锁所有该职业的核心天赋", x + 50 * scale, y + 130 * scale, 18 * scale, GREEN, m_alpha);
    UIRenderer::DrawTextUI(font, "• 其他职业的核心天赋将被永久封印", x + 50 * scale, y + 160 * scale, 18 * scale, RED, m_alpha);
    UIRenderer::DrawTextUI(font, "• 誓约一旦立下，不可更改或撤销", x + 50 * scale, y + 190 * scale, 18 * scale, ORANGE, m_alpha);

    // Hold to confirm button
    Rectangle confirmBtn = {x + w / 2 - 180 * scale, y + h - 100 * scale, 360 * scale, 60 * scale};
    bool hover = CheckCollisionPointRec(GetMousePosition(), confirmBtn);

    // Progress bar
    DrawRectangleRec(confirmBtn, Fade(DARKGRAY, 0.8f * m_alpha));
    DrawRectangleRec({confirmBtn.x, confirmBtn.y, confirmBtn.width * m_vowHoldProgress, confirmBtn.height},
                     Fade(GOLD, m_alpha));
    DrawRectangleLinesEx(confirmBtn, 2.0f, Fade(hover ? GOLD : GRAY, m_alpha));

    const char* btnText = m_vowHoldProgress > 0.01f
        ? TextFormat("确认中... (%.1fs)", VOW_HOLD_DURATION * (1.0f - m_vowHoldProgress))
        : "长按此处确认誓约 (2秒)";

    Vector2 textSize = MeasureTextEx(font, btnText, 20 * scale, 1);
    UIRenderer::DrawTextUI(font, btnText, confirmBtn.x + (confirmBtn.width - textSize.x) / 2, confirmBtn.y + (confirmBtn.height - textSize.y) / 2, 20 * scale, WHITE, m_alpha);

    // Cancel button
    Rectangle cancelBtn = {x + w / 2 - 60 * scale, y + h - 35 * scale, 120 * scale, 30 * scale};
    bool cancelHover = CheckCollisionPointRec(GetMousePosition(), cancelBtn);

    const char* cancelText = "[ 取消 ]";
    Vector2 cancelSize = MeasureTextEx(font, cancelText, 18 * scale, 1);
    UIRenderer::DrawTextUI(font, cancelText, cancelBtn.x + (cancelBtn.width - cancelSize.x) / 2, cancelBtn.y, 18 * scale, cancelHover ? YELLOW : GRAY, m_alpha);
}

void AstrolabeController::Toggle() {
    m_visible = !m_visible;
    if (m_visible) {
        EnsureLoaded();
        ResetView();
        // R8: sibling-panel coupling moved to the host KEY_N handler (the
        // host owns the other controllers; no SharedContext registry access).
    }
    SetNodeVisible(m_visible);
}

bool AstrolabeController::IsVisible() const noexcept {
    return m_visible || m_alpha > 0.0f;
}

void AstrolabeController::Show() { m_visible = true; }
void AstrolabeController::Hide() { m_visible = false; }

void AstrolabeController::Close() {
    m_visible = false;
    SetNodeVisible(false);
}

AstrolabeController::VisibilityState AstrolabeController::CaptureVisibilityState() const {
    return {m_visible, m_alpha};
}

void AstrolabeController::RestoreVisibilityState(VisibilityState state) {
    m_visible = state.visible;
    m_alpha = state.alpha;
}

void AstrolabeController::EnterGameplay() {
    m_inGameplay = true;
    m_visible = false;
    m_alpha = 0.0f;
    m_failMessage.clear();
    m_failMessageTimer = 0.0f;
    m_pendingVowProfession = ProfessionID::BladeAscendant;
    m_vowHoldProgress = 0.0f;
    m_showVowDialog = false;
    SetNodeVisible(false);
}

void AstrolabeController::LeaveGameplay() {
    m_visible = false;
    m_alpha = 0.0f;
    m_failMessage.clear();
    m_failMessageTimer = 0.0f;
    m_pendingVowProfession = ProfessionID::BladeAscendant;
    m_vowHoldProgress = 0.0f;
    m_showVowDialog = false;
    SetNodeVisible(false);
    m_inGameplay = false;
}

bool AstrolabeController::IsInGameplay() const noexcept {
    return m_inGameplay;
}

UiId AstrolabeController::NodeId() const noexcept {
    return m_rootNodeId;
}

void AstrolabeController::SetNodeVisible(bool visible) {
    if (m_rootNodeId != kInvalidUiId) {
        (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
    }
}

void AstrolabePaintCallback(void* userData, UiRect nativeBounds) {
    (void)nativeBounds;
    auto* controller = static_cast<AstrolabeController*>(userData);
    if (controller) {
        controller->PaintCanvas(nativeBounds);
    }
}

} // namespace NoMoreDay::ui
