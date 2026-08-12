#pragma once
#include "TestCommon.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/OverlayController.hpp"
#include "game/application/ui/UIAnimationSystem.hpp"
#include "game/application/ui/UISkillSpecRenderer.hpp"
#include "game/application/ui/UiRuntimeTypes.hpp"
#include "game/foundation/components/UIAnimationComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/application/ui/UISkillHub.hpp"
#include "game/application/ui/UISkillTalentTree.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/BladeMasteryRegistry.hpp"
#include "game/foundation/components/vfx/SwordIntentVisualComponent.hpp"
#include "game/application/ui/MonsterHealthBarSystem.hpp"
#include "game/systems/vfx/SwordIntentVisualSystem.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/application/ui/PlayerHUD.hpp"
#include "game/application/ui/SwordIntentWidget.hpp"
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

    // U8 final: drag state lives in the host-owned UIDragSession (the legacy
    // UISystem::State drag fields are gone).
    NoMoreDay::ui::GameUiHost host;
    auto& drag = host.DragSession();
    drag.isDraggingSkill = true;
    drag.draggedSkillId = 1; // Assuming skill 1 exists

    // Simulation logic omitted but structure remains for verification
}

TEST_CASE("[Tech] SkillUI - Context Menu State") {
    // U8 final: the skill context menu state lives in the hosted overlay
    // controller (the legacy UISystem::State fields are gone).
    NoMoreDay::ui::UiRuntime runtime;
    NoMoreDay::ui::OverlayController overlay(runtime);

    // Simulate opening context menu
    overlay.OpenSkillContextMenu(3);

    CHECK(overlay.IsContextMenuVisible() == true);
    CHECK(overlay.IsSkillContext() == true);
    CHECK(overlay.ContextSourceSkillSlot() == 3);
}

TEST_CASE("[Tech] SkillUI - UISkillTalentTree scissor scope uses exactly one pair") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/application/ui/UISkillTalentTree.cpp")
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
        fs::path("src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/application/ui/UISkillTalentTree.cpp")
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
        fs::path("src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/application/ui/UISkillTalentTree.cpp")
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
        fs::path("src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/application/ui/UISkillTalentTree.cpp")
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
        fs::path("src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/application/ui/UISkillTalentTree.cpp")
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
        fs::path("src/game/application/ui/UIRenderer.cpp"),
        fs::path("../src/game/application/ui/UIRenderer.cpp"),
        fs::path("../../src/game/application/ui/UIRenderer.cpp")
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

TEST_CASE("[Tech] InventoryUI - equipment replacement routes inventory drags "
          "through the transactional handler, not the paint path (R6)") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/application/ui/UIInventoryController.cpp"),
        fs::path("../src/game/application/ui/UIInventoryController.cpp"),
        fs::path("../../src/game/application/ui/UIInventoryController.cpp")
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

    // R6: the equipment drop no longer mutates the registry during paint.
    // The controller enqueues an EquipItem intent carrying the drag source
    // (inventory slot index + itemSource) and the target equipment slot; the
    // GameUiCommandHandler re-resolves and routes inventory-origin drags
    // through InventorySystem::swapInventoryItemIntoEquipment (transactional
    // swap). The old Draw-phase needles are gone.
    const size_t equipIntentPos = source.find("GameUiIntentKind::EquipItem");
    REQUIRE(equipIntentPos != std::string::npos);
    const size_t itemSourcePos = source.find("intent.payload.itemSource", equipIntentPos);
    const size_t sourceSlotPos = source.find("intent.payload.sourceSlot", equipIntentPos);
    CHECK(itemSourcePos != std::string::npos);
    CHECK(sourceSlotPos != std::string::npos);
    CHECK(source.find("InventorySystem::equipItem(") == std::string::npos);
    CHECK(source.find("IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && drag.draggedItemDomainId != 0") == std::string::npos);
}

TEST_CASE("[Tech] InventoryUI - gameplay fallback does not clear drags while inventory overlay is active") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/application/states/GameplayState.cpp"),
        fs::path("../src/game/application/states/GameplayState.cpp"),
        fs::path("../../src/game/application/states/GameplayState.cpp")
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

    // R8: the legacy call position is now registry-free (the phantom and the
    // tooltip paint from the drag session + frame snapshot inside Draw).
    const size_t cleanupPos = source.find("m_uiHost->DrawDraggingPhantom();");
    REQUIRE(cleanupPos != std::string::npos);

    // The repo sources are CRLF-terminated, so the source-text needle must
    // use \r\n to match the actual file bytes.
    const size_t releaseGuardPos = source.find("if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) &&\r\n      !m_uiHost->IsInventoryVisible())", cleanupPos);

    CHECK(releaseGuardPos != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - shared mastery theme plumbing guards hub and tree chrome") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 4> candidates = {
        fs::path("src/game/application/ui/UISkillHub.cpp"),
        fs::path("src/game/application/ui/UISkillSpecRenderer.hpp"),
        fs::path("src/game/application/ui/UISkillSpecRenderer.cpp"),
        fs::path("src/game/application/ui/UISkillTalentTree.cpp")
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

    // R8: the hub reads mastery view models (GameUiMasteryCardView) whose
    // field is masteryId, so the theme lookup casts that view id; the
    // plumbing contract (theme driven by the mastery id) is unchanged.
    CHECK(sources[0].find("GetBladeMasteryUIThemeProfile(static_cast<BladeMasteryId>(profile.masteryId))") != std::string::npos);
    CHECK(sources[1].find("ClassifyNodeVisual") != std::string::npos);
    CHECK(sources[2].find("GetBladeMasteryUIThemeProfile(tree->mastery_id)") != std::string::npos);
    CHECK(sources[2].find("ClassifyNodeVisual") != std::string::npos);
    // R8: the talent tree no longer re-implements node classification; its
    // painter delegates node drawing to UISkillSpecRenderer::Draw, which is
    // the single home of ClassifyNodeVisual (locked above).
    CHECK(sources[3].find("UISkillSpecRenderer::Draw(tree") != std::string::npos);
    CHECK(sources[3].find("GetBladeMasteryUIThemeProfile(tree->mastery_id)") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - mastery hub exposes Heavenly Sword attunement controls") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/application/ui/UISkillHub.cpp"),
        fs::path("../src/game/application/ui/UISkillHub.cpp"),
        fs::path("../../src/game/application/ui/UISkillHub.cpp")
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
    // R8: the attunement write moved out of the hub into the command handler
    // (SkillSetAttunement intent). The hub keeps the UI-only hover/highlight
    // chrome; the authoritative write must stay behind the handler so the hub
    // surface stays registry-free.
    CHECK(source.find("SkillSetAttunement") != std::string::npos);
}

TEST_CASE("[Tech] SkillUI - mastery hub locks all Blade Ascendant signature skills consistently") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/application/ui/UISkillHub.cpp"),
        fs::path("../src/game/application/ui/UISkillHub.cpp"),
        fs::path("../../src/game/application/ui/UISkillHub.cpp")
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
    // Signature-skill detection is data-driven (BladeMasteryProfile::signature_skill_id),
    // so no hardcoded skill-id literals may reappear.
    CHECK(source.find("id == 10 || id == 11 || id == 12") == std::string::npos);
    // R8: the locked-signature set is resolved by the snapshot builder (the
    // single registry read point; see GameUiSnapshotBuilder.cpp) and the hub
    // reads it from the snapshot. The hub keeps the lock badge flag; the
    // registry-touching detection must not reappear in the hub.
    CHECK(source.find("signatureLocked") != std::string::npos);
    CHECK(source.find("lockedSignatureSkills") != std::string::npos);
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
    // R8: the hub is a snapshot surface — the draw path is the registered
    // painter (PaintCanvas), fed by the paint state UpdateInput captured from
    // the frame snapshot. The registry is gone from the hub API; the smoke
    // drives UpdateInput + PaintCanvas with a minimal snapshot instead.
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    NoMoreDay::ui::GameUiHost host;
    NoMoreDay::UISkillHub hub;
    hub.SetHost(&host);

    NoMoreDay::ui::GameUiSnapshot snapshot;
    snapshot.player.hasPlayer = true;
    snapshot.player.level = 50;
    snapshot.skillTree.hasBladeProfession = true;
    snapshot.skillTree.availableTalentPoints = 3;

    NoMoreDay::ui::UiInputFrame input;
    input.deltaSeconds = 0.016f;
    input.tooltipTarget = NoMoreDay::ui::kInvalidUiId;

    CHECK_NOTHROW(hub.UpdateInput(snapshot, input, 1.0f));
    CHECK_NOTHROW(hub.PaintCanvas(
        NoMoreDay::ui::UiRect{{0.0f, 0.0f}, {1280.0f, 720.0f}}));
}

TEST_CASE("[Tech] SkillUI - Locked mastery selection shows popup") {
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    REQUIRE(data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    // R8: the mastery selection is a SkillSelectMastery intent executed by the
    // command handler; its failure notification is the contractual popup text
    // the host surfaces through the message box on the next Update (was
    // hub.TrySelectMastery, which wrote gameplay directly).
    NoMoreDay::ui::GameUiCommandHandler handler;
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<ActiveSkillsComponent>(player);
    auto& stats = registry.emplace<PlayerStats>(player);
    stats.level = 12;

    NoMoreDay::ui::GameUiIntent intent;
    intent.sourceNode = NoMoreDay::ui::kInvalidUiId;
    intent.kind = NoMoreDay::ui::GameUiIntentKind::SkillSelectMastery;
    intent.payload.masteryId =
        static_cast<std::uint8_t>(BladeMasteryId::SwordSaint);

    const auto result = handler.Execute(registry, intent);
    CHECK_FALSE(result.success);
    CHECK(result.notification == "等级或基础职业不满足职业专精条件");
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
    
    // U8 final: scale comes from UISystem::GetScaleFactor() (UIRenderer
    // internal scale) with a safe default; no State field to preset.

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
        fs::path("src/game/foundation/data/BuffRegistry.cpp"),
        fs::path("../src/game/foundation/data/BuffRegistry.cpp"),
        fs::path("../../src/game/foundation/data/BuffRegistry.cpp")
    };
    const std::array<fs::path, 3> bloodSeaCandidates = {
        fs::path("src/game/systems/skill/behaviors/BloodSea.cpp"),
        fs::path("../src/game/systems/skill/behaviors/BloodSea.cpp"),
        fs::path("../../src/game/systems/skill/behaviors/BloodSea.cpp")
    };
    const std::array<fs::path, 3> tooltipCandidates = {
        fs::path("src/game/application/ui/UIRenderer.cpp"),
        fs::path("../src/game/application/ui/UIRenderer.cpp"),
        fs::path("../../src/game/application/ui/UIRenderer.cpp")
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
        fs::path("src/game/application/ui/SwordIntentWidget.cpp"),
        fs::path("../src/game/application/ui/SwordIntentWidget.cpp"),
        fs::path("../../src/game/application/ui/SwordIntentWidget.cpp")
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

    // R5 adaptation: the widget paints into the UiDrawList (backend renders
    // text with the UI font). The guard now asserts the draw-list contract
    // and that no raylib immediate-mode text call sneaks back in.
    CHECK(source.find("drawList.Text(UiDrawLayer::Hud") != std::string::npos);
    CHECK(source.find("kGlobalFontResourceId") != std::string::npos);
    CHECK(source.find("drawList.Image(UiDrawLayer::Hud") != std::string::npos);
    CHECK(source.find("DrawText(labelText.c_str(),") == std::string::npos);
    CHECK(source.find("DrawText(thresholdText,") == std::string::npos);
    CHECK(source.find("DrawText(detail.c_str(),") == std::string::npos);
}

TEST_CASE("[Tech] Blood Sea - field render path is specialized") {
    namespace fs = std::filesystem;
    const std::array<fs::path, 3> candidates = {
        fs::path("src/game/application/render/GameplayRenderAdapter.cpp"),
        fs::path("../src/game/application/render/GameplayRenderAdapter.cpp"),
        fs::path("../../src/game/application/render/GameplayRenderAdapter.cpp")
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
        fs::path("src/game/application/ui/UIRenderer.cpp"),
        fs::path("../src/game/application/ui/UIRenderer.cpp"),
        fs::path("../../src/game/application/ui/UIRenderer.cpp")
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
        fs::path("src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../src/game/application/ui/UISkillTalentTree.cpp"),
        fs::path("../../src/game/application/ui/UISkillTalentTree.cpp")
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

TEST_CASE("[Tech] R2 - production GameUiHost has no test-data injection") {
    // D-01 (R2): the production host must not contain the removed one-shot
    // test item / skill / buff grant (m_hasGivenTestItems and the test_*
    // buff ids), and must not create a bag for injection. createBag is a
    // legitimate production API used by real game systems, so the guard is
    // scoped to the host sources only (the narrowest expression): the host
    // itself must never call it.
    namespace fs = std::filesystem;
    const std::array<fs::path, 6> candidates = {
        fs::path("src/game/application/ui/GameUiHost.cpp"),
        fs::path("../src/game/application/ui/GameUiHost.cpp"),
        fs::path("../../src/game/application/ui/GameUiHost.cpp"),
        fs::path("src/game/application/ui/GameUiHost.hpp"),
        fs::path("../src/game/application/ui/GameUiHost.hpp"),
        fs::path("../../src/game/application/ui/GameUiHost.hpp"),
    };

    std::string source;
    for (const auto& candidate : candidates) {
        if (!fs::exists(candidate)) {
            continue;
        }
        std::ifstream in(candidate, std::ios::in | std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        source += ss.str();
    }
    REQUIRE(!source.empty());

    const char* forbiddenTokens[] = {
        "m_hasGivenTestItems", "test_power", "test_speed",
        "test_stun",           "test_poison",
    };
    for (const char* needle : forbiddenTokens) {
        CHECK_MESSAGE(source.find(needle) == std::string::npos, "host contains ",
                      needle);
    }
    CHECK(source.find("ItemFactory::createBag") == std::string::npos);
}

} // namespace NoMoreDay
