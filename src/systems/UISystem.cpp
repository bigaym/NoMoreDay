#include "UISystem.hpp"
#include "UIInventory.hpp"
#include "UICharacter.hpp"
#include "UIMinimap.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../components/InventoryComponent.hpp"
#include "../core/LevelManager.hpp"
#include "../systems/InventorySystem.hpp"
#include "../systems/ProgressionSystem.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/UIAssetRegistry.hpp"
#include "../core/ItemFactory.hpp"
#include "../tools/Logger.hpp"
#include <algorithm>
#include <string>
#include <cstdio>

using namespace NoMoreDay;

// --- 静态成员初始化 ---
NoMoreDay::UIState_t UISystem::State;
Font UISystem::m_font = { 0 };

std::vector<const char*> UISystem::s_tooltipLines;
int UISystem::s_bufferPoolIndex = 0;
char UISystem::s_textBufferPool[16][128];

entt::entity UISystem::m_contextMenuItem = entt::null;
Vector2 UISystem::m_contextMenuPos = { 0, 0 };
bool UISystem::m_isContextFromInventory = false;
int UISystem::m_contextSourceInventoryIndex = -1;
EquipmentSlot UISystem::m_contextSourceEquipmentSlot = EquipmentSlot::None;

bool UISystem::m_showQuantityPopup = false;
entt::entity UISystem::m_quantityTargetItem = entt::null;
int UISystem::m_quantityActionType = 0;
int UISystem::m_quantityVal = 1;
int UISystem::m_quantityMax = 1;
char UISystem::m_quantityInputBuf[16] = {0};

static bool s_hasGivenTestItems = false; // 调试标记

// --- 生命周期 ---

void UISystem::Initialize(ResourceManager& resourceManager) {
    AssetLoadingSystem::Initialize(resourceManager);

#ifdef TEST_HEADLESS
    LOG_INFO("UISystem: Headless mode, skipping font loading.");
    m_font = GetFontDefault();
    return;
#endif

    // 准备码点 (CJK + ASCII + 标点)
    std::vector<int> codepoints;
    for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
    for (int i = 0x3000; i <= 0x303F; ++i) codepoints.push_back(i); // 标点
    for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i); // CJK 统一汉字
    for (int i = 0xFF00; i <= 0xFFEF; ++i) codepoints.push_back(i); // 全角 ASCII

    const auto& mainFont = assets::ui::fonts::Main_Chinese;

    // 字体候选列表 (优先级从高到低)
    // 注意：simsun.ttc (宋体) 在某些环境下加载可能会失败，因此优先尝试 simhei.ttf (黑体)
    std::vector<std::string> fontCandidates;
    fontCandidates.push_back("C:/Windows/Fonts/simhei.ttf"); // 2. 黑体 (TTF 兼容性更好)
    fontCandidates.push_back("C:/Windows/Fonts/msyh.ttc");   // 3. 微软雅黑
    fontCandidates.push_back("C:/Windows/Fonts/simsun.ttc"); // 4. 宋体

    for (const auto& path : fontCandidates) {
        if (FileExists(path.c_str())) {
            LOG_INFO("UISystem: Attempting to load font from '{}'...", path);
            m_font = resourceManager.loadFont(mainFont.id, path, mainFont.defaultSize, codepoints.data(), (int)codepoints.size());
            
            if (m_font.texture.id != 0) {
                SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
                LOG_INFO("UISystem: Successfully loaded Chinese font from '{}'", path);
                return;
            } else {
                LOG_WARN("UISystem: Failed to load font from '{}', trying next candidate...", path);
            }
        }
    }
    
    LOG_ERROR("UISystem: All Chinese font candidates failed. Falling back to default font (??? for Chinese).");
    if (m_font.texture.id == 0) m_font = GetFontDefault();
}

void UISystem::Shutdown() {
    m_font = { 0 }; // 资源由 ResourceManager 管理
    UIMinimap::Cleanup();
    AssetLoadingSystem::Shutdown();
}

// --- 主循环 ---

void UISystem::Update(entt::registry& registry, const LevelManager& levelManager) {
    // 1. 处理全局快捷键
    
    // 角色面板 (C)
    if (IsKeyPressed(KEY_C)) {
        State.showCharacterPanel = !State.showCharacterPanel;
        if (!State.showCharacterPanel) {
            // 关闭时重置临时加点
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end()) {
                auto& ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
                ui.tempStr = ui.tempDex = ui.tempInt = ui.tempVit = 0;
                ui.showConfirmPopup = false;
            }
        }
        State.showContextMenu = false;
    }

    // 背包 (I / Tab / ESC)
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
        UIInventory::Toggle();
    }

    // ESC 处理 (层级关闭)
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (m_showQuantityPopup) {
            m_showQuantityPopup = false;
        } else if (State.showCharacterPanel) {
            // 检查是否有确认弹窗
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

    // 调试功能
    if (IsKeyPressed(KEY_F1)) UIMinimap::ToggleDebugReveal();
    
    // 调试：自动发放初始背包
    if (!s_hasGivenTestItems) {
        auto view = registry.view<PlayerTag>();
        if (view.begin() != view.end()) {
            auto bag = ItemFactory::createBag(registry, 1, Rarity::Common);
            registry.get<ItemComponent>(bag).name = "破烂的背包";
            registry.get<ItemComponent>(bag).bagCapacity = 8;
            InventorySystem::pickUpItem(registry, view.front(), bag);
            s_hasGivenTestItems = true;
        }
    }
    
    // 子系统 Update (如果需要)
    UIInventory::Update(registry);

    // 更新消息提示框计时器
    if (State.showMessageBox) {
        State.messageBoxTimer -= GetFrameTime();
        if (State.messageBoxTimer <= 0.0f) State.showMessageBox = false;
    }
}

void UISystem::Draw(entt::registry& registry, const LevelManager& levelManager, const Camera2D& camera) {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    State.hoveredItem = entt::null;

    // 1. 绘制子系统
    if (State.showInventory) UIInventory::Draw(registry);
    UIMinimap::Draw(registry, levelManager);
    if (State.showCharacterPanel) UICharacter::Draw(registry);

    // 2. 地面物品交互 (当未悬停 UI 时)
    if (State.hoveredItem == entt::null) {
        auto groundItemView = registry.view<ItemComponent, Position>();
        Vector2 mousePos = GetMousePosition();
        
        // 获取玩家位置
        Vector2 playerPos2D = {0, 0};
        entt::entity playerEntity = entt::null;
        auto playerView = registry.view<PlayerTag, Position>();
        if (playerView.begin() != playerView.end()) {
            playerEntity = playerView.front();
            auto& p = playerView.get<Position>(playerEntity);
            playerPos2D = {p.x, p.y};
        }

        for (auto entity : groundItemView) {
            const auto& pos = groundItemView.get<Position>(entity);
            Vector2 screenPos = GetWorldToScreen2D({pos.x, pos.y}, camera);
            
            if (CheckCollisionPointCircle(mousePos, screenPos, 30.0f)) {
                State.hoveredItem = entity;
                
                // 绘制交互提示圈 (视觉反馈)
                DrawCircleLines((int)screenPos.x, (int)screenPos.y, 30.0f, Fade(GREEN, 0.6f));
                
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && playerEntity != entt::null) {
                    float dx = pos.x - playerPos2D.x;
                    float dy = pos.y - playerPos2D.y;
                    float distSq = dx*dx + dy*dy;

                    if (distSq <= 150.0f * 150.0f) {
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

    // 3. 全局覆盖层 (Tooltip, Menu, Dragging)
    if (State.hoveredItem != entt::null && registry.valid(State.hoveredItem)) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        DrawTooltip(registry, State.hoveredItem);
    }

    if (State.showContextMenu) DrawContextMenu(registry);
    if (m_showQuantityPopup) DrawQuantityPopup(registry);
    if (State.showMessageBox) DrawMessageBox();

    // 拖拽幻影
    if (State.draggedItem != entt::null) {
        Vector2 mPos = GetMousePosition();
        float size = 44.0f;
        DrawSlot(registry, mPos.x - size/2, mPos.y - size/2, size, State.draggedItem, nullptr, true);
        
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            State.draggedItem = entt::null; // 释放
        }
    }
}

// --- 绘图辅助函数 ---

void UISystem::DrawTextUI(const char* text, float x, float y, float fontSize, Color color) {
    if (IsFontReady(m_font)) {
        DrawTextEx(m_font, text, { x, y }, fontSize, 1.0f, color);
    } else {
        DrawText(text, (int)x, (int)y, (int)fontSize, color);
    }
}

void UISystem::DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color) {
    if (!text || text[0] == '\0') return;
    float currentWidth = IsFontReady(m_font) ? MeasureTextEx(m_font, text, fontSize, 1.0f).x : (float)MeasureText(text, (int)fontSize);

    if (currentWidth > maxWidth && maxWidth > 0) {
        float scale = maxWidth / currentWidth;
        float scaledFontSize = fontSize * scale;
        float yOffset = (fontSize - scaledFontSize) * 0.5f;
        if (IsFontReady(m_font)) DrawTextEx(m_font, text, { x, y + yOffset }, scaledFontSize, 1.0f, color);
        else DrawText(text, (int)x, (int)(y + yOffset), (int)scaledFontSize, color);
    } else {
        DrawTextUI(text, x, y, fontSize, color);
    }
}

void UISystem::DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted, bool isLocked) {
    Rectangle rec = { x, y, size, size };
    DrawRectangleRec(rec, highlighted ? Fade(YELLOW, 0.2f) : (isLocked ? Fade(BLACK, 0.8f) : Fade(BLACK, 0.5f)));
    DrawRectangleLinesEx(rec, 1.0f, highlighted ? GOLD : GRAY);
    
    if (item != entt::null && registry.valid(item)) {
        auto* itemComp = registry.try_get<ItemComponent>(item);
        auto* sprite = registry.try_get<SpriteComponent>(item);

        if (itemComp) {
            Color rarityColor = GetRarityColor(itemComp->rarity);
            DrawRectangleLinesEx(rec, 2.0f, rarityColor);

            if (sprite && sprite->texture.id > 0) {
                Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
                Rectangle dest = {x + 4, y + 4, size - 8, size - 8};
                DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
            } else {
                const char* shortName = GetShortItemTypeName(*itemComp);
                float fontSize = 16.0f;
                Vector2 textSize = IsFontReady(m_font) ? MeasureTextEx(m_font, shortName, fontSize, 1.0f) : Vector2{(float)MeasureText(shortName, (int)fontSize), fontSize};
                DrawTextUI(shortName, x + (size - textSize.x) / 2.0f, y + (size - textSize.y) / 2.0f, fontSize, rarityColor);
            }

            if (itemComp->quantity > 1) {
                DrawTextUI(std::to_string(itemComp->quantity).c_str(), x + size - 15, y + size - 15, 12, WHITE);
            }
        }
    }
    
    if (isLocked) {
        DrawLine(x + size * 0.3f, y + size * 0.3f, x + size * 0.7f, y + size * 0.7f, Fade(GRAY, 0.5f));
        DrawLine(x + size * 0.7f, y + size * 0.3f, x + size * 0.3f, y + size * 0.7f, Fade(GRAY, 0.5f));
    }
    DrawRectangleLinesEx({x+1, y+1, size-2, size-2}, 1.0f, Fade(BLACK, 0.3f));
}

// --- 内部逻辑 (Tooltip, Menu, Popup) ---

void UISystem::DrawTooltip(entt::registry& registry, entt::entity item) {
    auto* itemComp = registry.try_get<ItemComponent>(item);
    if (!itemComp) return;

    // 1. 准备数据
    std::vector<std::string> lines;
    
    // 基础属性
    if (itemComp->attack > 0) lines.push_back(TextFormat("攻击力: %.0f", itemComp->attack));
    if (itemComp->defense > 0) lines.push_back(TextFormat("护甲: %.0f", itemComp->defense));
    if (itemComp->bagCapacity > 0) lines.push_back(TextFormat("容量: %d 格", itemComp->bagCapacity));
    
    // 固有词缀
    for (const auto& aff : itemComp->implicits) {
        lines.push_back(GetAffixDescription(aff));
    }
    
    // 分割线 (如果既有基础/固有 又有 显性词缀)
    if ((!lines.empty()) && !itemComp->affixes.empty()) {
        lines.push_back("---");
    }

    // 显性词缀
    for (const auto& aff : itemComp->affixes) {
        lines.push_back(GetAffixDescription(aff));
    }

    // 描述
    if (!itemComp->description.empty()) {
        if (!lines.empty()) lines.push_back(" "); // 空行
        lines.push_back(itemComp->description);
    }
    
    // 2. 计算尺寸
    float fontSize = 18.0f;
    float titleSize = 22.0f;
    float padding = 10.0f;
    float lineHeight = fontSize + 4.0f;
    
    float maxW = 0.0f;
    // 标题宽度
    Vector2 titleDim = IsFontReady(m_font) ? MeasureTextEx(m_font, itemComp->name.c_str(), titleSize, 1.0f) : Vector2{(float)MeasureText(itemComp->name.c_str(), (int)titleSize), titleSize};
    maxW = std::max(maxW, titleDim.x);
    
    // 内容宽度
    for (const auto& line : lines) {
        if (line == "---" || line == " ") continue;
        float w = IsFontReady(m_font) ? MeasureTextEx(m_font, line.c_str(), fontSize, 1.0f).x : (float)MeasureText(line.c_str(), (int)fontSize);
        maxW = std::max(maxW, w);
    }
    
    float w = maxW + padding * 2;
    float h = padding * 2 + titleSize + 5.0f + lines.size() * lineHeight;

    Vector2 m = GetMousePosition();
    float x = m.x + 15;
    float y = m.y + 15;
    
    // 屏幕边界检查
    if (x + w > GetScreenWidth()) x -= (w + 20);
    if (y + h > GetScreenHeight()) y -= (h + 20);

    // 3. 绘制
    DrawRectangle(x, y, w, h, Fade(BLACK, 0.9f));
    DrawRectangleLines(x, y, w, h, GetRarityColor(itemComp->rarity));
    
    // 标题
    DrawTextUI(itemComp->name.c_str(), x + padding, y + padding, titleSize, GetRarityColor(itemComp->rarity));
    
    float curY = y + padding + titleSize + 5.0f;
    for (const auto& line : lines) {
        if (line == "---") {
            DrawLine(x + padding, curY + lineHeight/2, x + w - padding, curY + lineHeight/2, GRAY);
        } else if (line != " ") {
            Color c = WHITE;
            if (line.find("+") == 0) c = GREEN; // 简单的高亮逻辑
            DrawTextUI(line.c_str(), x + padding, curY, fontSize, c);
        }
        curY += lineHeight;
    }
}

void UISystem::OpenContextMenu(entt::entity item, bool fromInv, int invIdx, NoMoreDay::EquipmentSlot slot) {
    State.showContextMenu = true;
    m_contextMenuItem = item;
    m_contextMenuPos = GetMousePosition();
    m_isContextFromInventory = fromInv;
    m_contextSourceInventoryIndex = invIdx;
    m_contextSourceEquipmentSlot = slot;
}

void UISystem::DrawContextMenu(entt::registry& registry) {
    if (!State.showContextMenu || !registry.valid(m_contextMenuItem)) {
        State.showContextMenu = false;
        return;
    }

    // 获取玩家实体 (假设单人游戏，取第一个 PlayerTag)
    auto view = registry.view<PlayerTag>();
    if (view.begin() == view.end()) return;
    entt::entity player = view.front();

    float w = 140; 
    float h = 0;
    float btnH = 30;
    int btnCount = 0;

    // 确定有哪些按钮
    bool showEquip = m_isContextFromInventory; // 简单判断：在背包里就能装备/使用
    bool showUnequip = !m_isContextFromInventory && m_contextSourceEquipmentSlot != EquipmentSlot::None;
    bool showDrop = true;

    if (showEquip) btnCount++;
    if (showUnequip) btnCount++;
    if (showDrop) btnCount++;
    btnCount++; // Cancel

    h = btnCount * btnH + 10;
    float x = m_contextMenuPos.x;
    float y = m_contextMenuPos.y;

    // 背景
    DrawRectangle(x, y, w, h, Fade(BLACK, 0.95f));
    DrawRectangleLines(x, y, w, h, GOLD);

    float curY = y + 5;

    auto DrawMenuBtn = [&](const char* text) -> bool {
        Rectangle r = {x + 5, curY, w - 10, btnH - 2};
        bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
        if (hovered) DrawRectangleRec(r, Fade(GOLD, 0.3f));
        DrawTextUI(text, x + 15, curY + 5, 18, hovered ? WHITE : LIGHTGRAY);
        curY += btnH;
        return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    };

    if (showEquip) {
        if (DrawMenuBtn("装备 / 使用")) {
            InventorySystem::equipItem(registry, player, m_contextMenuItem);
            State.showContextMenu = false;
        }
    }
    if (showUnequip) {
        if (DrawMenuBtn("卸下")) {
            if (!InventorySystem::unequipItem(registry, player, m_contextSourceEquipmentSlot)) {
                State.showMessageBox = true;
                snprintf(State.messageBoxText, 64, "背包已满！无法卸下装备。");
                State.messageBoxTimer = 2.0f;
            }
            State.showContextMenu = false;
        }
    }
    if (showDrop) {
        if (DrawMenuBtn("丢弃")) {
            InventorySystem::dropItem(registry, player, m_contextMenuItem);
            State.showContextMenu = false;
        }
    }
    if (DrawMenuBtn("取消")) {
        State.showContextMenu = false;
    }
    
    // 点击菜单外部关闭
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!CheckCollisionPointRec(GetMousePosition(), {x, y, w, h})) {
            State.showContextMenu = false;
        }
    }
}

void UISystem::DrawQuantityPopup(entt::registry& registry) {
    // ... (保留原有数量选择弹窗逻辑)
}

void UISystem::DrawMessageBox() {
    if (!State.showMessageBox) return;
    
    const char* text = State.messageBoxText;
    float fontSize = 20;
    int textW = MeasureText(text, (int)fontSize);
    float w = textW + 60.0f;
    float h = 50.0f;
    float x = (GetScreenWidth() - w) / 2.0f;
    float y = (GetScreenHeight() - h) / 2.0f;
    
    DrawRectangle((int)x, (int)y, (int)w, (int)h, Fade(BLACK, 0.9f));
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, RED);
    DrawTextUI(text, x + 30, y + 15, fontSize, WHITE);
}

Color UISystem::GetRarityColor(Rarity rarity) {
    switch (rarity) {
        case Rarity::Common:    return LIGHTGRAY;
        case Rarity::Magic:     return SKYBLUE;
        case Rarity::Rare:      return YELLOW;
        case Rarity::Uncommon:  return LIME;
        case Rarity::Set:       return GREEN;
        case Rarity::Epic:      return PURPLE;
        case Rarity::Legendary: return ORANGE;
        case Rarity::Mythic:    return RED;
        default:                return WHITE;
    }
}

const char* UISystem::GetShortItemTypeName(const NoMoreDay::ItemComponent& item) {
    if (item.type == ItemType::Weapon) return "武";
    if (item.type == ItemType::Armor) return "甲";
    if (item.type == ItemType::Consumable) return "耗";
    if (item.type == ItemType::Material) return "料";
    return "物";
}

void UISystem::Benchmark(entt::registry& registry, const LevelManager& levelManager, int frames) {
    // Benchmark logic
}
