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
// 假设 LevelManager.hpp 包含了 MapSystem 的定义，如果报错则需要显式包含 MapSystem.hpp
#include "raylib.h"
#include "../tools/Logger.hpp"
#include <format>
#include <algorithm>
#include <vector>

using namespace NoMoreDay;

bool UISystem::m_showCharacterPanel = false;
bool UISystem::m_showInventory = false;
Font UISystem::m_font = { 0 };

entt::entity UISystem::m_draggedItem = entt::null;
bool UISystem::m_isDraggingFromInventory = false;
int UISystem::m_dragSourceInventoryIndex = -1;
EquipmentSlot UISystem::m_dragSourceEquipmentSlot = EquipmentSlot::None;

// --- 小地图专用静态资源 (无需修改头文件) ---
static Texture2D s_minimapTexture = { 0 };
static int s_minimapW = 0;
static int s_minimapH = 0;
static std::vector<Color> s_minimapPixels;
static bool s_debugRevealMap = false; // 调试：强制显示全图

void UISystem::Initialize() {
    // 尝试加载系统宋体 (Windows)
    // 优先尝试宋体，其次尝试微软雅黑
    const char* fontPaths[] = {
        "assets/fonts/simsun.ttc", // 优先检查项目资源目录
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf"
    };

    // 生成码点列表 (Codepoints)
    // 包含: ASCII + CJK标点 + CJK统一汉字 (常用中文范围)
    std::vector<int> codepoints;
    // ASCII (32-126)
    for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
    // CJK Symbols and Punctuation (0x3000-0x303F)
    for (int i = 0x3000; i <= 0x303F; ++i) codepoints.push_back(i);
    // CJK Unified Ideographs (0x4E00-0x9FFF) - 约2万字
    for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i);

    bool loaded = false;
    for (const char* path : fontPaths) {
        if (FileExists(path)) {
            LOG_INFO("Attempting to load font: {}", path);
            m_font = LoadFontEx(path, 24, codepoints.data(), (int)codepoints.size());
            
            if (m_font.texture.id != 0) {
                LOG_INFO("Font loaded successfully from {}. Texture ID: {}", path, m_font.texture.id);
                SetTextureFilter(m_font.texture, TEXTURE_FILTER_BILINEAR);
                loaded = true;
                break;
            } else {
                LOG_ERROR("Failed to load font texture from {}", path);
            }
        }
    }

    if (!loaded) {
        LOG_WARN("No suitable Chinese font found. Falling back to default font.");
        m_font = GetFontDefault();
    }
}

void UISystem::Shutdown() {
    if (IsFontReady(m_font)) {
        UnloadFont(m_font);
    }
    // 清理小地图纹理
    if (s_minimapTexture.id != 0) {
        UnloadTexture(s_minimapTexture);
        s_minimapTexture.id = 0;
    }
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

void UISystem::Update(entt::registry& registry) {
    // 切换角色面板显示
    if (IsKeyPressed(KEY_C)) {
        m_showCharacterPanel = !m_showCharacterPanel;
    }
    // 切换背包显示
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_TAB)) {
        m_showInventory = !m_showInventory;
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
}

void UISystem::Draw(entt::registry& registry, const LevelManager& levelManager) {
    if (m_showInventory) {
        DrawInventoryAndEquipment(registry);
    }

    DrawMinimap(registry, levelManager);

    // 角色面板 (仅当开启且存在玩家时绘制)
    if (!m_showCharacterPanel) return;

    auto view = registry.view<PlayerTag>();
    if (view.begin() != view.end()) {
        DrawCharacterPanel(registry, view.front());
    }
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
    // 尝试获取 Sprite 和 PlayerStats
    const auto* sprite = registry.try_get<SpriteComponent>(player);
    const auto* pStats = registry.try_get<PlayerStats>(player);

    // 绘制头像 (缩略图)
    float avatarSize = 80.0f;
    DrawRectangleLines(panelX + padding, currentY, avatarSize, avatarSize, LIGHTGRAY);
    if (sprite && sprite->texture.id > 0) {
        // 简单缩放绘制纹理到头像框
        Rectangle source = {0, 0, (float)sprite->texture.width, (float)sprite->texture.height};
        Rectangle dest = {panelX + padding, currentY, avatarSize, avatarSize};
        DrawTexturePro(sprite->texture, source, dest, {0, 0}, 0.0f, WHITE);
    } else {
        DrawTextUI("?", panelX + padding + 30, currentY + 20, 40, GRAY);
    }

    // 绘制等级信息
    float infoX = panelX + padding + avatarSize + 20.0f;
    if (pStats) {
        DrawTextUI(std::format("等级 {}", pStats->level).c_str(), infoX, currentY + 10, 24, GOLD);
        DrawTextUI(std::format("经验: {:.0f}", pStats->current_xp).c_str(), infoX, currentY + 40, 16, LIGHTGRAY);
    } else {
        DrawTextUI("等级 ??", infoX, currentY + 10, 24, GRAY);
    }

    currentY += avatarSize + 30.0f;

    // 获取属性组件
    const auto* primStats = registry.try_get<PrimaryStats>(player);
    const auto* combatStats = registry.try_get<CombatStats>(player);

    if (!primStats || !combatStats) return;

    // --- 3. 基础属性 (Primary Stats) ---
    DrawTextUI("基础属性", panelX + padding, currentY, 20, YELLOW);
    currentY += 25.0f;
    
    float col1X = panelX + padding;
    float col2X = panelX + panelW / 2.0f + padding;

    // 使用 18.0f 字体大小 (原为默认 20.0f)
    DrawStatRow("力量", std::format("{:.0f}", primStats->strength).c_str(), col1X, currentY, 150, 18.0f);
    DrawStatRow("敏捷", std::format("{:.0f}", primStats->dexterity).c_str(), col1X, currentY, 150, 18.0f);
    DrawStatRow("智力", std::format("{:.0f}", primStats->intelligence).c_str(), col1X, currentY, 150, 18.0f);
    DrawStatRow("体能", std::format("{:.0f}", primStats->vitality).c_str(), col1X, currentY, 150, 18.0f);

    currentY += 20.0f;
    DrawLine(panelX + padding, currentY, panelX + panelW - padding, currentY, GRAY);
    currentY += 20.0f;

    // --- 4. 战斗属性 (Combat Stats) ---
    DrawTextUI("战斗属性", panelX + padding, currentY, 20, RED);
    currentY += 25.0f;

    // 重置 Y 坐标用于双列布局
    float combatStartY = currentY;
    
    // 左列：进攻
    float leftY = combatStartY;
    DrawStatRow("伤害", std::format("{:.0f}-{:.0f}", combatStats->min_weapon_damage, combatStats->max_weapon_damage).c_str(), col1X, leftY, 150);
    DrawStatRow("攻速", std::format("{:.2f}", combatStats->attack_speed).c_str(), col1X, leftY, 150);
    DrawStatRow("暴击率", std::format("{:.1f}%", combatStats->crit_chance * 100.0f).c_str(), col1X, leftY, 150);
    DrawStatRow("暴击伤害", std::format("{:.0f}%", combatStats->crit_damage * 100.0f).c_str(), col1X, leftY, 150);
    // 显示综合冷却效率 (这里简单显示回复速度，或者你可以计算最终系数)
    DrawStatRow("冷却回复", std::format("{:.0f}%", combatStats->cooldown_recovery_speed * 100.0f).c_str(), col1X, leftY, 150);

    // 右列：防御 & 状态
    float rightY = combatStartY;
    DrawStatRow("生命值", std::format("{:.0f}/{:.0f}", combatStats->health, combatStats->max_health).c_str(), col2X, rightY, 150);
    DrawStatRow("护甲", std::format("{:.0f}", combatStats->armor).c_str(), col2X, rightY, 150);
    DrawStatRow("闪避", std::format("{:.1f}%", combatStats->dodge_chance * 100.0f).c_str(), col2X, rightY, 150);
    DrawStatRow("移动速度", std::format("{:.0f}", combatStats->move_speed).c_str(), col2X, rightY, 150);
    DrawStatRow("生命回复", std::format("{:.1f}/s", combatStats->health_regen).c_str(), col2X, rightY, 150);

    currentY = std::max(leftY, rightY) + 20.0f;
    DrawLine(panelX + padding, currentY, panelX + panelW - padding, currentY, GRAY);
    currentY += 20.0f;

    // --- 5. 抗性 (Resistances) ---
    DrawTextUI("抗性", panelX + padding, currentY, 20, SKYBLUE);
    currentY += 25.0f;

    struct ResInfo { const char* name; Color color; int index; };
    ResInfo resList[] = {
        {"火焰", ORANGE, (int)DamageType::Fire},
        {"冰霜", SKYBLUE, (int)DamageType::Cold},
        {"闪电", YELLOW, (int)DamageType::Lightning},
        {"毒素", LIME, (int)DamageType::Poison},
        {"暗影", PURPLE, (int)DamageType::Shadow},
        {"物理", BEIGE, (int)DamageType::Physical}
    };

    float resX = panelX + padding;
    for (const auto& res : resList) {
        float val = combatStats->resistances[res.index];
        
        // 绘制抗性条背景
        DrawRectangle(resX, currentY, 60, 10, Fade(BLACK, 0.5f));
        // 绘制抗性值条
        float barWidth = std::clamp(val, 0.0f, 1.0f) * 60.0f;
        DrawRectangle(resX, currentY, barWidth, 10, res.color);
        
        // 文字
        DrawTextUI(res.name, resX, currentY + 12, 10, LIGHTGRAY);
        DrawTextUI(std::format("{:.0f}%", val * 100.0f).c_str(), resX, currentY + 24, 10, WHITE);

        resX += 70.0f; // 间距
    }
}

void UISystem::DrawStatRow(const char* label, const char* value, float x, float& y, float width, float fontSize) {
    DrawTextUI(label, x, y, fontSize, LIGHTGRAY);
    // 右对齐数值
    float textWidth = 0.0f;
    if (IsFontReady(m_font)) {
        textWidth = MeasureTextEx(m_font, value, fontSize, 1.0f).x;
    } else {
        textWidth = (float)MeasureText(value, (int)fontSize);
    }
    DrawTextUI(value, x + width - textWidth, y, fontSize, WHITE);
    y += fontSize + 5.0f;
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
        
        // --- 拖拽交互 (装备槽) ---
        if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
            m_draggedItem = item;
            m_isDraggingFromInventory = false;
            m_dragSourceEquipmentSlot = slotType;
        }

        // --- 右键卸载 (装备槽) ---
        if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null && inv) {
            if (!inv->isFull()) {
                inv->items.push_back(item);
                equip->set(slotType, entt::null);
            }
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
                
                // --- 拖拽交互 (背包槽) ---
                if (isHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && item != entt::null) {
                    m_draggedItem = item;
                    m_isDraggingFromInventory = true;
                    m_dragSourceInventoryIndex = index;
                }

                // --- 右键穿戴 (背包槽) ---
                if (isHovered && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && item != entt::null && equip) {
                    auto* itemComp = registry.try_get<ItemComponent>(item);
                    if (itemComp && itemComp->slot != EquipmentSlot::None) {
                        // 穿戴
                        entt::entity oldEquip = equip->get(itemComp->slot);
                        equip->set(itemComp->slot, item);
                        if (oldEquip != entt::null) {
                            inv->items[index] = oldEquip;
                        } else {
                            inv->items.erase(inv->items.begin() + index);
                        }
                    }
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
                // 无图标则绘制简短名称
                const char* shortName = itemComp->name.c_str(); // 暂时用全名
                DrawTextUI(shortName, x + 5, y + size/2 - 5, 12, rarityColor);
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

    // --- 3. 纹理更新与绘制 (Texture Update & Draw) ---
    
    // 初始化或重建纹理 (如果尺寸变化或未加载)
    if (s_minimapTexture.id == 0 || s_minimapW != gridW || s_minimapH != gridH) {
        if (s_minimapTexture.id != 0) UnloadTexture(s_minimapTexture);
        s_minimapW = gridW;
        s_minimapH = gridH;
        s_minimapPixels.resize(gridW * gridH);
        Image img = GenImageColor(gridW, gridH, BLACK);
        s_minimapTexture = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(s_minimapTexture, TEXTURE_FILTER_POINT); // 保持像素清晰
        LOG_INFO("Minimap texture initialized: {}x{}", gridW, gridH);
    }

    // 更新像素数据
    int exploredCount = 0; // 统计已探索格子数
    for (int gy = 0; gy < gridH; ++gy) {
        for (int gx = 0; gx < gridW; ++gx) {
            int index = gy * gridW + gx;
            Color c = BLACK;

            bool isExplored = fog.isExplored(gx, gy);
            if (isExplored) exploredCount++;

            if (isExplored || s_debugRevealMap) {
                bool isVisible = s_debugRevealMap ? true : fog.isVisible(gx, gy);
                if (map.isWalkable(gx, gy)) {
                    c = isVisible ? Color{180, 180, 180, 255} : Color{80, 80, 80, 255};
                } else {
                    c = isVisible ? Color{100, 100, 100, 255} : Color{40, 40, 40, 255};
                }
            }
            s_minimapPixels[index] = c;
        }
    }
    UpdateTexture(s_minimapTexture, s_minimapPixels.data());

    // 调试日志 (每 5 秒一次，帮助定位全黑问题)
    static double lastLogTime = 0;
    if (GetTime() - lastLogTime > 5.0) {
        lastLogTime = GetTime();
        bool pExplored = fog.isExplored(playerGx, playerGy);
        bool pVisible = fog.isVisible(playerGx, playerGy);
        LOG_INFO("[Minimap Debug] Player Raw: ({:.1f}, {:.1f}) -> Grid: ({}, {}), Explored: {}, TotalExplored: {}, TextureID: {}", 
                 playerPos.x, playerPos.y, playerGx, playerGy, pExplored, exploredCount, s_minimapTexture.id);
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