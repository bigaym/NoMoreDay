#include "UISystem.hpp"
#include "UIInventory.hpp"
#include "UICharacter.hpp"
#include "UIMinimap.hpp"
#include "UIAnimationSystem.hpp" // Include UIAnimationSystem
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../components/InventoryComponent.hpp"
#include "../components/UIAnimationComponent.hpp"
#include "../core/LevelManager.hpp"
#include "../systems/InventorySystem.hpp"
#include "../systems/ProgressionSystem.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/UIAssetRegistry.hpp"
#include "../core/ItemFactory.hpp"
#include "../core/LootFilter.hpp"
#include "../tools/Logger.hpp"
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath> // For std::min

using namespace NoMoreDay;

// --- Static Member Initialization ---
NoMoreDay::UIContext UISystem::State;
static bool s_hasGivenTestItems = false; 

// --- Lifecycle ---

void UISystem::Initialize(ResourceManager& resourceManager) {
    AssetLoadingSystem::Initialize(resourceManager);
    
    // Initialize Animations
    State.inventorySlotAnims.assign(100, {0.0f, 1.0f}); // Assume max 100 slots for now
    State.equipmentSlotAnims.assign(15, {0.0f, 1.0f});
    State.bagSlotAnims.assign(4, {0.0f, 1.0f});

#ifdef TEST_HEADLESS
    LOG_INFO("UISystem: Headless mode, skipping font loading.");
    State.globalFont = GetFontDefault();
    return;
#endif

    std::vector<int> codepoints;
    for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
    for (int i = 0x3000; i <= 0x303F; ++i) codepoints.push_back(i); 
    for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i); 
    for (int i = 0xFF00; i <= 0xFFEF; ++i) codepoints.push_back(i); 

    const auto& mainFont = assets::ui::fonts::Main_Chinese;

    std::vector<std::string> fontCandidates;
    fontCandidates.push_back("C:/Windows/Fonts/simhei.ttf"); 
    fontCandidates.push_back("C:/Windows/Fonts/msyh.ttc");   
    fontCandidates.push_back("C:/Windows/Fonts/simsun.ttc"); 

    for (const auto& path : fontCandidates) {
        if (FileExists(path.c_str())) {
            LOG_INFO("UISystem: Attempting to load font from '{}'...", path);
            State.globalFont = resourceManager.loadFont(mainFont.id, path, mainFont.defaultSize, codepoints.data(), (int)codepoints.size());
            
            if (State.globalFont.texture.id != 0) {
                SetTextureFilter(State.globalFont.texture, TEXTURE_FILTER_BILINEAR);
                LOG_INFO("UISystem: Successfully loaded Chinese font from '{}'", path);
                return;
            } else {
                LOG_WARN("UISystem: Failed to load font from '{}', trying next candidate...", path);
            }
        }
    }
    
    LOG_ERROR("UISystem: All Chinese font candidates failed. Falling back to default font (??? for Chinese).");
    if (State.globalFont.texture.id == 0) State.globalFont = GetFontDefault();
}

void UISystem::Shutdown() {
    State.globalFont = { 0 }; 
    UIMinimap::Cleanup();
    AssetLoadingSystem::Shutdown();
}

// --- Helper ---

Vector2 UISystem::GetMousePositionLogic() {
    Vector2 m = GetMousePosition();
    float s = State.scaleFactor;
    if (s <= 0.0001f) s = 1.0f;
    return { m.x / s, m.y / s };
}

// --- Main Loop ---

void UISystem::Update(entt::registry& registry, const LevelManager& levelManager) {
    float dt = GetFrameTime();
    
    // 0. Update Animation System
    UIAnimationSystem::Update(registry, dt);

    // Transition Panel Alphas
    float alphaSpeed = 6.0f; 
    if (State.showInventory) State.inventoryAlpha = std::min(1.0f, State.inventoryAlpha + dt * alphaSpeed);
    else State.inventoryAlpha = std::max(0.0f, State.inventoryAlpha - dt * alphaSpeed);
    
    if (State.showCharacterPanel) State.characterPanelAlpha = std::min(1.0f, State.characterPanelAlpha + dt * alphaSpeed);
    else State.characterPanelAlpha = std::max(0.0f, State.characterPanelAlpha - dt * alphaSpeed);

    // 1. Global Hotkeys
    
    // Character Panel (C)
    if (IsKeyPressed(KEY_C)) {
        State.showCharacterPanel = !State.showCharacterPanel;
        if (!State.showCharacterPanel) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end()) {
                auto& ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
                ui.tempStr = ui.tempDex = ui.tempInt = ui.tempVit = 0;
                ui.showConfirmPopup = false;
            }
        }
        State.showContextMenu = false;
    }

    // Inventory (I / Tab)
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
        UIInventory::Toggle();
    }

    // Quick Sort (Z)
    if (IsKeyPressed(KEY_Z)) {
        auto playerView = registry.view<PlayerTag>();
        if (playerView.begin() != playerView.end()) {
            InventorySystem::organize(registry, playerView.front());
        }
    }

    // Quick Pickup (F)
    if (IsKeyPressed(KEY_F)) {
        auto playerView = registry.view<PlayerTag, Position>();
        if (playerView.begin() != playerView.end()) {
            auto playerEntity = playerView.front();
            const auto& pPos = playerView.get<Position>(playerEntity);
            
            auto groundItemView = registry.view<ItemComponent, Position>();
            entt::entity nearestItem = entt::null;
            float nearestDistSq = 150.0f * 150.0f; // Max pickup range

            for (auto entity : groundItemView) {
                const auto& iPos = groundItemView.get<Position>(entity);
                float dx = iPos.x - pPos.x;
                float dy = iPos.y - pPos.y;
                float distSq = dx*dx + dy*dy;
                
                if (distSq < nearestDistSq) {
                    nearestDistSq = distSq;
                    nearestItem = entity;
                }
            }
            
            if (nearestItem != entt::null) {
                if (InventorySystem::pickUpItem(registry, playerEntity, nearestItem)) {
                    // Success
                } else {
                    State.showMessageBox = true;
                    snprintf(State.messageBoxText, 64, "背包已满");
                    State.messageBoxTimer = 1.5f;
                }
            }
        }
    }

    // ESC Handling
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (State.showQuantityPopup) {
            State.showQuantityPopup = false;
        } else if (State.showCharacterPanel) {
            bool popupHandled = false;
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end()) {
                auto& ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
                if (ui.showConfirmPopup) {
                    ui.showConfirmPopup = false;
                    popupHandled = true;
                }
            }
            if (!popupHandled) State.showCharacterPanel = false;
        } else if (State.showContextMenu) {
            State.showContextMenu = false;
        } else if (State.showInventory) {
            UIInventory::Toggle();
        }
    }

    // Debug
    if (IsKeyPressed(KEY_F1)) UIMinimap::ToggleDebugReveal();
    
    if (!s_hasGivenTestItems) {
        auto view = registry.view<PlayerTag>();
        if (view.begin() != view.end()) {
            auto bag = ItemFactory::createBag(registry, 1, Rarity::Common);
            registry.get<ItemComponent>(bag).name = "破烂的背包";
            registry.get<ItemComponent>(bag).bagCapacity = 40;
            InventorySystem::pickUpItem(registry, view.front(), bag);
            s_hasGivenTestItems = true;
        }
    }
    
    UIInventory::Update(registry);

    if (State.showMessageBox) {
        State.messageBoxTimer -= GetFrameTime();
        if (State.messageBoxTimer <= 0.0f) State.showMessageBox = false;
    }
}

void UISystem::Draw(entt::registry& registry, const LevelManager& levelManager, const Camera2D& camera) {
    // --- Scale Calculation ---
    float scaleX = (float)GetScreenWidth() / UI_REF_WIDTH;
    float scaleY = (float)GetScreenHeight() / UI_REF_HEIGHT;
    float scale = std::min(scaleX, scaleY);
    State.scaleFactor = scale;
    UIRenderer::SetScale(scale);
    
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    State.hoveredItem = entt::null;

    // 1. Draw Subsystems (Passed logic coordinates will be scaled by UIRenderer)
    if (State.inventoryAlpha > 0.0f) UIInventory::Draw(registry);
    UIMinimap::Draw(registry, levelManager);
    if (State.characterPanelAlpha > 0.0f) UICharacter::Draw(registry);

    // 2. Ground Interaction
    if (State.hoveredItem == entt::null) {
        auto groundItemView = registry.view<ItemComponent, Position>();
        Vector2 mouseLogicPos = GetMousePositionLogic(); // Logic Space
        bool altHeld = IsKeyDown(KEY_LEFT_ALT);
        
        Vector2 playerPos2D = {0, 0};
        entt::entity playerEntity = entt::null;
        auto playerView = registry.view<PlayerTag, Position>();
        if (playerView.begin() != playerView.end()) {
            playerEntity = playerView.front();
            auto& p = playerView.get<Position>(playerEntity);
            playerPos2D = {p.x, p.y};
        }

        Font font = GetFont();

        for (auto entity : groundItemView) {
            const auto* filterResult = registry.try_get<LootFilterResultComponent>(entity);
            if (filterResult && !filterResult->visible && !altHeld) {
                continue; 
            }

            const auto& pos = groundItemView.get<Position>(entity);
            const auto& item = groundItemView.get<ItemComponent>(entity);

            Vector2 screenPos = GetWorldToScreen2D({pos.x, pos.y}, camera); // Screen Space
            Vector2 screenPosLogic = { screenPos.x / scale, screenPos.y / scale }; // Logic Space

            // Calculate Label Rectangle in Logic Space
            float labelScale = 1.0f;
            if (filterResult) labelScale = filterResult->scale;
            float fontSize = 18.0f * labelScale;
            Vector2 textSize = IsFontValid(font) ? MeasureTextEx(font, item.name.c_str(), fontSize, 1.0f) : Vector2{(float)MeasureText(item.name.c_str(), (int)fontSize), fontSize};
            
            Rectangle labelRect = { 
                screenPosLogic.x - (textSize.x / 2.0f) - 4, 
                screenPosLogic.y - 30.0f * labelScale - textSize.y - 2, 
                textSize.x + 8, 
                textSize.y + 4 
            };

            // Interaction Check: Circle around item OR the Label Rectangle
            bool hovered = CheckCollisionPointCircle(mouseLogicPos, screenPosLogic, 30.0f) || 
                           CheckCollisionPointRec(mouseLogicPos, labelRect);

            if (hovered) {
                State.hoveredItem = entity;
                
                // Visual Highlight for the label
                DrawRectangleLinesEx({labelRect.x * scale, labelRect.y * scale, labelRect.width * scale, labelRect.height * scale}, 1.0f * scale, Fade(WHITE, 0.8f));
                
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && playerEntity != entt::null) {
                    float dx = pos.x - playerPos2D.x;
                    float dy = pos.y - playerPos2D.y;
                    float distSq = dx*dx + dy*dy;

                    if (distSq <= 180.0f * 180.0f) { // Slightly increased pickup range for convenience
                        if (InventorySystem::pickUpItem(registry, playerEntity, entity)) {
                            State.hoveredItem = entt::null;
                        } else {
                            State.showMessageBox = true;
                            snprintf(State.messageBoxText, 64, "背包已满");
                            State.messageBoxTimer = 2.0f;
                        }
                    } else {
                        State.showMessageBox = true;
                        snprintf(State.messageBoxText, 64, "距离太远");
                        State.messageBoxTimer = 1.5f;
                    }
                }
                break; 
            }
        }
    }

    // 3. Global Overlays
    if (State.hoveredItem != entt::null && registry.valid(State.hoveredItem)) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        DrawTooltip(registry, State.hoveredItem);
    }

    if (State.showContextMenu) DrawContextMenu(registry);
    if (State.showQuantityPopup) DrawQuantityPopup(registry);
    if (State.showMessageBox) DrawMessageBox();

    // Dragging Phantom
    if (State.draggedItem != entt::null) {
        Vector2 mPos = GetMousePositionLogic(); // Logic Space
        float size = 64.0f; // Logic Size - Increased from 44.0f for better visibility
        // DrawSlot scales input X/Y/Size. We pass Logic Coords.
        // Subtracting half the size to center the icon on the mouse tip.
        UIRenderer::DrawSlot(State.globalFont, registry, mPos.x - size * 0.5f, mPos.y - size * 0.5f, size, State.draggedItem, nullptr, true);
        
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            State.draggedItem = entt::null; // Release
        }
    }
}

// --- Delegate to UIRenderer ---

void UISystem::DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted, bool isLocked, float alpha) {
    UIRenderer::DrawSlot(State.globalFont, registry, x, y, size, item, defaultLabel, highlighted, isLocked, alpha);
}

void UISystem::DrawTextUI(const char* text, float x, float y, float fontSize, Color color, float alpha) {
    UIRenderer::DrawTextUI(State.globalFont, text, x, y, fontSize, color, alpha);
}

void UISystem::DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha) {
    UIRenderer::DrawTextScaled(State.globalFont, text, x, y, fontSize, maxWidth, color, alpha);
}

void UISystem::OpenContextMenu(entt::entity item, bool fromInv, int invIdx, NoMoreDay::EquipmentSlot slot) {
    State.showContextMenu = true;
    State.contextMenuItem = item;
    State.contextMenuPos = GetMousePosition(); // Store Screen Pos for Context Menu (handled by UIRenderer specially)
    State.isContextFromInventory = fromInv;
    State.contextSourceInventoryIndex = invIdx;
    State.contextSourceEquipmentSlot = slot;
}

void UISystem::DrawContextMenu(entt::registry& registry) {
    UIRenderer::DrawContextMenu(State.globalFont, State, registry, 1.0f); // Context menu usually immediate? Or use global alpha if needed.
}

void UISystem::DrawTooltip(entt::registry& registry, entt::entity item) {
    UIRenderer::DrawTooltip(State.globalFont, registry, item, 1.0f);
}

void UISystem::DrawMessageBox() {
    UIRenderer::DrawMessageBox(State.globalFont, State, 1.0f);
}

void UISystem::DrawQuantityPopup(entt::registry& registry) {
    if (State.showQuantityPopup) {
         float x = (float)GetScreenWidth()/2.0f - 100;
         float y = (float)GetScreenHeight()/2.0f - 50;
         DrawRectangle((int)x, (int)y, 200, 100, DARKGRAY);
         DrawText("Quantity Popup (TODO)", (int)(x+10), (int)(y+10), 20, WHITE);
         if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) State.showQuantityPopup = false;
    }
}

void UISystem::Benchmark(entt::registry& registry, const LevelManager& levelManager, int frames) {
}