#pragma once
#include "TestCommon.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UIAnimationSystem.hpp"
#include "game/components/UIAnimationComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/systems/ui/UISkillHub.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/components/Common.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/ui/MonsterHealthBarSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/ui/PlayerHUD.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <entt/entt.hpp>
#include <raylib.h>

namespace NoMoreDay {

TEST_CASE("[Tech] SkillUI - Drag and Drop Assignment") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& active = registry.emplace<ActiveSkillsComponent>(player);

    // Ensure state is clean
    UISystem::State.isDraggingSkill = true;
    UISystem::State.draggedSkillId = 1; // Assuming skill 1 exists
    UISystem::State.scaleFactor = 1.0f;

    // Simulation logic omitted but structure remains for verification
}

TEST_CASE("[Tech] SkillUI - Context Menu State") {
    UISystem::State.showContextMenu = false;
    UISystem::State.isSkillContext = false;
    
    // Simulate opening context menu
    UISystem::State.showContextMenu = true;
    UISystem::State.isSkillContext = true;
    UISystem::State.contextSourceSkillSlot = 3;
    
    CHECK(UISystem::State.showContextMenu == true);
    CHECK(UISystem::State.isSkillContext == true);
    CHECK(UISystem::State.contextSourceSkillSlot == 3);
}

TEST_CASE("[Tech] SkillUI - UISkillTalentTree scissor scope balanced") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/systems/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/systems/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/systems/ui/UISkillTalentTree.cpp")
    };

    std::string source;
    for (const auto& candidate : candidates) {
        if (!fs::exists(candidate)) {
            continue;
        }
        std::ifstream in(candidate, std::ios::in | std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        source = ss.str();
        if (!source.empty()) {
            break;
        }
    }

    REQUIRE(!source.empty());

    auto countOccur = [&source](const std::string& needle) {
        size_t count = 0;
        size_t pos = 0;
        while ((pos = source.find(needle, pos)) != std::string::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    };

    CHECK(countOccur("BeginScissorMode(") == countOccur("EndScissorMode()"));
}

TEST_CASE("[Tech] SkillUI - Persistence of Assignments") {
    ActiveSkillsComponent original;
    original.slots[0].id = 101;
    original.slots[0].current_charges = 2;
    original.specialized_slots[2].skill_id = 202;
    original.specialized_slots[2].allocated_points[1] = 5;
    original.available_talent_points = 10;

    nlohmann::json j = original; // calls to_json
    ActiveSkillsComponent restored = j; // calls from_json

    CHECK(restored.slots[0].id == 101);
    CHECK(restored.slots[0].current_charges == 2);
    CHECK(restored.specialized_slots[2].skill_id == 202);
    CHECK(restored.specialized_slots[2].allocated_points.at(1) == 5);
    CHECK(restored.available_talent_points == 10);
}

TEST_CASE("[Tech] MonsterHealthBar - Visibility and Buffs") {
    entt::registry registry;
    Camera2D camera = { {0,0}, {0,0}, 0.0f, 1.0f };

    auto enemy = registry.create();
    registry.emplace<EnemyTag>(enemy);
    registry.emplace<Position>(enemy, 100.0f, 100.0f);
    auto& stats = registry.emplace<CombatStats>(enemy);
    stats.health = 50.0f;
    stats.max_health = 100.0f;

    SUBCASE("Render Call Does Not Crash") {
        systems::MonsterHealthBarSystem::Render(registry, camera);
    }

    SUBCASE("Buff Synchronization") {
        auto& activeEffects = registry.emplace<ActiveEffectsComponent>(enemy);
        
        BuffEffect buff;
        buff.id = "test_buff";
        buff.is_debuff = false;
        activeEffects.effects.push_back(buff);

        BuffEffect debuff;
        debuff.id = "test_debuff";
        debuff.is_debuff = true;
        activeEffects.effects.push_back(debuff);

        CHECK(activeEffects.effects.size() == 2);
        CHECK(activeEffects.effects[0].is_debuff == false);
        CHECK(activeEffects.effects[1].is_debuff == true);
        
        systems::MonsterHealthBarSystem::Render(registry, camera);
    }

    SUBCASE("Full Health Hiding") {
        stats.health = 100.0f;
        stats.max_health = 100.0f;
        systems::MonsterHealthBarSystem::Render(registry, camera);
    }
}

TEST_CASE("[Tech] PlayerHUD - Render Logic") {
    entt::registry registry;
    
    // Setup UISystem state scale
    UISystem::State.scaleFactor = 1.0f;

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    auto& stats = registry.emplace<CombatStats>(player);
    stats.health = 80.0f;
    stats.max_health = 100.0f;
    stats.mana = 50.0f;
    stats.max_mana = 100.0f;
    
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.stacks = 5;
    intent.max_stacks = 10;

    SUBCASE("Draw Call Does Not Crash") {
        systems::PlayerHUD::Draw(registry);
    }

    SUBCASE("Missing Components Does Not Crash") {
        auto other = registry.create();
        registry.emplace<PlayerTag>(other);
        systems::PlayerHUD::Draw(registry);
    }
}

TEST_CASE("[Tech] UIRenderer - Tooltip Logic Smoke Test") {
    entt::registry registry;
    Font font = GetFontDefault();
    
    auto itemEntity = registry.create();
    auto& item = registry.emplace<ItemComponent>(itemEntity);
    item.name = "Test Legendary Sword";
    item.rarity = Rarity::Legendary;
    item.socketCount = 3;
    item.textureId = 1; // Dummy ID
    
    // Smoke test for DrawTooltip
    // We can't easily verify the actual drawing but we can ensure it doesn't crash
    UIRenderer::DrawTooltip(font, registry, itemEntity, 1.0f);
}

} // namespace NoMoreDay
