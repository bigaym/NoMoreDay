#include "game/systems/item/SalvageSystem.hpp"
#include "engine/audio/AudioSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/MaterialBankComponent.hpp"
#include <spdlog/spdlog.h>
#include <raylib.h>

namespace NoMoreDay::SalvageSystem {

bool CanSalvage(const ItemComponent& item) {
    // 1. Locked items cannot be salvaged
    if (item.isLocked) return false;

    // 2. Must be high rarity (Magic+)
    if (item.rarity < Rarity::Magic) return false;
    
    // 3. Must be equipment or jewelry
    if (item.type != ItemType::Weapon && 
        item.type != ItemType::Armor && 
        item.type != ItemType::Shield && 
        item.type != ItemType::Jewelry) return false;

    // 4. Unique items with legendary potential are usually kept.
    // Spec: "Unique items (Rarity::Legendary and ItemType == Unique (or legendaryPotential > 0)) are prohibited by default"
    if (item.legendaryPotential > 0) return false;
    
    // Add check for actual Unique items if they don't have LP
    // Many uniques have a specific ID range or tag, but for now LP check is a good heuristic.
    
    return true;
}

std::vector<SalvageResult> CalculateYield(const ItemComponent& item) {
    std::vector<SalvageResult> results;
    
    // Process regular affixes
    for (const auto& aff : item.affixes) {
        if (aff.type == AffixType::Count) continue;
        
        uint32_t materialId;
        if (aff.isLegendary || IsLegendaryAffix(aff.type)) {
            materialId = 4999; // Legendary Essence
        } else {
            materialId = 4000 + static_cast<uint32_t>(aff.type);
        }
        
        int count = 0;
        if (aff.tier >= 1 && aff.tier <= 3) {
            count = GetRandomValue(0, aff.tier);
        } else if (aff.tier >= 4 && aff.tier <= 7) {
            count = GetRandomValue(aff.tier - 3, aff.tier);
        }
        
        if (count > 0) {
            bool found = false;
            for (auto& res : results) {
                if (res.materialId == materialId) {
                    res.count += count;
                    found = true;
                    break;
                }
            }
            if (!found) {
                results.push_back({materialId, count});
            }
        }
    }
    
    return results;
}

void Execute(entt::registry& registry, entt::entity itemEntity, entt::entity playerEntity) {
    if (!registry.valid(itemEntity) || !registry.valid(playerEntity)) return;
    
    if (!registry.all_of<ItemComponent>(itemEntity)) return;

    const auto& item = registry.get<ItemComponent>(itemEntity);
    if (!CanSalvage(item)) {
        spdlog::warn("Attempted to salvage non-salvageable item: {}", item.name);
        return;
    }
    
    auto yield = CalculateYield(item);
    
    auto* bank = registry.try_get<MaterialBankComponent>(playerEntity);
    if (bank) {
        for (const auto& res : yield) {
            bank->Add(res.materialId, res.count);
        }
        spdlog::info("Salvaged {}: obtained {} different types of shards", item.name, yield.size());
    }
    
    // --- Cleanup References in Inventory ---
    auto* inv = registry.try_get<InventoryComponent>(playerEntity);
    if (inv) {
        for (auto& slot : inv->items) {
            if (slot == itemEntity) {
                slot = entt::null;
                break;
            }
        }
    }
    
    Vector2 fxPos = {0.0f, 0.0f};
    if (const auto *itemPos = registry.try_get<Position>(itemEntity)) {
        fxPos = {itemPos->x, itemPos->y};
    } else if (const auto *playerPos = registry.try_get<Position>(playerEntity)) {
        fxPos = {playerPos->x, playerPos->y};
    }

    auto fxEntity = registry.create();
    registry.emplace<Position>(fxEntity, fxPos.x, fxPos.y);
    VisualEffect vfx;
    vfx.type = VisualEffectType::GoldSparkle;
    vfx.lifeTime = 0.45f;
    vfx.color = GOLD;
    vfx.startScale = 0.7f;
    vfx.endScale = 1.3f;
    registry.emplace<VisualEffect>(fxEntity, vfx);

    auto &audio = AudioSystem::Get();
    constexpr const char* SFX_CANDIDATES[] = {
        "salvage",
        "ui_salvage",
        "craft_salvage"
    };
    for (const char* soundId : SFX_CANDIDATES) {
        if (audio.HasSound(soundId)) {
            audio.PlaySound(soundId, AudioChannel::SFX);
            break;
        }
    }
    
    registry.destroy(itemEntity);
}

void BatchExecute(entt::registry& registry, const std::vector<entt::entity>& entities, entt::entity playerEntity) {
    // Work on a copy of entities to avoid issues if registry changes during loop 
    // (though registry.destroy is fine here as we use a provided list)
    for (auto ent : entities) {
        if (registry.valid(ent)) {
            Execute(registry, ent, playerEntity);
        }
    }
}

} // namespace NoMoreDay::SalvageSystem
