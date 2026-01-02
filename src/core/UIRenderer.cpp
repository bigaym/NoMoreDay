#include "UIRenderer.hpp"
#include "AstrolabeRegistry.hpp"
#include "AssetLoadingSystem.hpp"
#include "SkillRegistry.hpp"
#include "BuffRegistry.hpp"
#include "../systems/InventorySystem.hpp" 
#include "../systems/DamagePipeline.hpp"
#include "../systems/StatsSystem.hpp"
#include "../components/Progression.hpp"
#include <algorithm>
#include <string>
#include <cstdio>
#include <cmath> 

namespace NoMoreDay {

    static float s_uiScale = 1.0f;
    static UITheme s_theme;

    void UIRenderer::SetTheme(const UITheme& theme) {
        s_theme = theme;
    }

    UITheme& UIRenderer::GetTheme() {
        return s_theme;
    }

    void UIRenderer::SetScale(float scale) {
        s_uiScale = scale;
    }

    float UIRenderer::GetScale() {
        return s_uiScale;
    }

    void UIRenderer::DrawTextUI(const Font& font, const char* text, float x, float y, float fontSize, Color color, float alpha) {
        float scaledSize = fontSize * s_uiScale;
        Vector2 pos = { x * s_uiScale, y * s_uiScale };
        Color finalColor = Fade(color, alpha);

        if (IsFontValid(font)) {
            DrawTextEx(font, text, pos, scaledSize, 1.0f * s_uiScale, finalColor);
        } else {
            DrawText(text, (int)pos.x, (int)pos.y, (int)scaledSize, finalColor);
        }
    }

    void UIRenderer::DrawTextScaled(const Font& font, const char* text, float x, float y, float fontSize, float maxWidth, Color color, float alpha) {
        if (!text || text[0] == '\0') return;
        
        float logicWidth = IsFontValid(font) ? MeasureTextEx(font, text, fontSize, 1.0f).x : (float)MeasureText(text, (int)fontSize);
        
        float finalFontSize = fontSize;
        float yOffset = 0.0f;

        if (logicWidth > maxWidth && maxWidth > 0) {
            float scale = maxWidth / logicWidth;
            finalFontSize = fontSize * scale;
            yOffset = (fontSize - finalFontSize) * 0.5f;
        }

        DrawTextUI(font, text, x, y + yOffset, finalFontSize, color, alpha);
    }

    Color UIRenderer::GetRarityColor(NoMoreDay::Rarity rarity) {
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

    const char* UIRenderer::GetShortItemTypeName(const ItemComponent& item) {
        if (item.type == ItemType::Weapon) return "武";
        if (item.type == ItemType::Armor) return "甲";
        if (item.type == ItemType::Consumable) return "耗";
        if (item.type == ItemType::Material) return "料";
        return "物";
    }

    const char* UIRenderer::GetItemCategoryString(const ItemComponent& item) {
        if (item.type == ItemType::Bag) return "背包";
        if (item.type == ItemType::Consumable) return "消耗品";
        if (item.type == ItemType::Material) return "材料";
        
        if (item.type == ItemType::Weapon) {
            if (item.isTwoHanded) return "双手武器";
            return "单手武器";
        }
        
        if (item.type == ItemType::Armor || item.type == ItemType::Shield) {
            switch (item.slot) {
                case EquipmentSlot::Head: return "头盔";
                case EquipmentSlot::Shoulder: return "护肩";
                case EquipmentSlot::Chest: return "胸甲";
                case EquipmentSlot::Hands: return "手套";
                case EquipmentSlot::Legs: return "护腿";
                case EquipmentSlot::Feet: return "鞋子";
                case EquipmentSlot::Neck: return "项链";
                case EquipmentSlot::Ring: return "戒指";
                case EquipmentSlot::OffHand: return "副手";
                default: return "装备";
            }
        }
        return "物品";
    }

    void UIRenderer::DrawSlot(const Font& font, entt::registry& registry, float x, float y, float size, entt::entity item, const char* defaultLabel, bool highlighted, bool isLocked, float alpha) {
        float sx = x * s_uiScale;
        float sy = y * s_uiScale;
        float sSize = size * s_uiScale;

        Rectangle rec = { sx, sy, sSize, sSize };
        
        auto ApplyAlpha = [&](Color c, float a) -> Color {
            return { c.r, c.g, c.b, (unsigned char)((float)c.a * a) };
        };

        // Background
        Color bg = highlighted ? ApplyAlpha(s_theme.panelBorderHighlight, 0.2f) : s_theme.slotBackground;
        if (isLocked) bg = ApplyAlpha(BLACK, 0.8f); 
        
        DrawRectangleRec(rec, ApplyAlpha(bg, alpha));
        
        // Bevel Effect
        DrawLineEx({sx, sy}, {sx + sSize, sy}, 2.0f * s_uiScale, ApplyAlpha(BLACK, 0.5f * alpha));
        DrawLineEx({sx, sy}, {sx, sy + sSize}, 2.0f * s_uiScale, ApplyAlpha(BLACK, 0.5f * alpha));
        DrawLineEx({sx, sy + sSize}, {sx + sSize, sy + sSize}, 1.0f * s_uiScale, ApplyAlpha(WHITE, 0.1f * alpha));
        DrawLineEx({sx + sSize, sy}, {sx + sSize, sy + sSize}, 1.0f * s_uiScale, ApplyAlpha(WHITE, 0.1f * alpha));

        // Border
        Color border = highlighted ? s_theme.panelBorderHighlight : s_theme.panelBorder;
        DrawRectangleLinesEx(rec, 1.0f * s_uiScale, ApplyAlpha(border, alpha));
        
        if (item != entt::null && registry.valid(item)) {
            auto* itemComp = registry.try_get<ItemComponent>(item);
            if (itemComp) {
                Color rarityColor = GetRarityColor(itemComp->rarity);
                DrawRectangleLinesEx(rec, 2.0f * s_uiScale, ApplyAlpha(rarityColor, alpha));

                Texture2D tex = AssetLoadingSystem::GetTexture(itemComp->textureId);
                if (tex.id > 0) {
                    Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
                    float pad = 4.0f * s_uiScale;
                    Rectangle dest = {sx + pad, sy + pad, sSize - pad*2, sSize - pad*2};
                    DrawTexturePro(tex, source, dest, {0, 0}, 0.0f, ApplyAlpha(WHITE, alpha));
                } else {
                    const char* shortName = GetShortItemTypeName(*itemComp);
                    DrawTextUI(font, shortName, x + 10, y + 10, 16, rarityColor, alpha);
                }

                if (itemComp->quantity > 1) {
                    DrawTextUI(font, std::to_string(itemComp->quantity).c_str(), x + size - 15, y + size - 15, 12, s_theme.textPrimary, alpha);
                }
            }
        } 
    }

    void UIRenderer::DrawSkillSlot(const Font& font, float x, float y, float size, 
                                 Texture2D icon, const char* keyLabel, 
                                 float cooldownRatio, float manaCost, 
                                 int charges, int maxCharges,
                                 bool hasEnoughMana, bool isHighlighted, float alpha) {
        float sx = x * s_uiScale;
        float sy = y * s_uiScale;
        float sSize = size * s_uiScale;

        Rectangle rec = { sx, sy, sSize, sSize };
        
        auto ApplyAlpha = [&](Color c, float a) -> Color {
            return { c.r, c.g, c.b, (unsigned char)((float)c.a * a) };
        };

        DrawRectangleRec(rec, ApplyAlpha(isHighlighted ? ApplyAlpha(s_theme.panelBorderHighlight, 0.2f) : s_theme.slotBackground, alpha));
        
        if (icon.id > 0) {
            Rectangle source = {0, 0, (float)icon.width, (float)icon.height};
            float pad = 4.0f * s_uiScale;
            Rectangle dest = {sx + pad, sy + pad, sSize - pad*2, sSize - pad*2};
            
            Color iconColor = WHITE;
            if (!hasEnoughMana) iconColor = { 50, 50, 200, 255 }; 
            else if (cooldownRatio > 0 && charges == 0) iconColor = ApplyAlpha(GRAY, 0.8f);
            
            DrawTexturePro(icon, source, dest, {0, 0}, 0.0f, ApplyAlpha(iconColor, alpha));
        }

        if (cooldownRatio > 0.0f && charges == 0) {
            float startAngle = -90.0f;
            float endAngle = startAngle + (cooldownRatio * 360.0f);
            DrawCircleSector({sx + sSize/2, sy + sSize/2}, sSize/2, startAngle, endAngle, 32, ApplyAlpha(BLACK, 0.6f * alpha));
        }

        if (keyLabel) DrawTextUI(font, keyLabel, x + 4, y + 4, 14, ApplyAlpha(s_theme.textSecondary, alpha));

        if (manaCost > 0) {
            char manaStr[16];
            snprintf(manaStr, 16, "%.0f", manaCost);
            DrawTextUI(font, manaStr, x + 4, y + size - 16, 12, ApplyAlpha(SKYBLUE, alpha));
        }

        if (maxCharges > 1) {
            char chargeStr[16];
            snprintf(chargeStr, 16, "%d", charges);
            DrawTextUI(font, chargeStr, x + size - 14, y + size - 16, 14, ApplyAlpha(WHITE, alpha));
        }

        DrawRectangleLinesEx(rec, 1.0f * s_uiScale, ApplyAlpha(isHighlighted ? s_theme.panelBorderHighlight : s_theme.panelBorder, alpha));
    }

    void UIRenderer::DrawBuffIcon(const Font& font, float x, float y, float size,
                                 Texture2D icon, const char* text, float durationRatio, int stacks,
                                 bool isDebuff, float alpha) {
        float sx = x * s_uiScale;
        float sy = y * s_uiScale;
        float sSize = size * s_uiScale;

        auto ApplyAlpha = [&](Color c, float a) -> Color {
            return { c.r, c.g, c.b, (unsigned char)((float)c.a * a) };
        };

        Rectangle rec = { sx, sy, sSize, sSize };
        DrawRectangleRec(rec, ApplyAlpha(s_theme.slotBackground, alpha));

        if (icon.id > 0) {
            Rectangle source = { 0, 0, (float)icon.width, (float)icon.height };
            float pad = 2.0f * s_uiScale;
            Rectangle dest = { sx + pad, sy + pad, sSize - pad * 2, sSize - pad * 2 };
            DrawTexturePro(icon, source, dest, { 0, 0 }, 0.0f, ApplyAlpha(WHITE, alpha));
        } else if (text && text[0] != '\0') {
            float fontSize = 16.0f;
            float sFontSize = fontSize * s_uiScale;
            Vector2 textSize = IsFontValid(font) ? MeasureTextEx(font, text, sFontSize, 1.0f) : Vector2{(float)MeasureText(text, (int)sFontSize), sFontSize};
            DrawTextUI(font, text, x + (size - textSize.x / s_uiScale) / 2.0f, y + (size - textSize.y / s_uiScale) / 2.0f, fontSize, ApplyAlpha(isDebuff ? RED : GREEN, alpha));
        } else {
            DrawRectangleRec(rec, ApplyAlpha(isDebuff ? RED : GREEN, 0.3f * alpha));
        }

        if (durationRatio > 0.0f) {
            float startAngle = -90.0f;
            float endAngle = startAngle + (durationRatio * 360.0f);
            // Draw a semi-transparent ring instead of sector for cleaner look
            DrawRing({sx + sSize/2, sy + sSize/2}, sSize/2 - 2.0f*s_uiScale, sSize/2, startAngle, endAngle, 16, ApplyAlpha(isDebuff ? RED : YELLOW, 0.4f * alpha));
        }

        if (stacks > 1) {
            char stackStr[16];
            snprintf(stackStr, 16, "%d", stacks);
            DrawTextUI(font, stackStr, x + size - 12, y + size - 12, 12, ApplyAlpha(WHITE, alpha));
        }

        DrawRectangleLinesEx(rec, 1.0f * s_uiScale, ApplyAlpha(isDebuff ? RED : s_theme.panelBorder, alpha));
    }

    void UIRenderer::DrawTooltip(const Font& font, entt::registry& registry, entt::entity item, float alpha) {
        if (!IsWindowReady()) return;
        auto* itemComp = registry.try_get<ItemComponent>(item);
        if (!itemComp) return;

        std::vector<TooltipLine> lines;
        lines.push_back({ GetItemCategoryString(*itemComp), s_theme.textSecondary });
        
        char buffer[128];
        if (itemComp->attack > 0) {
            snprintf(buffer, sizeof(buffer), "攻击力: %.0f", itemComp->attack);
            lines.push_back({ buffer, s_theme.textPrimary });
        }
        if (itemComp->defense > 0) {
            snprintf(buffer, sizeof(buffer), "护甲: %.0f", itemComp->defense);
            lines.push_back({ buffer, s_theme.textPrimary });
        }
        if (itemComp->bagCapacity > 0) {
            snprintf(buffer, sizeof(buffer), "容量: %d 格", itemComp->bagCapacity);
            lines.push_back({ buffer, s_theme.textPrimary });
        }
        
        for (const auto& aff : itemComp->implicits) {
            lines.push_back({ GetAffixDescription(aff, false), GetAffixTierColor(aff.tier) });
        }

        if ((itemComp->attack > 0 || itemComp->defense > 0 || !itemComp->implicits.empty()) && !itemComp->affixes.empty()) {
            lines.push_back({ "---", WHITE, true });
        }

        for (const auto& aff : itemComp->affixes) {
            lines.push_back({ GetAffixDescription(aff, true), GetAffixTierColor(aff.tier) });
        }

        if (!itemComp->description.empty()) {
            lines.push_back({ " ", WHITE }); 
            lines.push_back({ itemComp->description, s_theme.textPrimary });
        }
        
        float fontSize = 18.0f;
        float titleSize = 22.0f;
        float padding = 10.0f;
        float lineHeight = fontSize + 4.0f;
        
        float maxW = 0.0f;
        Vector2 titleDim = IsFontValid(font) ? MeasureTextEx(font, itemComp->name.c_str(), titleSize, 1.0f) : Vector2{(float)MeasureText(itemComp->name.c_str(), (int)titleSize), titleSize};
        maxW = std::max(maxW, titleDim.x);
        for (const auto& line : lines) {
            if (line.isSeparator || line.text == " ") continue;
            float w = IsFontValid(font) ? MeasureTextEx(font, line.text.c_str(), fontSize, 1.0f).x : (float)MeasureText(line.text.c_str(), (int)fontSize);
            maxW = std::max(maxW, w);
        }
        
        float w = maxW + padding * 2;
        float h = padding * 2 + titleSize + 5.0f + lines.size() * lineHeight;

        Vector2 m = GetMousePosition();
        float x = m.x + 15 * s_uiScale;
        float y = m.y + 15 * s_uiScale;
        float sW = w * s_uiScale;
        float sH = h * s_uiScale;

        if (IsWindowReady()) {
            if (x + sW > (float)GetScreenWidth()) x -= (sW + 20 * s_uiScale);
            if (y + sH > (float)GetScreenHeight()) y -= (sH + 20 * s_uiScale);
        }

        DrawRectangle((int)x, (int)y, (int)sW, (int)sH, Fade(s_theme.panelBackground, 0.95f * alpha));
        DrawRectangleLinesEx({x, y, sW, sH}, 1.0f * s_uiScale, Fade(GetRarityColor(itemComp->rarity), alpha));
        
        DrawTextUI(font, itemComp->name.c_str(), (x + padding * s_uiScale) / s_uiScale, (y + padding * s_uiScale) / s_uiScale, titleSize, GetRarityColor(itemComp->rarity), alpha);
        float curSY = y + (padding + titleSize + 5.0f) * s_uiScale;

        for (const auto& line : lines) {
            if (line.isSeparator) {
                DrawLineEx({x + padding*s_uiScale, curSY + lineHeight*s_uiScale/2}, {x + sW - padding*s_uiScale, curSY + lineHeight*s_uiScale/2}, 1.0f*s_uiScale, Fade(s_theme.panelBorder, alpha));
            } else if (line.text != " ") {
                DrawTextUI(font, line.text.c_str(), (x + padding*s_uiScale)/s_uiScale, curSY/s_uiScale, fontSize, line.color, alpha);
            }
            curSY += lineHeight * s_uiScale;
        }
    }

    std::vector<UIRenderer::TooltipLine> UIRenderer::GetSkillTooltipLines(entt::registry& registry, uint32_t skillId) {
        const auto* skill = SkillRegistry::Get().GetSkill(skillId);
        if (!skill) return {};

        std::vector<TooltipLine> lines;
        lines.push_back({ skill->name_key, YELLOW });

        float minDmg = 0, maxDmg = 0;
        float astroIncBonus = 0.0f;
        float astroMoreBonus = 1.0f;
        
        auto playerView = registry.view<PlayerTag, CombatStats>();
        if (playerView.begin() != playerView.end()) {
            entt::entity player = playerView.front();
            const auto& stats = playerView.get<CombatStats>(player);
            
            float avgWeapon = (stats.min_weapon_damage + stats.max_weapon_damage) * 0.5f;
            DamagePool pool;
            pool.Add(Tag::Physical, avgWeapon * skill->weapon_damage_mult + skill->base_damage);
            
            auto result = DamagePipeline::Calculate(registry, player, entt::null, skillId, pool, Tag::Hit);
            minDmg = result.total_damage * 0.9f;
            maxDmg = result.total_damage * 1.1f;

            if (auto* astro = registry.try_get<AstrolabeComponent>(player)) {
                Tag primary_type = Tag::Physical;
                for (int i = 0; i < 6; ++i) {
                    Tag t = static_cast<Tag>(1ULL << i);
                    if (HasTag(skill->tags, t)) {
                        primary_type = t;
                        break;
                    }
                }

                StatType dmg_stat = StatType::PhysicalDamage;
                switch (primary_type) {
                    case Tag::Physical:  dmg_stat = StatType::PhysicalDamage; break;
                    case Tag::Fire:      dmg_stat = StatType::FireDamage; break;
                    case Tag::Cold:      dmg_stat = StatType::ColdDamage; break;
                    case Tag::Lightning: dmg_stat = StatType::LightningDamage; break;
                    case Tag::Poison:    dmg_stat = StatType::PoisonDamage; break;
                    case Tag::Shadow:    dmg_stat = StatType::ShadowDamage; break;
                    default: break;
                }

                const auto& reg = AstrolabeRegistry::Get();
                for (uint32_t node_id : astro->activated_nodes) {
                    if (const auto* node = reg.GetNode(node_id)) {
                        for (const auto& mod : node->modifiers) {
                            if (mod.type == dmg_stat && (mod.required_tags == Tag::None || HasTag(skill->tags, mod.required_tags))) {
                                if (mod.mode == ModifierMode::PercentAdd) {
                                    astroIncBonus += mod.value;
                                } else if (mod.mode == ModifierMode::PercentMult) {
                                    astroMoreBonus *= (1.0f + mod.value);
                                }
                            }
                        }
                    }
                }
            }
        }

        char buffer[128];
        if (minDmg > 0) {
            snprintf(buffer, sizeof(buffer), "预估伤害: %.0f - %.0f", minDmg, maxDmg);
            lines.push_back({ buffer, SKYBLUE });
        }

        if (astroIncBonus > 0.0f) {
            snprintf(buffer, sizeof(buffer), "星盘伤害增加: +%.0f%% (Increased)", astroIncBonus * 100.0f);
            lines.push_back({ buffer, LIME });
        }

        if (astroMoreBonus > 1.0f) {
            snprintf(buffer, sizeof(buffer), "星盘总伤害额外: +%.0f%% (More)", (astroMoreBonus - 1.0f) * 100.0f);
            lines.push_back({ buffer, ORANGE });
        }

        snprintf(buffer, sizeof(buffer), "法力消耗: %.0f", skill->mana_cost);
        lines.push_back({ buffer, s_theme.textSecondary });

        if (skill->cooldown > 0) {
            snprintf(buffer, sizeof(buffer), "冷却时间: %.1fs", skill->cooldown);
            lines.push_back({ buffer, s_theme.textSecondary });
        }

        lines.push_back({ " ", WHITE });
        lines.push_back({ skill->desc_key, s_theme.textPrimary });

        return lines;
    }

    void UIRenderer::DrawSkillTooltip(const Font& font, entt::registry& registry, uint32_t skillId, float alpha) {
        std::vector<TooltipLine> lines = GetSkillTooltipLines(registry, skillId);
        if (lines.empty()) return;

        float padding = 10.0f;
        float titleSize = 22.0f;
        float fontSize = 18.0f;
        float descSize = 16.0f;

        float maxW = 320.0f; 
        
        // Dynamic height calculation
        float h = padding * 2;
        for (size_t i = 0; i < lines.size(); ++i) {
            float size = (i == 0) ? titleSize : fontSize;
            if (i >= lines.size() - 1) size = descSize; // Description is last
            h += (size + 4);
        }

        Vector2 m = { 0, 0 };
        int screenW = 800;
        int screenH = 600;

        if (IsWindowReady()) {
            m = GetMousePosition();
            screenW = GetScreenWidth();
            screenH = GetScreenHeight();
        }

        float x = m.x + 15 * s_uiScale;
        float y = m.y + 15 * s_uiScale;
        float sw = (maxW + padding * 2) * s_uiScale;
        float sh = h * s_uiScale;

        if (x + sw > (float)screenW) x -= (sw + 20 * s_uiScale);
        if (y + sh > (float)screenH) y -= (sh + 20 * s_uiScale);

        DrawRectangle((int)x, (int)y, (int)sw, (int)sh, Fade(s_theme.panelBackground, 0.95f * alpha));
        DrawRectangleLinesEx({x, y, sw, sh}, 1.0f * s_uiScale, Fade(s_theme.panelBorder, alpha));

        float curSY = y + padding * s_uiScale;
        for (size_t i = 0; i < lines.size(); ++i) {
            float size = (i == 0) ? titleSize : fontSize;
            if (i >= lines.size() - 1) size = descSize;

            if (lines[i].text != " ") {
                DrawTextScaled(font, lines[i].text.c_str(), (x + padding * s_uiScale) / s_uiScale, curSY / s_uiScale, size, maxW, lines[i].color, alpha);
            }
            curSY += (size + 4) * s_uiScale;
        }
    }

    static std::string GetStatModifierDescription(const StatModifier& mod) {
        char buffer[128];
        const char* sign = mod.value >= 0 ? "+" : "";
        const char* suffix = "";
        float displayValue = mod.value;

        if (mod.mode == ModifierMode::PercentAdd) {
            suffix = "%";
        } else if (mod.mode == ModifierMode::PercentMult) {
            sign = "x";
            displayValue = 1.0f + mod.value;
            suffix = "";
        }

        const char* statName = "属性";
        switch (mod.type) {
            case StatType::Strength: statName = "力量"; break;
            case StatType::Dexterity: statName = "敏捷"; break;
            case StatType::Intelligence: statName = "智力"; break;
            case StatType::Vitality: statName = "体质"; break;
            case StatType::MaxHealth: statName = "最大生命值"; break;
            case StatType::MaxMana: statName = "最大法力值"; break;
            case StatType::MoveSpeed: statName = "移动速度"; break;
            case StatType::Armor: statName = "护甲"; break;
            case StatType::PhysicalDamage: statName = "物理伤害"; break;
            case StatType::FireDamage: statName = "火焰伤害"; break;
            case StatType::ColdDamage: statName = "冰霜伤害"; break;
            case StatType::LightningDamage: statName = "闪电伤害"; break;
            case StatType::PoisonDamage: statName = "毒素伤害"; break;
            case StatType::ShadowDamage: statName = "暗影伤害"; break;
            case StatType::CritChance: statName = "暴击率"; break;
            case StatType::CritDamage: statName = "暴击伤害"; break;
            case StatType::AttackSpeed: statName = "攻击速度"; break;
            case StatType::CastSpeed: statName = "施法速度"; break;
            case StatType::Accuracy: statName = "命中率"; break;
            case StatType::ManaOnHit: statName = "击中回蓝"; break;
            case StatType::ResistPhysical: statName = "物理抗性"; break;
            case StatType::ResistFire: statName = "火焰抗性"; break;
            case StatType::ResistCold: statName = "冰霜抗性"; break;
            case StatType::ResistLightning: statName = "闪电抗性"; break;
            case StatType::ResistPoison: statName = "毒素抗性"; break;
            case StatType::ResistShadow: statName = "暗影抗性"; break;
            case StatType::ResistAll: statName = "全抗性"; break;
            default: break;
        }

        if (mod.mode == ModifierMode::PercentMult) {
            snprintf(buffer, sizeof(buffer), "%s%.2f %s", sign, displayValue, statName);
        } else {
            snprintf(buffer, sizeof(buffer), "%s%.0f%s %s", sign, displayValue, suffix, statName);
        }
        return std::string(buffer);
    }

    void UIRenderer::DrawBuffTooltip(const Font& font, const BuffEffect& effect, float alpha) {
        const auto& visual = BuffRegistry::GetVisualData(effect.type);
        if (visual.name == "Unknown") return;

        std::vector<TooltipLine> lines;
        
        Color titleColor = visual.is_debuff ? RED : GREEN;
        std::string title = visual.name;
        if (effect.stacks > 1) title += TextFormat(" (x%d)", effect.stacks);
        lines.push_back({ title, titleColor });

        lines.push_back({ visual.description, s_theme.textPrimary });

        for (const auto& mod : effect.modifiers) {
            lines.push_back({ GetStatModifierDescription(mod), titleColor });
        }

        if (effect.duration > 0 && effect.remaining < 3600.0f) {
            char timeBuf[64];
            snprintf(timeBuf, sizeof(timeBuf), "剩余时间: %.1fs", effect.remaining);
            lines.push_back({ timeBuf, s_theme.textSecondary });
        }

        float padding = 10.0f;
        float fontSize = 16.0f;
        float titleSize = 18.0f;

        float maxW = 220.0f;
        float h = padding * 2;
        for (size_t i = 0; i < lines.size(); ++i) {
            h += (i == 0 ? titleSize : fontSize) + 4;
        }

        Vector2 m = GetMousePosition();
        float x = m.x + 15 * s_uiScale;
        float y = m.y + 15 * s_uiScale;
        float sw = (maxW + padding * 2) * s_uiScale;
        float sh = h * s_uiScale;

        if (IsWindowReady()) {
            if (x + sw > (float)GetScreenWidth()) x -= (sw + 20 * s_uiScale);
            if (y + sh > (float)GetScreenHeight()) y -= (sh + 20 * s_uiScale);
        }

        DrawRectangle((int)x, (int)y, (int)sw, (int)sh, Fade(s_theme.panelBackground, 0.95f * alpha));
        DrawRectangleLinesEx({x, y, sw, sh}, 1.0f * s_uiScale, Fade(titleColor, alpha));

        float curSY = y + padding * s_uiScale;
        for (size_t i = 0; i < lines.size(); ++i) {
            float size = (i == 0) ? titleSize : fontSize;
            DrawTextScaled(font, lines[i].text.c_str(), (x + padding * s_uiScale) / s_uiScale, curSY / s_uiScale, size, maxW, lines[i].color, alpha);
            curSY += (size + 4) * s_uiScale;
        }
    }

    void UIRenderer::DrawContextMenu(const Font& font, UIContext& uiContext, entt::registry& registry, float alpha) {
        if (!uiContext.showContextMenu || !registry.valid(uiContext.contextMenuItem)) {
            uiContext.showContextMenu = false;
            return;
        }

        auto* itemComp = registry.try_get<ItemComponent>(uiContext.contextMenuItem);
        if (!itemComp) {
            uiContext.showContextMenu = false;
            return;
        }

        float w = 180.0f; 
        float btnH = 36.0f; 
        int btnCount = 0;

        bool showEquip = false;
        bool showUse = false;
        if (uiContext.isContextFromInventory) {
            if (itemComp->type == ItemType::Weapon || itemComp->type == ItemType::Armor || 
                itemComp->type == ItemType::Shield || itemComp->type == ItemType::Bag) {
                showEquip = true;
            } else if (itemComp->type == ItemType::Consumable) {
                showUse = true;
            }
        }

        bool showUnequip = !uiContext.isContextFromInventory && uiContext.contextSourceEquipmentSlot != EquipmentSlot::None;
        bool showDrop = true;

        if (showEquip) btnCount++;
        if (showUse) btnCount++;
        if (showUnequip) btnCount++;
        if (showDrop) btnCount++;
        btnCount++; 

        float h = btnCount * btnH + 20.0f; 
        
        float sx = uiContext.contextMenuPos.x;
        float sy = uiContext.contextMenuPos.y;
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sBtnH = btnH * s_uiScale;

        if (sx + sw > (float)GetScreenWidth()) sx -= sw;
        if (sy + sh > (float)GetScreenHeight()) sy -= sh;

        DrawRectangle(sx, sy, sw, sh, Fade(s_theme.panelBackground, 0.98f * alpha));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f * s_uiScale, Fade(s_theme.panelBorder, alpha));
        DrawLineEx({sx, sy}, {sx + sw, sy}, 2.0f * s_uiScale, Fade(s_theme.panelBorderHighlight, alpha));

        float curSY = sy + 10.0f * s_uiScale;

        auto DrawMenuBtn = [&](const char* text, Color textColor = WHITE) -> bool {
            Rectangle r = {sx + 5.0f * s_uiScale, curSY, sw - 10.0f * s_uiScale, sBtnH};
            bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
            bool pressed = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            
            if (hovered) {
                Color bg = pressed ? s_theme.buttonPress : s_theme.buttonHover;
                DrawRectangleRec(r, Fade(bg, 0.5f * alpha));
                DrawRectangleLinesEx(r, 1.0f * s_uiScale, Fade(s_theme.panelBorder, 0.5f * alpha));
            }
            
            float sSize = 18.0f * s_uiScale;
            if (IsFontValid(font)) {
                Vector2 textSize = MeasureTextEx(font, text, sSize, 1.0f);
                DrawTextEx(font, text, { sx + (sw - textSize.x) / 2.0f, curSY + (sBtnH - textSize.y) / 2.0f }, sSize, 1.0f * s_uiScale, Fade(hovered ? s_theme.textHighlight : textColor, alpha));
            } else {
                int textW = MeasureText(text, (int)sSize);
                DrawText(text, (int)(sx + (sw - textW) / 2.0f), (int)(curSY + (sBtnH - sSize) / 2.0f), (int)sSize, Fade(hovered ? s_theme.textHighlight : textColor, alpha));
            }
            
            curSY += sBtnH;
            return hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
        };

        if (showEquip) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("装备")) {
                entt::entity player = view.front();
                if (itemComp->type == ItemType::Bag) {
                    auto* inv = registry.try_get<InventoryComponent>(player);
                    if (inv) {
                        int emptySlot = -1;
                        for (int i = 0; i < InventoryComponent::MAX_BAG_SLOTS; ++i) {
                            if (!registry.valid(inv->bag_slots[i])) { emptySlot = i; break; }
                        }
                        if (emptySlot != -1) {
                            InventorySystem::equipBag(registry, player, uiContext.contextMenuItem, emptySlot);
                        } else {
                            InventorySystem::equipBag(registry, player, uiContext.contextMenuItem, 0);
                        }
                    }
                } else {
                    InventorySystem::equipItem(registry, player, uiContext.contextMenuItem);
                }
                uiContext.showContextMenu = false;
            }
        }
        if (showUse) {
            auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("使用")) {
                InventorySystem::useItem(registry, view.front(), uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        if (showUnequip) {
             auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("卸下")) {
                if (!InventorySystem::unequipItem(registry, view.front(), uiContext.contextSourceEquipmentSlot)) {
                    uiContext.showMessageBox = true;
                    snprintf(uiContext.messageBoxText, 64, "背包已满！无法卸下装备。");
                    uiContext.messageBoxTimer = 2.0f;
                }
                uiContext.showContextMenu = false;
            }
        }
        if (showDrop) {
             auto view = registry.view<PlayerTag>();
            if (view.begin() != view.end() && DrawMenuBtn("丢弃", s_theme.danger)) {
                InventorySystem::dropItem(registry, view.front(), uiContext.contextMenuItem);
                uiContext.showContextMenu = false;
            }
        }
        
        if (btnCount > 1) {
             DrawLineEx({sx + 10*s_uiScale, curSY + 2*s_uiScale}, {sx + sw - 10*s_uiScale, curSY + 2*s_uiScale}, 1.0f*s_uiScale, Fade(s_theme.panelBorder, 0.3f*alpha));
             curSY += 5*s_uiScale;
        }

        if (DrawMenuBtn("取消", s_theme.textSecondary)) {
            uiContext.showContextMenu = false;
        }
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            if (!CheckCollisionPointRec(GetMousePosition(), {sx, sy, sw, sh})) {
                uiContext.showContextMenu = false;
            }
        }
    }

    void UIRenderer::DrawMessageBox(const Font& font, UIContext& uiContext, float alpha) {
        if (!uiContext.showMessageBox) return;
        
        const char* text = uiContext.messageBoxText;
        float fontSize = 20;
        int textW = IsFontValid(font) ? (int)MeasureTextEx(font, text, fontSize * s_uiScale, 1.0f).x : MeasureText(text, (int)(fontSize * s_uiScale));
        
        float w = (textW / s_uiScale) + 60.0f;
        float h = 50.0f;
        float sw = w * s_uiScale;
        float sh = h * s_uiScale;
        float sx = ((float)GetScreenWidth() - sw) / 2.0f;
        float sy = ((float)GetScreenHeight() - sh) / 2.0f;
        
        DrawRectangle((int)sx, (int)sy, (int)sw, (int)sh, Fade(s_theme.panelBackground, 0.9f * alpha));
        DrawRectangleLinesEx({sx, sy, sw, sh}, 1.0f * s_uiScale, Fade(s_theme.danger, alpha));
        
        DrawTextUI(font, text, (sx + 30*s_uiScale)/s_uiScale, (sy + 15*s_uiScale)/s_uiScale, fontSize, s_theme.textPrimary, alpha);
    }

} // namespace NoMoreDay