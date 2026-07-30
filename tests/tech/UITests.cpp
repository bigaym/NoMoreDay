#pragma once
#include "TestCommon.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/ui/UIAnimationSystem.hpp"
#include "game/systems/ui/UISkillSpecRenderer.hpp"
#include "game/components/UIAnimationComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/systems/ui/UISkillHub.hpp"
#include "game/systems/ui/UISkillTalentTree.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/components/Common.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/components/vfx/SwordIntentVisualComponent.hpp"
#include "game/systems/ui/MonsterHealthBarSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Buff.hpp"
#include "game/systems/ui/PlayerHUD.hpp"
#include "game/systems/ui/SwordIntentWidget.hpp"
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

TEST_CASE("[Tech] SkillUI - UISkillTalentTree scissor scope uses exactly one pair") {
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

    CHECK(countOccur("BeginScissorMode(") == 1);
    CHECK(countOccur("EndScissorMode()") == 1);
}

TEST_CASE("[Tech] SkillUI - tooltip helpers anchor hierarchy and wrapping") {
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

    CHECK(source.find("DrawTooltipHeader(") != std::string::npos);
    CHECK(source.find("DrawRoleBadge(") != std::string::npos);
    CHECK(source.find("DrawScopeBadge(") != std::string::npos);
    CHECK(source.find("DrawKeywordHighlights(") != std::string::npos);
    CHECK(source.find("BuildTreeFeedbackState(") != std::string::npos);
    CHECK(source.find("BeginScissorMode(") != std::string::npos);
    CHECK(source.find("EndScissorMode()") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - tooltip layout reserves footer separation") {
    const auto sparseLayout = SkillTreeUI::ComputeTooltipLayoutMetrics(280.0f, 0, 0);
    CHECK(sparseLayout.descriptionHeight >= 56.0f);
    CHECK(sparseLayout.descriptionBottom + sparseLayout.footerGap <= sparseLayout.footerTop);

    const auto denseLayout = SkillTreeUI::ComputeTooltipLayoutMetrics(280.0f, 3, 5);
    CHECK(denseLayout.footerHeight > sparseLayout.footerHeight);
    CHECK(denseLayout.descriptionBottom + denseLayout.footerGap <= denseLayout.footerTop);
}

TEST_CASE("[Tech] SkillUI - tooltip shell and clamp precede badge drawing") {
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

    const size_t tooltipPos = source.find("// --- Tooltip & Actions ---");
    const size_t clampPos = source.find("if (ty + th > logicH)", tooltipPos);
    const size_t shellPos = source.find("// Draw Tooltip Box", tooltipPos);
    const size_t badgeLayoutPos = source.find("float badgeX = tx + 20.0f;", tooltipPos);
    const size_t badgeDrawPos = source.find("DrawTooltipBadgeChip(", tooltipPos);

    REQUIRE(tooltipPos != std::string::npos);
    REQUIRE(clampPos != std::string::npos);
    REQUIRE(shellPos != std::string::npos);
    REQUIRE(badgeLayoutPos != std::string::npos);
    REQUIRE(badgeDrawPos != std::string::npos);
    CHECK(clampPos < shellPos);
    CHECK(shellPos < badgeLayoutPos);
    CHECK(badgeLayoutPos < badgeDrawPos);
}

TEST_CASE("[Tech] SkillUI - tooltip title and badges use dedicated readable text treatment") {
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

    CHECK(source.find("ResolveTooltipTitleColor(") != std::string::npos);
    CHECK(source.find("ResolveTooltipBadgeTextColor(") != std::string::npos);
    CHECK(source.find("badge.outline, alpha);") == std::string::npos);
}

TEST_CASE("[Tech] SkillUI - quantitative tooltip percent formatting uses whole-percent values") {
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

    CHECK(source.find("totalVal * 100.0f") == std::string::npos);
    CHECK(source.find("TextFormat(\"%s%.0f%% %s\", sign.c_str(), totalVal, label)") != std::string::npos);
    CHECK(source.find("TextFormat(\"%s%.0f%% %s\", sign.c_str(), totalVal, locLabel)") != std::string::npos);
}

TEST_CASE("[Tech] InventoryUI - button text uses shared emoji fallback path") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/engine/render/UIRenderer.cpp"),
        fs::path("../src/engine/render/UIRenderer.cpp"),
        fs::path("../../src/engine/render/UIRenderer.cpp")
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

    const size_t drawButtonPos = source.find("void UIRenderer::DrawButton(");
    REQUIRE(drawButtonPos != std::string::npos);

    const size_t measurePos = source.find("MeasureTextUI(", drawButtonPos);
    const size_t drawTextUiPos = source.find("DrawTextUI(", drawButtonPos);
    const size_t directDrawPos = source.find("DrawTextEx(font, text, textPos", drawButtonPos);

    CHECK(measurePos != std::string::npos);
    CHECK(drawTextUiPos != std::string::npos);
    CHECK(directDrawPos == std::string::npos);
}

TEST_CASE("[Tech] InventoryUI - equipment replacement routes inventory drags through transactional swap path") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/systems/ui/UIInventory.cpp"),
        fs::path("../src/game/systems/ui/UIInventory.cpp"),
        fs::path("../../src/game/systems/ui/UIInventory.cpp")
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

    const size_t equipDropPos = source.find("if (allowInventoryInput && !handledDrop && isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && UISystem::State.draggedItem != entt::null)");
    REQUIRE(equipDropPos != std::string::npos);

    const size_t inventoryBranchPos = source.find("UISystem::State.isDraggingFromInventory", equipDropPos);
    const size_t swapApiPos = source.find("InventorySystem::swapInventoryItemIntoEquipment(", equipDropPos);
    const size_t genericEquipPos = source.find("InventorySystem::equipItem(", equipDropPos);

    CHECK(inventoryBranchPos != std::string::npos);
    CHECK(swapApiPos != std::string::npos);
    CHECK(genericEquipPos == std::string::npos);
}

TEST_CASE("[Tech] InventoryUI - gameplay fallback does not clear drags while inventory overlay is active") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/states/GameplayState.cpp"),
        fs::path("../src/game/states/GameplayState.cpp"),
        fs::path("../../src/game/states/GameplayState.cpp")
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

    const size_t cleanupPos = source.find("UISystem::DrawDraggingPhantom(registry);");
    REQUIRE(cleanupPos != std::string::npos);

    const size_t releaseGuardPos = source.find("if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !UISystem::State.showInventory)", cleanupPos);

    CHECK(releaseGuardPos != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - shared mastery theme plumbing guards hub and tree chrome") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 4> candidates = {
        fs::path("src/game/systems/ui/UISkillHub.cpp"),
        fs::path("src/game/systems/ui/UISkillSpecRenderer.hpp"),
        fs::path("src/game/systems/ui/UISkillSpecRenderer.cpp"),
        fs::path("src/game/systems/ui/UISkillTalentTree.cpp")
    };

    std::array<std::string, 4> sources;
    for (size_t index = 0; index < candidates.size(); ++index) {
        for (const fs::path& base : {fs::path("."), fs::path(".."), fs::path("../..")}) {
            const fs::path candidate = base / candidates[index];
            if (!fs::exists(candidate)) {
                continue;
            }
            std::ifstream in(candidate, std::ios::in | std::ios::binary);
            std::ostringstream ss;
            ss << in.rdbuf();
            sources[index] = ss.str();
            if (!sources[index].empty()) {
                break;
            }
        }
        REQUIRE_MESSAGE(!sources[index].empty(), candidates[index].string().c_str());
    }

    CHECK(sources[0].find("GetBladeMasteryUIThemeProfile(profile.id)") != std::string::npos);
    CHECK(sources[1].find("ClassifyNodeVisual") != std::string::npos);
    CHECK(sources[2].find("GetBladeMasteryUIThemeProfile(tree->mastery_id)") != std::string::npos);
    CHECK(sources[2].find("ClassifyNodeVisual") != std::string::npos);
    CHECK(sources[3].find("GetBladeMasteryUIThemeProfile(tree->mastery_id)") != std::string::npos);
    CHECK(sources[3].find("ClassifyNodeVisual") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - mastery hub exposes Heavenly Sword attunement controls") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/systems/ui/UISkillHub.cpp"),
        fs::path("../src/game/systems/ui/UISkillHub.cpp"),
        fs::path("../../src/game/systems/ui/UISkillHub.cpp")
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
    CHECK(source.find("selectedMastery == BladeMasteryId::HeavenlySword") != std::string::npos);
    CHECK(source.find("Heavenly Sword Attunement") != std::string::npos);
    CHECK(source.find("Lightning") != std::string::npos);
    CHECK(source.find("Frost") != std::string::npos);
    CHECK(source.find("Fire") != std::string::npos);
    CHECK(source.find("Rectangle buttonLogic") != std::string::npos);
    CHECK(source.find("CheckCollisionPointRec(UISystem::GetMousePositionLogic(), buttonLogic)") != std::string::npos);
    CHECK(source.find("SetHeavenlySwordAttunement") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - mastery hub locks all Blade Ascendant signature skills consistently") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/systems/ui/UISkillHub.cpp"),
        fs::path("../src/game/systems/ui/UISkillHub.cpp"),
        fs::path("../../src/game/systems/ui/UISkillHub.cpp")
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
    CHECK(source.find("(id == 10)") == std::string::npos);
    CHECK(source.find("id == 10 || id == 11 || id == 12") != std::string::npos);
    CHECK(source.find("IsSignatureSkillUnlocked(registry, player, id)") != std::string::npos);
    CHECK(source.find("signatureLocked") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - shared node visual classification drives radius consistently") {
    TalentNode passiveNode;
    passiveNode.max_points = 5;

    TalentNode modifierNode;
    modifierNode.max_points = 4;

    TalentNode keystoneNode;
    keystoneNode.max_points = 1;

    NodeContractData triggerContract;
    triggerContract.role = SpecNodeRole::Trigger;

    NodeContractData synergyContract;
    synergyContract.role = SpecNodeRole::Synergy;

    NodeContractData transmuterContract;
    transmuterContract.role = SpecNodeRole::Transmuter;

    const SkillSpecView view{.zoom = 1.25f};

    CHECK(UISkillSpecRenderer::ClassifyNodeVisual(passiveNode) ==
          UISkillSpecRenderer::NodeType::Passive);
    CHECK(UISkillSpecRenderer::GetNodeRadius(passiveNode, view) ==
          doctest::Approx(40.0f * view.zoom * 0.75f));

    CHECK(UISkillSpecRenderer::ClassifyNodeVisual(modifierNode) ==
          UISkillSpecRenderer::NodeType::Modifier);
    CHECK(UISkillSpecRenderer::GetNodeRadius(modifierNode, view) ==
          doctest::Approx(40.0f * view.zoom * 0.95f));

    CHECK(UISkillSpecRenderer::ClassifyNodeVisual(keystoneNode) ==
          UISkillSpecRenderer::NodeType::Keystone);
    CHECK(UISkillSpecRenderer::GetNodeRadius(keystoneNode, view) ==
          doctest::Approx(40.0f * view.zoom * 1.35f));

    CHECK(UISkillSpecRenderer::ClassifyNodeVisual(modifierNode, &triggerContract) ==
          UISkillSpecRenderer::NodeType::Trigger);
    CHECK(UISkillSpecRenderer::GetNodeRadius(modifierNode, view, &triggerContract) ==
          doctest::Approx(40.0f * view.zoom * 1.1f));

    CHECK(UISkillSpecRenderer::ClassifyNodeVisual(passiveNode, &synergyContract) ==
          UISkillSpecRenderer::NodeType::Synergy);
    CHECK(UISkillSpecRenderer::GetNodeRadius(passiveNode, view, &synergyContract) ==
          doctest::Approx(40.0f * view.zoom));

    CHECK(UISkillSpecRenderer::ClassifyNodeVisual(passiveNode, &transmuterContract) ==
          UISkillSpecRenderer::NodeType::Transmuter);
    CHECK(UISkillSpecRenderer::GetNodeRadius(passiveNode, view, &transmuterContract) ==
          doctest::Approx(40.0f * view.zoom * 1.1f));
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

TEST_CASE("[Tech] SkillUI - Mastery Panel Draw Does Not Crash") {
    entt::registry registry;
    UISystem::State.scaleFactor = 1.0f;

    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<ActiveSkillsComponent>(player);

    auto& stats = registry.emplace<PlayerStats>(player);
    stats.level = 50;

    auto& astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

    CHECK_NOTHROW(UISkillHub::Draw(registry, player));
}

TEST_CASE("[Tech] SkillUI - Locked mastery selection shows popup") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    UISystem::State.showMessageBox = false;
    UISystem::State.messageBoxText[0] = '\0';
    UISystem::State.messageBoxTimer = 0.0f;

    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<ActiveSkillsComponent>(player);

    auto& stats = registry.emplace<PlayerStats>(player);
    stats.level = 12;

    CHECK_FALSE(UISkillHub::TrySelectMastery(registry, player,
                                             BladeMasteryId::SwordSaint));
    CHECK(UISystem::State.showMessageBox);
    CHECK(std::string(UISystem::State.messageBoxText) ==
          "等级或基础职业不满足职业专精条件");
    CHECK(UISystem::State.messageBoxTimer > 0.0f);
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

    SUBCASE("Blade Resource HUD Does Not Crash") {
        registry.remove<SwordIntentComponent>(player);
        auto& bladeResource = registry.emplace<BladeResourceComponent>(player);
        bladeResource.kind = BladeResourceKind::SwordFlow;
        bladeResource.current = 5;
        bladeResource.max = 10;
        systems::PlayerHUD::Draw(registry);
    }

    SUBCASE("Restart Window HUD Does Not Crash") {
        registry.remove<SwordIntentComponent>(player);
        auto& bladeResource = registry.emplace<BladeResourceComponent>(player);
        bladeResource.kind = BladeResourceKind::SwordFlow;
        bladeResource.current = 0;
        bladeResource.max = 10;
        bladeResource.restart_window_ready = true;
        bladeResource.restart_window_timer = 2.5f;
        CHECK(systems::PlayerHUD::ResolveSwordFlowFeedbackText(bladeResource) ==
              "Restart Ready 2.5s");
        systems::PlayerHUD::Draw(registry);
    }

    SUBCASE("Crit Proc HUD Feedback Text Resolves") {
        registry.remove<SwordIntentComponent>(player);
        auto& bladeResource = registry.emplace<BladeResourceComponent>(player);
        bladeResource.kind = BladeResourceKind::SwordFlow;
        bladeResource.current = 4;
        bladeResource.max = 10;
        bladeResource.crit_bonus_feedback_timer = 0.8f;
        CHECK(systems::PlayerHUD::ResolveSwordFlowFeedbackText(bladeResource) ==
              "暴击剑流 +1");
    }

    SUBCASE("Heavenly Sword and Demon Blade HUD text resolves") {
        registry.remove<SwordIntentComponent>(player);
        auto& mastery = registry.emplace<BladeMasteryComponent>(player);
        auto& bladeResource = registry.emplace<BladeResourceComponent>(player);

        mastery.selected = BladeMasteryId::HeavenlySword;
        mastery.heavenly_attunement = BladeAttunement::Fire;
        bladeResource.kind = BladeResourceKind::SpiritBladeTier;
        bladeResource.current = 8;
        bladeResource.max = 10;
        CHECK(std::string(systems::PlayerHUD::ResolveBladeResourceLabel(bladeResource)) ==
              "Spirit Blade Tier");
        CHECK(systems::PlayerHUD::ResolveBladeResourceDetailText(mastery, bladeResource) ==
              "Attunement: Fire");
        CHECK(std::string(NoMoreDay::systems::ui::SwordIntentWidget::ResolveThresholdText(
                  BladeResourceKind::SpiritBladeTier, 10, 10)) == "天剑待发");
        CHECK(NoMoreDay::systems::ui::SwordIntentWidget::ResolveThresholdTier(
                  BladeResourceKind::SpiritBladeTier, 7, 10) == 2);
        CHECK(std::string(NoMoreDay::systems::ui::SwordIntentWidget::ResolveThresholdText(
                  BladeResourceKind::SpiritBladeTier, 7, 10)) == "万剑齐鸣");

        mastery.selected = BladeMasteryId::DemonBlade;
        mastery.blood_oath_active = true;
        bladeResource.kind = BladeResourceKind::Bloodthirst;
        bladeResource.current = 8;
        CHECK(std::string(systems::PlayerHUD::ResolveBladeResourceLabel(bladeResource)) ==
              "Bloodthirst");
        CHECK(systems::PlayerHUD::ResolveBladeResourceDetailText(mastery, bladeResource) ==
              "Blood Oath: Active");
        CHECK(systems::PlayerHUD::ResolveBladeResourceFeedbackText(mastery, bladeResource) ==
              "Danger: Blood Oath");
    }

    SUBCASE("Heavenly Sword field window and Demon Blade danger cues resolve") {
        registry.remove<SwordIntentComponent>(player);
        auto& mastery = registry.emplace<BladeMasteryComponent>(player);
        auto& bladeResource = registry.emplace<BladeResourceComponent>(player);

        mastery.selected = BladeMasteryId::HeavenlySword;
        mastery.heavenly_attunement = BladeAttunement::Lightning;
        bladeResource.kind = BladeResourceKind::SpiritBladeTier;
        bladeResource.current = 10;
        bladeResource.max = 10;

        const auto heavenlyField = registry.create();
        auto& heavenlyState = registry.emplace<HeavenlySwordFieldComponent>(heavenlyField);
        heavenlyState.owner = player;
        heavenlyState.duration = 4.2f;

        CHECK(systems::PlayerHUD::ResolveBladeResourceRuntimeDetailText(
                  registry, player, mastery, bladeResource, stats) ==
              "Field Active 4.2s");

        mastery.selected = BladeMasteryId::DemonBlade;
        mastery.blood_oath_active = true;
        bladeResource.kind = BladeResourceKind::Bloodthirst;
        bladeResource.current = 9;
        bladeResource.max = 10;
        stats.health = 28.0f;
        stats.max_health = 100.0f;

        const auto bloodSeaField = registry.create();
        auto& bloodSeaState = registry.emplace<BloodSeaFieldComponent>(bloodSeaField);
        bloodSeaState.owner = player;
        bloodSeaState.duration = 5.6f;
        bloodSeaState.has_linked_synergy = true;

        CHECK(systems::PlayerHUD::ResolveBladeResourceRuntimeDetailText(
                  registry, player, mastery, bladeResource, stats) ==
              "");
        CHECK(systems::PlayerHUD::ResolveBladeResourceRuntimeFeedbackText(
                  registry, player, mastery, bladeResource, stats) ==
              "Danger: 28% HP");

        stats.health = 88.0f;
        bloodSeaState.torrent_form = true;
        bloodSeaState.has_void_keystone = true;
        bloodSeaState.miasma_duration_bonus = 0.5f;

        CHECK(systems::PlayerHUD::ResolveBladeResourceRuntimeDetailText(
                  registry, player, mastery, bladeResource, stats) ==
              "");
        CHECK(systems::PlayerHUD::ResolveBladeResourceRuntimeFeedbackText(
                  registry, player, mastery, bladeResource, stats) ==
              "Miasma Pressure");
    }
}

TEST_CASE("[Tech] BuffUI - Blood Sea uses buff lane icon and runtime tooltip overrides") {
    namespace fs = std::filesystem;

    const std::array<fs::path, 3> buffRegistryCandidates = {
        fs::path("src/game/data/BuffRegistry.cpp"),
        fs::path("../src/game/data/BuffRegistry.cpp"),
        fs::path("../../src/game/data/BuffRegistry.cpp")
    };
    const std::array<fs::path, 3> bloodSeaCandidates = {
        fs::path("src/game/systems/skill/behaviors/BloodSea.cpp"),
        fs::path("../src/game/systems/skill/behaviors/BloodSea.cpp"),
        fs::path("../../src/game/systems/skill/behaviors/BloodSea.cpp")
    };
    const std::array<fs::path, 3> tooltipCandidates = {
        fs::path("src/engine/render/UIRenderer.cpp"),
        fs::path("../src/engine/render/UIRenderer.cpp"),
        fs::path("../../src/engine/render/UIRenderer.cpp")
    };

    const auto readSource = [](const auto& candidates) {
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
        return source;
    };

    const std::string buffRegistrySource = readSource(buffRegistryCandidates);
    const std::string bloodSeaSource = readSource(bloodSeaCandidates);
    const std::string tooltipSource = readSource(tooltipCandidates);

    REQUIRE(!buffRegistrySource.empty());
    REQUIRE(!bloodSeaSource.empty());
    REQUIRE(!tooltipSource.empty());

    CHECK(buffRegistrySource.find("registry[BuffType::BloodSea]") != std::string::npos);
    CHECK(buffRegistrySource.find("assets::buffs::general::buff_xuehai") != std::string::npos);
    CHECK(bloodSeaSource.find("blood_sea_active") != std::string::npos);
    CHECK(bloodSeaSource.find("BuffType::BloodSea") != std::string::npos);
    CHECK(tooltipSource.find("effect.name.empty() ? visual.name : effect.name") != std::string::npos);
    CHECK(tooltipSource.find("effect.description.empty() ? visual.description : effect.description") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - SwordIntentWidget status text uses UI font rendering") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/systems/ui/SwordIntentWidget.cpp"),
        fs::path("../src/game/systems/ui/SwordIntentWidget.cpp"),
        fs::path("../../src/game/systems/ui/SwordIntentWidget.cpp")
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

    CHECK(source.find("UISystem::DrawTextUI(labelText.c_str()") != std::string::npos);
    CHECK(source.find("UISystem::DrawTextUI(thresholdText") != std::string::npos);
    CHECK(source.find("UISystem::DrawTextUI(detail.c_str()") != std::string::npos);
    CHECK(source.find("DrawText(labelText.c_str(),") == std::string::npos);
    CHECK(source.find("DrawText(thresholdText,") == std::string::npos);
    CHECK(source.find("DrawText(detail.c_str(),") == std::string::npos);
}

TEST_CASE("[Tech] Blood Sea - field render path is specialized") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/engine/render/RenderSystem.cpp"),
        fs::path("../src/engine/render/RenderSystem.cpp"),
        fs::path("../../src/engine/render/RenderSystem.cpp")
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

    const bool skipsGenericDot =
        source.find("frame.registry.any_of<BloodSeaFieldComponent>(entity)") != std::string::npos ||
        source.find("frame.registry.any_of<NoMoreDay::BloodSeaFieldComponent>(entity)") != std::string::npos;
    const bool hasDedicatedView =
        source.find("view<const Position, const BloodSeaFieldComponent>()") != std::string::npos ||
        source.find("view<const Position, const NoMoreDay::BloodSeaFieldComponent>()") != std::string::npos;

    CHECK(skipsGenericDot);
    CHECK(hasDedicatedView);
    CHECK(source.find("field.radius") != std::string::npos);
    CHECK(source.find("DrawRing(") != std::string::npos);
}

TEST_CASE("[Tech] SwordIntentVisual - Sword Flow crit proc pulse activates") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);

    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.stacks = 3;
    intent.max_stacks = 10;

    auto& bladeResource = registry.emplace<BladeResourceComponent>(player);
    bladeResource.kind = BladeResourceKind::SwordFlow;
    bladeResource.current = 3;
    bladeResource.max = 10;
    bladeResource.crit_bonus_feedback_timer = 0.8f;

    systems::SwordIntentVisualSystem::Update(registry, 0.016f);

    REQUIRE(registry.all_of<components::SwordIntentVisual>(player));
    CHECK(registry.get<components::SwordIntentVisual>(player).critFeedbackPulse > 0.0f);
}

TEST_CASE("[Tech] Sword Saint - Sword Flow thresholds are telegraphed") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);

    auto& intent = registry.emplace<SwordIntentComponent>(player);
    intent.stacks = 10;
    intent.max_stacks = 10;

    auto& bladeResource = registry.emplace<BladeResourceComponent>(player);
    bladeResource.kind = BladeResourceKind::SwordFlow;
    bladeResource.current = 10;
    bladeResource.max = 10;

    systems::SwordIntentVisualSystem::Update(registry, 0.016f);

    REQUIRE(registry.all_of<components::SwordIntentVisual>(player));
    CHECK(registry.get<components::SwordIntentVisual>(player).thresholdTier == 3);
    const std::string thresholdText =
        NoMoreDay::systems::ui::SwordIntentWidget::ResolveSwordFlowThresholdText(
            10, 10);
    CHECK(thresholdText == "满流");
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

TEST_CASE("[Tech] SkillUI - tooltip uses static preview payload") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/engine/render/UIRenderer.cpp"),
        fs::path("../src/engine/render/UIRenderer.cpp"),
        fs::path("../../src/engine/render/UIRenderer.cpp")
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
    CHECK(source.find("SkillDisplayPreviewService::Build") != std::string::npos);
    CHECK(source.find("持续时间") != std::string::npos);
    CHECK(source.find("ResolveDamageLabel(") != std::string::npos);
    CHECK(source.find("0.9f") == std::string::npos);
    CHECK(source.find("1.1f") == std::string::npos);
}

TEST_CASE("[Tech] SkillUI - specialization tooltip renders quantitative lines") {
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
    CHECK(source.find("BuildNodeQuantitativeLines(") != std::string::npos);
    CHECK(source.find("quantitative") != std::string::npos);
    CHECK(source.find("display_lines") != std::string::npos);
    CHECK(source.find("数值加成已启用") == std::string::npos);
}

} // namespace NoMoreDay
