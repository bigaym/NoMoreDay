#include "UISystem.hpp"
#include "../components/Common.hpp"
#include "../components/Stats.hpp"
#include "../components/PlayerState.hpp"
#include "../components/AIComponent.hpp"
#include "../components/ItemComponent.hpp"
#include "../components/InventoryComponent.hpp"
#include "../core/LevelManager.hpp"
#include "../systems/FogOfWarSystem.hpp"
#include "../systems/ProgressionSystem.hpp"
#include "../core/AssetLoadingSystem.hpp"
#include "../core/UIAssetRegistry.hpp"
// 假设 LevelManager.hpp 包含了 MapSystem 的定义，如果报错则需要显式包含 MapSystem.hpp
#include "raylib.h"
#include "../tools/Logger.hpp"
#include <format>
#include <algorithm>
#include <vector>

using namespace NoMoreDay;
using namespace entt::literals;

bool UISystem::m_showCharacterPanel = false;
bool UISystem::m_showInventory = false;
Font UISystem::m_font = { 0 };

entt::entity UISystem::m_draggedItem = entt::null;
bool UISystem::m_isDraggingFromInventory = false;
int UISystem::m_dragSourceInventoryIndex = -1;
EquipmentSlot UISystem::m_dragSourceEquipmentSlot = EquipmentSlot::None;
entt::entity UISystem::m_hoveredItem = entt::null;
std::vector<const char*> UISystem::s_tooltipLines;
int UISystem::s_bufferPoolIndex = 0;
char UISystem::s_textBufferPool[16][128];
bool UISystem::s_minimapDirty = true;
int UISystem::s_minimapExploredCount = 0;

bool UISystem::m_showContextMenu = false;
entt::entity UISystem::m_contextMenuItem = entt::null;
Vector2 UISystem::m_contextMenuPos = { 0, 0 };
bool UISystem::m_isContextFromInventory = false;
int UISystem::m_contextSourceInventoryIndex = -1;
EquipmentSlot UISystem::m_contextSourceEquipmentSlot = EquipmentSlot::None;

// --- 角色面板状态 ---
static int s_activeCharTab = 0; // 0: 攻击, 1: 防御, 2: 召唤, 3: 其他
static float s_charPanelScroll = 0.0f;
static float s_lastContentHeight = 0.0f;

// --- 小地图专用静态资源 (无需修改头文件) ---
static Texture2D s_minimapTexture = { 0 };
static int s_minimapW = 0;
static int s_minimapH = 0;
static std::vector<Color> s_minimapPixels;
static bool s_debugRevealMap = false; // 调试：强制显示全图

void UISystem::Initialize(ResourceManager& resourceManager) {
    AssetLoadingSystem::Initialize(resourceManager);

#ifdef TEST_HEADLESS
    LOG_INFO("UISystem: Headless mode, skipping font loading.");
    m_font = GetFontDefault();
    return;
#endif

    // 1. 使用 UIAssetRegistry 中定义的资源
    const auto& mainFont = assets::ui::fonts::Main_Chinese;
    
    // 尝试加载 UIAssetRegistry 中定义的路径
    if (FileExists(mainFont.path.data())) {
        LOG_INFO("UISystem: Loading main font from registry: {}", mainFont.path);
        
        std::vector<int> codepoints;
        for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
        for (int i = 0x3000; i <= 0x303F; ++i) codepoints.push_back(i);
        for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i);

        m_font = resourceManager.loadFont(mainFont.id, std::string(mainFont.path), mainFont.defaultSize, codepoints.data(), (int)codepoints.size());
        
        if (m_font.texture.id != 0) {
            LOG_INFO("UISystem: Main font loaded successfully. ID: {}", mainFont.id);
            SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
            return;
        }
    }

    // 2. 如果 Registry 路径失败，尝试备选路径 (保持向后兼容/鲁棒性)
    const char* fallbackPaths[] = {
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf"
    };

    std::vector<int> codepoints;
    for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
    for (int i = 0x3000; i <= 0x303F; ++i) codepoints.push_back(i);
    for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i);

    bool loaded = false;
    for (const char* path : fallbackPaths) {
        if (FileExists(path)) {
            LOG_INFO("UISystem: Attempting to load fallback font: {}", path);
            
            // 使用路径哈希作为 ID，确保不同路径不会冲突
            entt::id_type fontId = entt::hashed_string(path);
            m_font = resourceManager.loadFont(fontId, path, 24, codepoints.data(), (int)codepoints.size());
            
            if (m_font.texture.id != 0) {
                LOG_INFO("UISystem: Fallback font loaded successfully. Texture ID: {}", m_font.texture.id);
                SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
                loaded = true;
                break;
            }
        }
    }

    if (!loaded) {
        LOG_WARN("UISystem: No suitable Chinese font found. Falling back to default font.");
        m_font = GetFontDefault();
    }
}

void UISystem::Shutdown() {
    // 字体由 ResourceManager 卸载，UISystem 只需要重置引用
    m_font = { 0 };

    // 清理小地图纹理
    if (s_minimapTexture.id != 0) {
        UnloadTexture(s_minimapTexture);
        s_minimapTexture.id = 0;
    }
    AssetLoadingSystem::Shutdown();
}

void UISystem::DrawTextUI(const char* text, float x, float y, float fontSize, Color color) {
    if (IsFontReady(m_font)) {
        // 使用自定义字体绘制，间距设为 1.0f
        DrawTextEx(m_font, text, { x, y }, fontSize, 1.0f, color);
    } else {
        // 回退到默认绘制
        DrawText(text, (int)x, (int)y, (int)fontSize, color);
    }
}

void UISystem::DrawTextScaled(const char* text, float x, float y, float fontSize, float maxWidth, Color color) {
    if (!text || text[0] == '\0') return;

    float currentWidth = 0.0f;
    if (IsFontReady(m_font)) {
        currentWidth = MeasureTextEx(m_font, text, fontSize, 1.0f).x;
    } else {
        currentWidth = (float)MeasureText(text, (int)fontSize);
    }

    if (currentWidth > maxWidth && maxWidth > 0) {
        float scale = maxWidth / currentWidth;
        float scaledFontSize = fontSize * scale;
        // 垂直居中微调，保持基线一致性（简单处理：向下偏移一点点）
        float yOffset = (fontSize - scaledFontSize) * 0.5f;
        
        if (IsFontReady(m_font)) {
            DrawTextEx(m_font, text, { x, y + yOffset }, scaledFontSize, 1.0f, color);
        } else {
            DrawText(text, (int)x, (int)(y + yOffset), (int)scaledFontSize, color);
        }
    } else {
        DrawTextUI(text, x, y, fontSize, color);
    }
}

void UISystem::Update(entt::registry& registry, const LevelManager& levelManager) {
    // 如果右键菜单开启，点击其他地方关闭
    if (m_showContextMenu) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            // ...
        }
    }

    // 切换角色面板显示
    if (IsKeyPressed(KEY_C)) {
        m_showCharacterPanel = !m_showCharacterPanel;
        // 关闭面板时重置临时加点
        if (!m_showCharacterPanel) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end()) {
                auto& ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
                ui.tempStr = 0; ui.tempDex = 0; ui.tempInt = 0; ui.tempVit = 0;
                ui.showConfirmPopup = false;
            }
        }
        m_showContextMenu = false;
    }
    // 切换背包显示
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        // 如果有确认弹窗，ESC先关闭弹窗
        // 这里逻辑稍微复杂，放在下面统一处理
        
        if (m_showInventory) {
            m_showInventory = false;
            m_showContextMenu = false;
        } else {
            m_showInventory = true;
        }
    }
    
    if (IsKeyPressed(KEY_ESCAPE)) {
        // 优先处理确认弹窗
        bool popupHandled = false;
        if (m_showCharacterPanel) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end()) {
                auto& ui = registry.get_or_emplace<AttributeUIComponent>(view.front());
                if (ui.showConfirmPopup) {
                    ui.showConfirmPopup = false;
                    popupHandled = true;
                }
            }
        }

        if (!popupHandled) {
            if (m_showContextMenu) {
            m_showContextMenu = false;
            } else {
            m_showCharacterPanel = false;
            }
        }
    }
    // 调试：切换小地图全开
    if (IsKeyPressed(KEY_F1)) {
        s_debugRevealMap = !s_debugRevealMap;
        LOG_INFO("Minimap Debug Reveal: {}", s_debugRevealMap ? "ON" : "OFF");
    }

    // 调试：增加经验
    if (IsKeyPressed(KEY_F2)) {
        auto view = registry.view<PlayerTag>();
        for (auto entity : view) {
            NoMoreDay::ProgressionSystem::AddExperience(registry, entity, 100.0f);
            LOG_INFO("Debug: Awarded 100 XP to player");
        }
    }

    // 调试：分配力量属性
    if (IsKeyPressed(KEY_F3)) {
        auto view = registry.view<PlayerTag>();
        for (auto entity : view) {
            if (NoMoreDay::ProgressionSystem::AllocateAttribute(registry, entity, StatType::Strength)) {
                LOG_INFO("Debug: Allocated 1 point to Strength");
            } else {
                LOG_WARN("Debug: Failed to allocate point (maybe none available?)");
            }
        }
    }

    // 调试：性能测试
    if (IsKeyPressed(KEY_F4)) {
        Benchmark(registry, levelManager, 1000);
    }
}

void UISystem::Draw(entt::registry& registry, const LevelManager& levelManager) {
    m_hoveredItem = entt::null; // 每帧重置悬停项

    if (m_showInventory) {
        DrawInventoryAndEquipment(registry);
    }

    DrawMinimap(registry, levelManager);

    // 角色面板 (仅当开启且存在玩家时绘制)
    if (m_showCharacterPanel) {
        auto view = registry.view<PlayerTag>();
        if (view.begin() != view.end()) {
            DrawCharacterPanel(registry, view.front());
        }
    }

    // 最后绘制悬停提示 (Tooltip)
    if (m_hoveredItem != entt::null && registry.valid(m_hoveredItem)) {
        DrawTooltip(registry, m_hoveredItem);
    }

    // 绘制右键菜单
    if (m_showContextMenu) {
        DrawContextMenu(registry);
    }
}

void UISystem::Benchmark(entt::registry& registry, const LevelManager& levelManager, int frames) {
    double totalTime = 0;
    for (int i = 0; i < frames; ++i) {
        double start = GetTime();
        // 仅测量逻辑准备时间（注意：在渲染循环外调用可能有副作用，仅用于粗略对比）
        Draw(registry, levelManager); 
        totalTime += (GetTime() - start);
    }
    LOG_INFO("UISystem Benchmark: Avg Draw Time (CPU logic): {:.6f} ms", (totalTime / (double)frames) * 1000.0);
}

void UISystem::DrawCharacterPanel(entt::registry& registry, entt::entity player) {
    // --- 1. 面板背景 ---
    const float panelW = 420.0f;
    const float panelH = 580.0f;
    const float margin = 20.0f;
    
    // 锚定左下角 (Bottom-Left Anchor)
    const float panelX = margin;
    const float panelY = (float)GetScreenHeight() - panelH - margin;
    const float padding = 20.0f;

    // 半透明黑色背景 + 边框
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.85f));
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 2.0f, GOLD);

    // 标题
    DrawTextUI("角色属性", panelX + padding, panelY + padding, 30, WHITE);
    DrawTextUI("按 'C' 关闭", panelX + panelW - 100, panelY + padding + 10, 18, LIGHTGRAY);

    float currentY = panelY + 70.0f;

    // --- 2. 角色概览 (头像 & 等级) ---
    const auto* sprite = registry.try_get<SpriteComponent>(player);
    auto* pStats = registry.try_get<PlayerStats>(player);

    // 绘制头像 (缩略图)
    float avatarSize = 80.0f;
    DrawRectangleLines(panelX + padding, currentY, avatarSize, avatarSize, LIGHTGRAY);
    if (sprite && sprite->texture.id > 0) {
        Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
        Rectangle dest = {panelX + padding, currentY, avatarSize, avatarSize};
        DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
    } else {
        DrawTextUI("?", panelX + padding + 30, currentY + 20, 40, GRAY);
    }

    // 绘制等级信息
    float infoX = panelX + padding + avatarSize + 20.0f;
    if (pStats) {
        DrawTextUI(TextFormat("等级 %d", pStats->level), infoX, currentY + 10, 24, GOLD);
        DrawTextUI(TextFormat("经验: %.0f", pStats->current_xp), infoX, currentY + 40, 16, LIGHTGRAY);
    } else {
        DrawTextUI("等级 ??", infoX, currentY + 10, 24, GRAY);
    }

    currentY += avatarSize + 30.0f;

    // 获取属性组件
    const auto* primStats = registry.try_get<PrimaryStats>(player);
    const auto* combatStats = registry.try_get<CombatStats>(player);
    auto& attrUI = registry.get_or_emplace<AttributeUIComponent>(player);

    if (!primStats || !combatStats || !pStats) return;

    // --- 3. 基础属性 (Primary Stats) ---
    DrawTextUI("基础属性", panelX + padding, currentY, 20, YELLOW);
    
    // 计算剩余点数
    int totalTemp = attrUI.tempStr + attrUI.tempDex + attrUI.tempInt + attrUI.tempVit;
    int remainingPoints = pStats->available_attribute_points - totalTemp;
    
    // 显示可用点数
    const char* pointsText = TextFormat("可用点数: %d", remainingPoints);
    float pointsWidth = MeasureTextEx(m_font, pointsText, 18, 1.0f).x;
    DrawTextUI(pointsText, panelX + panelW - padding - pointsWidth, currentY + 2, 18, remainingPoints > 0 ? GREEN : LIGHTGRAY);

    currentY += 25.0f;
    
    float col1X = panelX + padding;
    // float col2X = panelX + panelW / 2.0f + padding; // 暂时不用双列，给按钮留空间

    // 简单的按钮辅助函数
    auto DrawBtn = [&](float bx, float by, const char* txt) -> bool {
        float size = 20.0f;
        Rectangle r = {bx, by, size, size};
        bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
        bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        DrawRectangleRec(r, hovered ? LIGHTGRAY : DARKGRAY);
        DrawRectangleLinesEx(r, 1.0f, WHITE);
        float tw = MeasureTextEx(m_font, txt, 16, 1.0f).x;
        DrawTextUI(txt, bx + (size-tw)/2, by + 2, 16, WHITE);
        return clicked;
    };

    auto DrawAttrRow = [&](const char* label, float baseVal, int& tempVal, float& y) {
        float rowH = 24.0f;
        DrawTextUI(label, col1X, y, 18, LIGHTGRAY);
        
        float finalVal = baseVal + tempVal;
        const char* valStr = (tempVal > 0) ? TextFormat("%.0f (+%d)", finalVal, tempVal) : TextFormat("%.0f", finalVal);
        Color valColor = (tempVal > 0) ? GREEN : WHITE;
        DrawTextUI(valStr, col1X + 80, y, 18, valColor);

        // 按钮
        float btnX = col1X + 200.0f;
        if (tempVal > 0) {
            if (DrawBtn(btnX, y, "-")) tempVal--;
        }
        if (remainingPoints > 0) {
            if (DrawBtn(btnX + 25, y, "+")) tempVal++;
        }
        
        y += rowH + 5.0f;
    };

    DrawAttrRow("力量", primStats->strength, attrUI.tempStr, currentY);
    DrawAttrRow("敏捷", primStats->dexterity, attrUI.tempDex, currentY);
    DrawAttrRow("智力", primStats->intelligence, attrUI.tempInt, currentY);
    DrawAttrRow("体能", primStats->vitality, attrUI.tempVit, currentY);

    currentY += 15.0f;
    DrawLine(panelX + padding, currentY, panelX + panelW - padding, currentY, GRAY);
    currentY += 10.0f;

    // --- 4. 标签页 (Tabs) ---
    const char* tabNames[] = { "攻击", "防御", "召唤", "其他" };
    int tabCount = 4;
    float tabW = (panelW - padding * 2) / tabCount;
    float tabH = 30.0f;

    for (int i = 0; i < tabCount; ++i) {
        float tx = panelX + padding + i * tabW;
        Rectangle tabRect = { tx, currentY, tabW, tabH };
        bool isSelected = (s_activeCharTab == i);
        bool isHovered = CheckCollisionPointRec(GetMousePosition(), tabRect);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isHovered) {
            s_activeCharTab = i;
            s_charPanelScroll = 0.0f; // 切换标签重置滚动
        }

        DrawRectangleRec(tabRect, isSelected ? Fade(GOLD, 0.3f) : (isHovered ? Fade(WHITE, 0.1f) : Fade(BLACK, 0.5f)));
        DrawRectangleLinesEx(tabRect, 1.0f, isSelected ? GOLD : DARKGRAY);

        float textW = 0;
        if (IsFontReady(m_font)) textW = MeasureTextEx(m_font, tabNames[i], 18, 1.0f).x;
        else textW = (float)MeasureText(tabNames[i], 18);
        
        DrawTextUI(tabNames[i], tx + (tabW - textW) / 2, currentY + 6, 18, isSelected ? WHITE : GRAY);
    }
    currentY += tabH + 5.0f;

    // --- 5. 可滚动内容区域 ---
    float contentH = panelY + panelH - currentY - padding - (totalTemp > 0 ? 40.0f : 0.0f); // 留出确认按钮空间
    Rectangle viewRect = { panelX + padding, currentY, panelW - padding * 2, contentH };

    // 处理滚动
    if (CheckCollisionPointRec(GetMousePosition(), viewRect)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            s_charPanelScroll += wheel * 20.0f;
        }
    }

    // 限制滚动范围
    if (s_charPanelScroll > 0) s_charPanelScroll = 0;
    if (s_lastContentHeight > viewRect.height) {
        float minScroll = viewRect.height - s_lastContentHeight;
        if (s_charPanelScroll < minScroll) s_charPanelScroll = minScroll;
    } else {
        s_charPanelScroll = 0;
    }

    BeginScissorMode((int)viewRect.x, (int)viewRect.y, (int)viewRect.width, (int)viewRect.height);

    float startY = currentY + s_charPanelScroll;
    float y = startY;
    float rowX = panelX + padding + 5.0f;
    float rowW = viewRect.width - 10.0f;

    if (s_activeCharTab == 0) { // --- 攻击标签 ---
        // 计算显示用的有效伤害
        float physMult = combatStats->damage_multipliers[(int)DamageType::Physical];
        float flatPhys = combatStats->flat_damage[(int)DamageType::Physical];
        float dispMin = (combatStats->min_weapon_damage + flatPhys) * physMult;
        float dispMax = (combatStats->max_weapon_damage + flatPhys) * physMult;

        DrawStatRow("面板伤害", TextFormat("%.0f-%.0f", dispMin, dispMax), rowX, y, rowW);
        DrawStatRow("攻击速度", TextFormat("%.2f", combatStats->attack_speed), rowX, y, rowW);
        DrawStatRow("暴击几率", TextFormat("%.1f%%", combatStats->crit_chance * 100.0f), rowX, y, rowW);
        DrawStatRow("暴击伤害", TextFormat("%.0f%%", combatStats->crit_damage * 100.0f), rowX, y, rowW);
        DrawStatRow("冷却回复", TextFormat("%.0f%%", combatStats->cooldown_recovery_speed * 100.0f), rowX, y, rowW);
        DrawStatRow("护甲穿透", TextFormat("%.0f", combatStats->armor_pen), rowX, y, rowW);
        DrawStatRow("击退力度", TextFormat("%.0f", combatStats->knockback), rowX, y, rowW);

        y += 10.0f;
        DrawTextUI("属性伤害加成", rowX, y, 18, ORANGE);
        y += 25.0f;

        for (int i = 0; i < (int)DamageType::Count; ++i) {
            float flat = combatStats->flat_damage[i];
            float mult = combatStats->damage_multipliers[i];
            if (flat > 0 || mult > 1.0f) {
                const char* typeName = GetDamageTypeName((DamageType)i);
                DrawStatRow(typeName, TextFormat("+%.0f / +%.0f%%", flat, (mult - 1.0f) * 100.0f), rowX, y, rowW);
            }
        }

    } else if (s_activeCharTab == 1) { // --- 防御标签 ---
        DrawStatRow("生命值", TextFormat("%.0f / %.0f", combatStats->health, combatStats->max_health), rowX, y, rowW);
        DrawStatRow("生命回复", TextFormat("%.1f /秒", combatStats->health_regen), rowX, y, rowW);
        DrawStatRow("护甲", TextFormat("%.0f", combatStats->armor), rowX, y, rowW);
        DrawStatRow("闪避几率", TextFormat("%.1f%%", combatStats->dodge_chance * 100.0f), rowX, y, rowW);
        DrawStatRow("格挡几率", TextFormat("%.1f%%", combatStats->block_chance * 100.0f), rowX, y, rowW);
        
        y += 10.0f;
        DrawTextUI("抗性", rowX, y, 18, SKYBLUE);
        y += 25.0f;

        struct ResInfo { const char* name; Color color; int index; };
        ResInfo resList[] = {
            {"火焰抗性", ORANGE, (int)DamageType::Fire},
            {"冰霜抗性", SKYBLUE, (int)DamageType::Cold},
            {"闪电抗性", YELLOW, (int)DamageType::Lightning},
            {"毒素抗性", LIME, (int)DamageType::Poison},
            {"暗影抗性", PURPLE, (int)DamageType::Shadow},
            {"物理抗性", BEIGE, (int)DamageType::Physical}
        };

        for (const auto& res : resList) {
            float val = combatStats->resistances[res.index];
            // 绘制抗性条
            DrawTextUI(res.name, rowX, y, 16, LIGHTGRAY);
            DrawTextUI(TextFormat("%.0f%%", val * 100.0f), rowX + rowW - 50, y, 16, WHITE);
            y += 20.0f;
            
            DrawRectangle(rowX, y, rowW, 6, Fade(BLACK, 0.5f));
            DrawRectangle(rowX, y, std::clamp(val, 0.0f, 0.75f) / 0.75f * rowW, 6, res.color); // 假设75%满抗
            y += 15.0f;
        }

    } else if (s_activeCharTab == 2) { // --- 召唤标签 ---
        DrawTextUI("召唤物系统开发中...", rowX, y, 20, GRAY);
        y += 30.0f;

    } else if (s_activeCharTab == 3) { // --- 其他标签 ---
        DrawStatRow("移动速度", TextFormat("%.0f", combatStats->move_speed), rowX, y, rowW);
        DrawStatRow("魔法寻宝", TextFormat("%.0f%%", combatStats->magic_find * 100.0f), rowX, y, rowW);
        DrawStatRow("金币加成", TextFormat("%.0f%%", combatStats->gold_bonus * 100.0f), rowX, y, rowW);
        DrawStatRow("经验加成", TextFormat("%.0f%%", combatStats->experience_gain_mult * 100.0f), rowX, y, rowW);
    }

    s_lastContentHeight = y - startY;
    EndScissorMode();

    // --- 6. 确认/重置按钮 (如果有临时加点) ---
    if (totalTemp > 0) {
        float btnY = panelY + panelH - 45.0f;
        float btnW = 100.0f;
        float btnH = 30.0f;
        float btnX_Confirm = panelX + panelW - padding - btnW;
        float btnX_Reset = btnX_Confirm - btnW - 10.0f;

        // 确认按钮
        Rectangle confirmRect = {btnX_Confirm, btnY, btnW, btnH};
        bool hConfirm = CheckCollisionPointRec(GetMousePosition(), confirmRect);
        if (hConfirm && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            attrUI.showConfirmPopup = true;
        }
        DrawRectangleRec(confirmRect, hConfirm ? GREEN : DARKGREEN);
        DrawRectangleLinesEx(confirmRect, 1.0f, WHITE);
        DrawTextUI("确认加点", btnX_Confirm + 15, btnY + 6, 18, WHITE);

        // 重置按钮
        Rectangle resetRect = {btnX_Reset, btnY, btnW, btnH};
        bool hReset = CheckCollisionPointRec(GetMousePosition(), resetRect);
        if (hReset && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            attrUI.tempStr = 0; attrUI.tempDex = 0; attrUI.tempInt = 0; attrUI.tempVit = 0;
        }
        DrawRectangleRec(resetRect, hReset ? RED : MAROON);
        DrawRectangleLinesEx(resetRect, 1.0f, WHITE);
        DrawTextUI("重置", btnX_Reset + 30, btnY + 6, 18, WHITE);
    }

    // --- 7. 确认弹窗 ---
    if (attrUI.showConfirmPopup) {
        // 遮罩
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
        
        float popW = 300.0f;
        float popH = 150.0f;
        float popX = (GetScreenWidth() - popW) / 2.0f;
        float popY = (GetScreenHeight() - popH) / 2.0f;
        
        DrawRectangle(popX, popY, popW, popH, DARKGRAY);
        DrawRectangleLines(popX, popY, popW, popH, GOLD);
        
        DrawTextUI("确定要分配这些属性点吗?", popX + 40, popY + 40, 20, WHITE);
        
        // 简单的按钮绘制逻辑复用 (因为没有引入 RayGui)
        Rectangle yesRect = {popX + 40, popY + 90, 80, 30};
        if (CheckCollisionPointRec(GetMousePosition(), yesRect)) {
            DrawRectangleRec(yesRect, LIGHTGRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 auto& mutPrim = registry.get<PrimaryStats>(player);
                 auto& mutPStats = registry.get<PlayerStats>(player);
                 mutPrim.strength += attrUI.tempStr;
                 mutPrim.dexterity += attrUI.tempDex;
                 mutPrim.intelligence += attrUI.tempInt;
                 mutPrim.vitality += attrUI.tempVit;
                 mutPStats.available_attribute_points -= totalTemp;
                 
                 attrUI.tempStr = 0; attrUI.tempDex = 0; attrUI.tempInt = 0; attrUI.tempVit = 0;
                 attrUI.showConfirmPopup = false;
                 registry.get_or_emplace<StatsDirty>(player);
            }
        } else {
            DrawRectangleRec(yesRect, GRAY);
        }
        DrawRectangleLinesEx(yesRect, 1.0f, WHITE);
        DrawTextUI("确定", yesRect.x + 20, yesRect.y + 5, 18, WHITE);

        // 取消
        Rectangle noRect = {popX + 180, popY + 90, 80, 30};
        if (CheckCollisionPointRec(GetMousePosition(), noRect)) {
            DrawRectangleRec(noRect, LIGHTGRAY);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                attrUI.showConfirmPopup = false;
            }
        } else {
            DrawRectangleRec(noRect, GRAY);
        }
        DrawRectangleLinesEx(noRect, 1.0f, WHITE);
        DrawTextUI("取消", noRect.x + 20, noRect.y + 5, 18, WHITE);
    }
}

void UISystem::DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize) {
    float labelMaxWidth = width * 0.6f; // 标签最多占 60% 宽度
    DrawTextScaled(label, x, y, fontSize, labelMaxWidth, LIGHTGRAY);
    
    // 右对齐数值
    float textWidth = 0.0f;
    if (IsFontReady(m_font)) {
        textWidth = MeasureTextEx(m_font, value, fontSize, 1.0f).x;
    } else {
        textWidth = (float)MeasureText(value, (int)fontSize);
    }
    
    // 如果数值太宽，也进行缩放
    float valueMaxWidth = width - labelMaxWidth - 5.0f;
    if (textWidth > valueMaxWidth) {
        DrawTextScaled(value, x + width - valueMaxWidth, y, fontSize, valueMaxWidth, WHITE);
    } else {
        DrawTextUI(value, x + width - textWidth, y, fontSize, WHITE);
    }
    
    y += fontSize + 5.0f;
}

void UISystem::DrawTooltip(entt::registry& registry, entt::entity item) {
    auto* itemComp = registry.try_get<ItemComponent>(item);
    if (!itemComp) return;

    // --- 1. 准备文本内容 (Zero-allocation using static vector and buffer pool) ---
    s_tooltipLines.clear();
    s_bufferPoolIndex = 0;

    auto getNextBuffer = [&]() -> char* {
        char* buf = s_textBufferPool[s_bufferPoolIndex];
        s_bufferPoolIndex = (s_bufferPoolIndex + 1) % 16;
        return buf;
    };

    const char* rarityText = "未知";
    switch (itemComp->rarity) {
        case Rarity::Common: rarityText = "普通"; break;
        case Rarity::Magic: rarityText = "魔法"; break;
        case Rarity::Rare: rarityText = "稀有"; break;
        case Rarity::Uncommon: rarityText = "不凡"; break;
        case Rarity::Set: rarityText = "套装"; break;
        case Rarity::Epic: rarityText = "史诗"; break;
        case Rarity::Legendary: rarityText = "传奇"; break;
        case Rarity::Mythic: rarityText = "神话"; break;
    }

    const char* typeText = "物品";
    switch (itemComp->type) {
        case ItemType::Weapon: typeText = "武器"; break;
        case ItemType::Armor: typeText = "护甲"; break;
        case ItemType::Consumable: typeText = "消耗品"; break;
        case ItemType::Material: typeText = "材料"; break;
        case ItemType::Quest: typeText = "任务物品"; break;
    }

    const char* slotText = nullptr;
    switch (itemComp->slot) {
        case EquipmentSlot::Head: slotText = "头部"; break;
        case EquipmentSlot::Shoulder: slotText = "肩部"; break;
        case EquipmentSlot::Chest: slotText = "胸部"; break;
        case EquipmentSlot::Hands: slotText = "手部"; break;
        case EquipmentSlot::Legs: slotText = "腿部"; break;
        case EquipmentSlot::Feet: slotText = "脚部"; break;
        case EquipmentSlot::Neck: slotText = "项链"; break;
        case EquipmentSlot::Ring1:
        case EquipmentSlot::Ring2: slotText = "手指"; break;
        case EquipmentSlot::MainHand: slotText = "主手"; break;
        case EquipmentSlot::OffHand: slotText = "副手"; break;
        default: break;
    }

    if (slotText) {
        char* buf = getNextBuffer();
        snprintf(buf, 128, "%s - %s", typeText, slotText);
        s_tooltipLines.push_back(buf);
    } else {
        s_tooltipLines.push_back(typeText);
    }

    // 基础属性
    if (itemComp->attack > 0) {
        char* buf = getNextBuffer();
        snprintf(buf, 128, "攻击力: %.0f", itemComp->attack);
        s_tooltipLines.push_back(buf);
    }
    if (itemComp->defense > 0) {
        char* buf = getNextBuffer();
        snprintf(buf, 128, "防御力: %.0f", itemComp->defense);
        s_tooltipLines.push_back(buf);
    }

    // 词缀
    for (const auto& affix : itemComp->implicits) {
        char* buf = getNextBuffer();
        snprintf(buf, 128, "%s (固有)", GetAffixDescriptionRef(affix));
        s_tooltipLines.push_back(buf);
    }
    for (const auto& affix : itemComp->affixes) {
        char* buf = getNextBuffer();
        // 必须拷贝，因为 GetAffixDescriptionRef 返回的是 Raylib 的临时静态缓冲
        snprintf(buf, 128, "%s", GetAffixDescriptionRef(affix));
        s_tooltipLines.push_back(buf);
    }

    // 潜力
    if (itemComp->forgingPotential > 0) {
        char* buf = getNextBuffer();
        snprintf(buf, 128, "锻造潜力: %d", itemComp->forgingPotential);
        s_tooltipLines.push_back(buf);
    }
    if (itemComp->legendaryPotential > 0) {
        char* buf = getNextBuffer();
        snprintf(buf, 128, "传奇潜力: %d", itemComp->legendaryPotential);
        s_tooltipLines.push_back(buf);
    }

    // 描述 (简单处理，暂不分行)
    if (!itemComp->description.empty()) {
        s_tooltipLines.push_back(""); // 空行
        s_tooltipLines.push_back(itemComp->description.c_str());
    }

    // --- 2. 计算尺寸 (动态适应内容) ---
    float padding = 15.0f;
    float titleFontSize = 22.0f;
    float bodyFontSize = 18.0f;
    float minWidth = 200.0f;
    float maxWidth = 400.0f;
    float calculatedWidth = minWidth;

    auto measureWidth = [&](const char* text, float fontSize) {
        if (IsFontReady(m_font)) return MeasureTextEx(m_font, text, fontSize, 1.0f).x;
        return (float)MeasureText(text, (int)fontSize);
    };

    // 检查名称宽度
    calculatedWidth = std::max(calculatedWidth, measureWidth(itemComp->name.c_str(), titleFontSize));
    // 检查稀有度宽度
    calculatedWidth = std::max(calculatedWidth, measureWidth(rarityText, bodyFontSize - 2));
    
    // 检查每一行内容的宽度
    for (const char* line : s_tooltipLines) {
        if (line && line[0] != '\0') {
            calculatedWidth = std::max(calculatedWidth, measureWidth(line, bodyFontSize));
        }
    }

    // 应用内边距并限制在 maxWidth 以内
    float finalWidth = std::min(calculatedWidth + padding * 2, maxWidth);
    
    float height = padding * 2;
    height += titleFontSize + 5.0f; // 标题高度
    height += bodyFontSize + 10.0f; // 稀有度文本高度
    
    for (const char* line : s_tooltipLines) {
        if (!line || line[0] == '\0') height += 10.0f;
        else height += bodyFontSize + 4.0f;
    }

    // --- 3. 确定绘制位置 ---
    Vector2 mPos = GetMousePosition();
    float x = mPos.x + 20.0f;
    float y = mPos.y + 20.0f;

    if (x + finalWidth > (float)GetScreenWidth()) x = mPos.x - finalWidth - 10.0f;
    if (y + height > (float)GetScreenHeight()) y = mPos.y - height - 10.0f;

    // --- 4. 绘制 ---
    Color rarityColor = GetRarityColor(itemComp->rarity);
    DrawRectangleRec({x, y, finalWidth, height}, Fade(BLACK, 0.95f));
    DrawRectangleLinesEx({x, y, finalWidth, height}, 2.0f, rarityColor);
    DrawRectangleLinesEx({x, y, finalWidth, height}, 1.0f, DARKGRAY);

    float curY = y + padding;
    DrawTextScaled(itemComp->name.c_str(), x + padding, curY, titleFontSize, finalWidth - padding * 2, rarityColor);
    curY += titleFontSize + 5.0f;

    DrawTextUI(rarityText, x + padding, curY, bodyFontSize - 2, rarityColor);
    curY += bodyFontSize + 5.0f;

    DrawLine(x + padding, curY, x + finalWidth - padding, curY, Fade(rarityColor, 0.3f));
    curY += 10.0f;

    for (const char* line : s_tooltipLines) {
        if (!line || line[0] == '\0') {
            curY += 10.0f;
            continue;
        }
        
        Color textColor = WHITE;
        // 使用 strstr 避免 std::string 比较
        if (strstr(line, "(固有)")) textColor = SKYBLUE;
        else if (strstr(line, "锻造潜力")) textColor = ORANGE;
        else if (strstr(line, "攻击力") || strstr(line, "防御力")) textColor = LIGHTGRAY;
        
        DrawTextScaled(line, x + padding, curY, bodyFontSize, finalWidth - padding * 2, textColor);
        curY += bodyFontSize + 4.0f;
    }
}

void UISystem::DrawContextMenu(entt::registry& registry) {
    auto* itemComp = registry.try_get<ItemComponent>(m_contextMenuItem);
    if (!itemComp) {
        m_showContextMenu = false;
        return;
    }

    float width = 120.0f;
    float optionHeight = 30.0f;
    int numOptions = 3;
    float height = numOptions * (optionHeight + 2.0f) + 10.0f;

    float x = m_contextMenuPos.x;
    float y = m_contextMenuPos.y;

    if (x + width > (float)GetScreenWidth()) x = m_contextMenuPos.x - width;
    if (y + height > (float)GetScreenHeight()) y = m_contextMenuPos.y - height;

    // 如果点击菜单外部，关闭菜单
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(GetMousePosition(), {x, y, width, height})) {
        m_showContextMenu = false;
        return;
    }

    // 绘制背景
    DrawRectangleRec({x, y, width, height}, Fade(BLACK, 0.95f));
    DrawRectangleLinesEx({x, y, width, height}, 1.0f, GOLD);

    // 获取玩家组件 (用于操作)
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() == playerView.end()) return;
    auto player = playerView.front();
    auto* inv = registry.try_get<InventoryComponent>(player);
    auto* equip = registry.try_get<EquipmentComponent>(player);

    float curY = y + 5.0f;
    auto drawOption = [&](const char* label, bool enabled, std::function<void()> action) {
        Rectangle optRec = { x + 5, curY, width - 10, optionHeight };
        bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), optRec);
        
        if (hovered) {
            DrawRectangleRec(optRec, Fade(GOLD, 0.3f));
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                action();
                m_showContextMenu = false;
            }
        }
        
        DrawTextUI(label, optRec.x + 10, optRec.y + 5, 18, enabled ? (hovered ? YELLOW : WHITE) : GRAY);
        curY += optionHeight + 2.0f;
    };

    // 选项 1: 穿戴/卸载
    if (m_isContextFromInventory) {
        bool canEquip = itemComp->slot != EquipmentSlot::None;
        drawOption("穿戴", canEquip, [&]() {
            if (equip) {
                entt::entity oldEquip = equip->get(itemComp->slot);
                equip->set(itemComp->slot, m_contextMenuItem);
                if (oldEquip != entt::null) {
                    inv->items[m_contextSourceInventoryIndex] = oldEquip;
                } else {
                    inv->items.erase(inv->items.begin() + m_contextSourceInventoryIndex);
                }
                registry.get_or_emplace<StatsDirty>(player);
            }
        });
    } else {
        drawOption("卸载", true, [&]() {
            if (inv && !inv->isFull()) {
                inv->items.push_back(m_contextMenuItem);
                equip->set(m_contextSourceEquipmentSlot, entt::null);
                registry.get_or_emplace<StatsDirty>(player);
            }
        });
    }

    // 选项 2: 使用 (消耗品)
    bool isConsumable = itemComp->type == ItemType::Consumable;
    drawOption("使用", isConsumable, [&]() {
        // TODO: 应用消耗品效果
        itemComp->quantity--;
        if (itemComp->quantity <= 0) {
            if (m_isContextFromInventory) {
                inv->items.erase(inv->items.begin() + m_contextSourceInventoryIndex);
            } else {
                equip->set(m_contextSourceEquipmentSlot, entt::null);
            }
            registry.destroy(m_contextMenuItem);
        }
    });

    // 选项 3: 丢弃
    drawOption("丢弃", true, [&]() {
        if (m_isContextFromInventory) {
            inv->items.erase(inv->items.begin() + m_contextSourceInventoryIndex);
        } else {
            equip->set(m_contextSourceEquipmentSlot, entt::null);
            registry.get_or_emplace<StatsDirty>(player);
        }
        // TODO: 在地上生成掉落物，或者直接销毁
        registry.destroy(m_contextMenuItem);
    });
}

void UISystem::DrawInventoryAndEquipment(entt::registry& registry) {
    auto playerView = registry.view<PlayerTag>();
    if (playerView.begin() == playerView.end()) return;
    auto player = playerView.front();

    auto* inv = registry.try_get<InventoryComponent>(player);
    auto* equip = registry.try_get<EquipmentComponent>(player);

    const float panelW = 840.0f;
    const float panelH = 650.0f;
    
    const float panelX = ((float)GetScreenWidth() - panelW) / 2.0f;
    const float panelY = ((float)GetScreenHeight() - panelH) / 2.0f;
    const float padding = 20.0f;

    // 1. 背景与边框
    DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.92f));
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 3.0f, DARKGRAY);
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 1.0f, GOLD);

    // 2. 标题栏
    DrawRectangle(panelX, panelY, panelW, 40, Fade(GRAY, 0.2f));
    DrawTextUI("角色物品栏 & 装备", panelX + padding, panelY + 8, 24, GOLD);
    DrawTextUI("按 'I' 或 'ESC' 关闭", panelX + panelW - 180, panelY + 12, 16, LIGHTGRAY);

    // --- 左侧：装备区 (Equipment) ---
    float equipX = panelX + padding;
    float equipY = panelY + 60.0f;
    float equipW = 320.0f;
    
    DrawRectangleRounded({equipX, equipY, equipW, panelH - 80}, 0.05f, 4, Fade(WHITE, 0.05f));
    DrawTextUI("装备槽位", equipX + 10, equipY + 10, 20, YELLOW);
    
    // 装备槽位配置 (按规格说明顺序)
    struct SlotDef { const char* label; EquipmentSlot slot; };
    static const SlotDef slotDefs[] = {
        {"头盔", EquipmentSlot::Head}, {"护肩", EquipmentSlot::Shoulder},
        {"胸甲", EquipmentSlot::Chest}, {"手套", EquipmentSlot::Hands},
        {"护腿", EquipmentSlot::Legs}, {"靴子", EquipmentSlot::Feet},
        {"项链", EquipmentSlot::Neck}, {"戒指 1", EquipmentSlot::Ring1},
        {"戒指 2", EquipmentSlot::Ring2}, {"主手武器", EquipmentSlot::MainHand},
        {"副手武器", EquipmentSlot::OffHand}
    };

    float slotSize = 54.0f;
    float slotGap = 12.0f;
    float startX = equipX + 20.0f;
    float startY = equipY + 45.0f;

    for (int i = 0; i < 11; ++i) {
        float x = startX + (i % 2) * (slotSize + 80.0f); // 两列布局
        float y = startY + (i / 2) * (slotSize + slotGap);
        
        EquipmentSlot slotType = slotDefs[i].slot;
        entt::entity item = (equip) ? equip->get(slotType) : entt::null;

        bool isHovered = CheckCollisionPointRec(GetMousePosition(), {x, y, slotSize, slotSize});
        
        // 设置悬停物品
        if (isHovered && item != entt::null && m_draggedItem == entt::null) {
            m_hoveredItem = item;
        }

        // --- 拖拽交互 (装备槽) ---
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
            m_draggedItem = item;
            m_isDraggingFromInventory = false;
            m_dragSourceEquipmentSlot = slotType;
        }

        // --- 右键菜单 (装备槽) ---
        if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null) {
            m_showContextMenu = true;
            m_contextMenuItem = item;
            m_contextMenuPos = GetMousePosition();
            m_isContextFromInventory = false;
            m_contextSourceEquipmentSlot = slotType;
        }
        
        if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && m_draggedItem != entt::null) {
            // 尝试放入该装备槽 (需校验槽位类型)
            auto* dragItemComp = registry.try_get<ItemComponent>(m_draggedItem);
            if (dragItemComp && dragItemComp->slot == slotType) {
                if (m_isDraggingFromInventory && inv) {
                    // 从背包 -> 装备 (交换)
                    entt::entity oldEquip = equip->get(slotType);
                    equip->set(slotType, m_draggedItem);
                    inv->items[m_dragSourceInventoryIndex] = oldEquip;
                    if (oldEquip == entt::null) {
                        inv->items.erase(inv->items.begin() + m_dragSourceInventoryIndex);
                    }
                } else if (!m_isDraggingFromInventory) {
                    // 装备 -> 装备 (交换)
                    entt::entity oldEquip = equip->get(slotType);
                    equip->set(slotType, m_draggedItem);
                    equip->set(m_dragSourceEquipmentSlot, oldEquip);
                }
                m_draggedItem = entt::null;
                registry.get_or_emplace<StatsDirty>(player);
            }
        }

        DrawSlot(registry, x, y, slotSize, (m_draggedItem == item) ? entt::null : item, slotDefs[i].label, isHovered);
        
        // 绘制槽位名称文本 (在槽位右侧)
        DrawTextUI(slotDefs[i].label, x + slotSize + 5, y + slotSize/2 - 8, 14, isHovered ? WHITE : GRAY);
    }

    // --- 中间：垂直分割线 ---
    DrawLineEx({panelX + 355, panelY + 60}, {panelX + 355, panelY + panelH - padding}, 2.0f, DARKGRAY);

    // --- 右侧：背包区 (Inventory) ---
    float invX = panelX + 375.0f;
    float invY = panelY + 60.0f;
    float invW = panelW - (invX - panelX) - padding;

    // 标签页 (Tabs)
    static int currentTab = 0; // 0: 物品, 1: 材料
    const char* tabs[] = { " 物品 ", " 材料 " };
    for (int i = 0; i < 2; ++i) {
        float tabW = 80.0f;
        float tabX = invX + i * (tabW + 5);
        Rectangle tabRec = { tabX, invY, tabW, 30 };
        
        bool hovered = CheckCollisionPointRec(GetMousePosition(), tabRec);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hovered) currentTab = i;
        
        DrawRectangleRec(tabRec, (currentTab == i) ? DARKGRAY : (hovered ? GRAY : BLACK));
        DrawRectangleLinesEx(tabRec, 1.0f, (currentTab == i) ? GOLD : DARKGRAY);
        DrawTextUI(tabs[i], tabX + 15, invY + 5, 18, (currentTab == i) ? WHITE : LIGHTGRAY);
    }

    // 背包内容背景
    DrawRectangleRounded({invX, invY + 30, invW, panelH - 110}, 0.05f, 4, Fade(WHITE, 0.05f));
    
    if (currentTab == 0) {
        // 绘制物品网格 (Task 4)
        const int cols = 7;
        const int rows = 8;
        float gridStartX = invX + 15.0f;
        float gridStartY = invY + 45.0f;

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int index = r * cols + c;
                float x = gridStartX + c * (slotSize + 6.0f);
                float y = gridStartY + r * (slotSize + 6.0f);
                
                entt::entity item = (inv && index < (int)inv->items.size()) ? inv->items[index] : entt::null;

                bool isHovered = CheckCollisionPointRec(GetMousePosition(), {x, y, slotSize, slotSize});
                
                // 设置悬停物品
                if (isHovered && item != entt::null && m_draggedItem == entt::null) {
                    m_hoveredItem = item;
                }

                // --- 拖拽交互 (背包槽) ---
                if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
                    m_draggedItem = item;
                    m_isDraggingFromInventory = true;
                    m_dragSourceInventoryIndex = index;
                }

                // --- 右键菜单 (背包槽) ---
                if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null) {
                    m_showContextMenu = true;
                    m_contextMenuItem = item;
                    m_contextMenuPos = GetMousePosition();
                    m_isContextFromInventory = true;
                    m_contextSourceInventoryIndex = index;
                }

                if (isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && m_draggedItem != entt::null) {
                    if (m_isDraggingFromInventory && inv) {
                        // 背包 -> 背包 (交换/移动)
                        if (index < (int)inv->items.size()) {
                            std::swap(inv->items[m_dragSourceInventoryIndex], inv->items[index]);
                        } else {
                            // 移到空白位置 (简单追加或按索引，目前InventoryComponent是vector)
                            // 这里简化逻辑：如果是空白格，就把源位置清空并放到最后，或者如果是在列表范围内则交换
                            // 实际上当前 InventoryComponent 使用的是 vector，不存储 null。
                        }
                    } else if (!m_isDraggingFromInventory && inv && equip) {
                        // 装备 -> 背包
                        // 卸下装备到指定位置或末尾
                        equip->set(m_dragSourceEquipmentSlot, entt::null);
                        inv->items.push_back(m_draggedItem);
                        // 如果指定位置有东西，那逻辑就复杂了，暂时只支持 push_back
                        registry.get_or_emplace<StatsDirty>(player);
                    }
                    m_draggedItem = entt::null;
                }

                DrawSlot(registry, x, y, slotSize, (m_draggedItem == item) ? entt::null : item, nullptr, isHovered);
            }
        }
    } else {
        // 绘制材料列表 (Task 5)
        DrawTextUI("暂无材料...", invX + 20, invY + 60, 20, GRAY);
    }

    // --- 绘制拖拽中的物体 (Phantom Icon) ---
    if (m_draggedItem != entt::null) {
        Vector2 mPos = GetMousePosition();
        DrawSlot(registry, mPos.x - slotSize/2, mPos.y - slotSize/2, slotSize, m_draggedItem, nullptr, true);
        
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            // 如果释放时没有被消费（说明没落在槽位上），则取消拖拽
            m_draggedItem = entt::null;
        }
    }
}

void UISystem::DrawSlot(entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted) {
    Rectangle rec = { x, y, size, size };
    
    // 背景
    DrawRectangleRec(rec, highlighted ? Fade(YELLOW, 0.2f) : Fade(BLACK, 0.5f));
    
    // 边框
    DrawRectangleLinesEx(rec, 1.0f, highlighted ? GOLD : GRAY);
    
    // 如果有物品
    if (item != entt::null && registry.valid(item)) {
        auto* itemComp = registry.try_get<ItemComponent>(item);
        auto* sprite = registry.try_get<SpriteComponent>(item);

        if (itemComp) {
            // 绘制品质边框
            Color rarityColor = GetRarityColor(itemComp->rarity);
            DrawRectangleLinesEx(rec, 2.0f, rarityColor);

            // 绘制物品图标 (如果有)
            if (sprite && sprite->texture.id > 0) {
                Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
                Rectangle dest = {x + 4, y + 4, size - 8, size - 8};
                DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
            } else {
                // 无图标则绘制简短名称 (Task 3)
                const char* shortName = GetShortItemTypeName(*itemComp);
                float fontSize = 16.0f;
                Vector2 textSize = { 0, 0 };
                if (IsFontReady(m_font)) {
                    textSize = MeasureTextEx(m_font, shortName, fontSize, 1.0f);
                } else {
                    textSize = { (float)MeasureText(shortName, (int)fontSize), fontSize };
                }
                DrawTextUI(shortName, x + (size - textSize.x) / 2.0f, y + (size - textSize.y) / 2.0f, fontSize, rarityColor);
            }

            // 绘制数量 (如果堆叠)
            if (itemComp->quantity > 1) {
                DrawTextUI(std::to_string(itemComp->quantity).c_str(), x + size - 15, y + size - 15, 12, WHITE);
            }
        }
    } else if (defaultLabel) {
        // 绘制占位符文字
        // DrawTextUI(defaultLabel, x + 5, y + size/2 - 5, 10, DARKGRAY);
    }
    
    // 内阴影效果
    DrawRectangleLinesEx({x+1, y+1, size-2, size-2}, 1.0f, Fade(BLACK, 0.3f));
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
    if (item.type == ItemType::Weapon) {
        // 简单判断武器类型
        if (item.name.find("剑") != std::string::npos || item.name.find("Sword") != std::string::npos) return "剑";
        if (item.name.find("斧") != std::string::npos || item.name.find("Axe") != std::string::npos) return "斧";
        if (item.name.find("匕") != std::string::npos || item.name.find("Dagger") != std::string::npos) return "匕";
        if (item.name.find("杖") != std::string::npos || item.name.find("Staff") != std::string::npos) return "杖";
        if (item.name.find("弓") != std::string::npos || item.name.find("Bow") != std::string::npos) return "弓";
        return "武";
    }

    if (item.type == ItemType::Armor) {
        switch (item.slot) {
            case EquipmentSlot::Head: return "头";
            case EquipmentSlot::Shoulder: return "肩";
            case EquipmentSlot::Chest: return "胸";
            case EquipmentSlot::Hands: return "手";
            case EquipmentSlot::Legs: return "腿";
            case EquipmentSlot::Feet: return "鞋";
            case EquipmentSlot::Neck: return "项";
            case EquipmentSlot::Ring1:
            case EquipmentSlot::Ring2: return "戒";
            default: return "甲";
        }
    }

    if (item.type == ItemType::Consumable) return "耗";
    if (item.type == ItemType::Material) return "料";
    if (item.type == ItemType::Quest) return "任";

    return "项";
}

void UISystem::DrawMinimap(entt::registry& registry, const LevelManager& levelManager) {
    const auto& map = levelManager.getMapSystem();
    const auto& fog = levelManager.getFogSystem();

    // 小地图配置
    const float mapSize = 150.0f;
    const float margin = 20.0f;
    const float x = (float)GetScreenWidth() - mapSize - margin;
    const float y = margin;

    // 绘制背景边框
    DrawRectangle(x - 2, y - 2, mapSize + 4, mapSize + 4, DARKGRAY);
    DrawRectangle(x, y, mapSize, mapSize, BLACK);

    // 获取地图尺寸 (Grid Units)
    int gridW = fog.getWidth();
    int gridH = fog.getHeight();
    if (gridW == 0 || gridH == 0) return;

    // 1. 获取玩家位置 (作为小地图中心)
    auto view = registry.view<PlayerTag, Position>();
    if (view.begin() == view.end()) return;
    
    entt::entity playerEntity = view.front();
    const auto& playerPos = view.get<Position>(playerEntity);
    
    // 玩家所在的网格坐标
    int playerGx = static_cast<int>(playerPos.x / FogOfWarSystem::TILE_SIZE);
    int playerGy = static_cast<int>(playerPos.y / FogOfWarSystem::TILE_SIZE);

    // 2. 定义视野范围 (半径，单位：格)
    // 25格半径 => 50x50格的显示区域
    const int viewRadius = 25; 
    
    // 计算缩放比例：地图框大小 / (视野直径)
    float scale = mapSize / (float)(viewRadius * 2);

    // --- 3. 纹理更新与绘制 ---
    
    // 初始化或重建纹理 (如果尺寸变化或未加载)
    if (s_minimapTexture.id == 0 || s_minimapW != gridW || s_minimapH != gridH) {
        if (s_minimapTexture.id != 0) UnloadTexture(s_minimapTexture);
        s_minimapW = gridW;
        s_minimapH = gridH;
        s_minimapPixels.resize(gridW * gridH);
        Image img = GenImageColor(gridW, gridH, BLACK);
        s_minimapTexture = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(s_minimapTexture, TEXTURE_FILTER_POINT);
        LOG_INFO("Minimap texture initialized: {}x{}", gridW, gridH);
        s_minimapDirty = true;
    }

    // 仅在数据可能变化时更新纹理 (简单起见每 5 帧检查一次，或者依赖外部触发)
    // 这里我们先保留每帧检查逻辑，但仅在像素真正变化时标记 Dirty
    static int frameCounter = 0;
    if (frameCounter++ % 10 == 0) {
        s_minimapExploredCount = 0;
        bool changed = false;
        for (int gy = 0; gy < gridH; ++gy) {
            for (int gx = 0; gx < gridW; ++gx) {
                int index = gy * gridW + gx;
                Color oldC = s_minimapPixels[index];
                Color c = BLACK;

                bool isExplored = fog.isExplored(gx, gy);
                if (isExplored) s_minimapExploredCount++;

                if (isExplored || s_debugRevealMap) {
                    bool isVisible = s_debugRevealMap ? true : fog.isVisible(gx, gy);
                    if (map.isWalkable(gx, gy)) {
                        c = isVisible ? Color{180, 180, 180, 255} : Color{80, 80, 80, 255};
                    } else {
                        c = isVisible ? Color{100, 100, 100, 255} : Color{40, 40, 40, 255};
                    }
                }
                if (c.r != oldC.r || c.g != oldC.g || c.b != oldC.b) {
                    s_minimapPixels[index] = c;
                    changed = true;
                }
            }
        }
        if (changed) s_minimapDirty = true;
    }

    if (s_minimapDirty) {
        UpdateTexture(s_minimapTexture, s_minimapPixels.data());
        s_minimapDirty = false;
    }

    // 调试日志 (每 5 秒一次，帮助定位全黑问题)
    static double lastLogTime = 0;
    if (GetTime() - lastLogTime > 5.0) {
        lastLogTime = GetTime();
        bool pExplored = fog.isExplored(playerGx, playerGy);
        bool pVisible = fog.isVisible(playerGx, playerGy);
        LOG_INFO("[Minimap Debug] Player Raw: ({:.1f}, {:.1f}) -> Grid: ({}, {}), Explored: {}, TotalExplored: {}, TextureID: {}", 
                 playerPos.x, playerPos.y, playerGx, playerGy, pExplored, s_minimapExploredCount, s_minimapTexture.id);
    }

    // 绘制纹理部分
    // 源区域：以玩家为中心，半径为 viewRadius 的区域
    Rectangle sourceRec = {
        (float)playerGx - viewRadius, 
        (float)playerGy - viewRadius, 
        (float)viewRadius * 2, 
        (float)viewRadius * 2
    };
    
    // 目标区域：UI 框
    Rectangle destRec = { x, y, mapSize, mapSize };
    
    // 绘制纹理 (Raylib 会自动处理 sourceRec 超出纹理边界的情况)
    DrawTexturePro(s_minimapTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);

    // 4. 绘制怪物 (红点)
    auto enemyView = registry.view<AIComponent, Position>();
    for (auto entity : enemyView) {
        const auto& enemyPos = enemyView.get<Position>(entity);
        
        // 计算相对于玩家的网格距离
        float dx = (enemyPos.x - playerPos.x) / FogOfWarSystem::TILE_SIZE;
        float dy = (enemyPos.y - playerPos.y) / FogOfWarSystem::TILE_SIZE;

        // 仅绘制视野范围内的怪物
        if (std::abs(dx) <= viewRadius && std::abs(dy) <= viewRadius) {
            float drawX = x + (dx + viewRadius) * scale;
            float drawY = y + (dy + viewRadius) * scale;
            DrawCircle((int)(drawX + scale * 0.5f), (int)(drawY + scale * 0.5f), 2.0f, RED);
        }
    }

    // 绘制玩家位置
    // 玩家永远在小地图中心
    DrawCircle((int)(x + mapSize / 2.0f), (int)(y + mapSize / 2.0f), 3.0f, GREEN);
    
    // 边框装饰
    DrawRectangleLines(x, y, mapSize, mapSize, GOLD);
}