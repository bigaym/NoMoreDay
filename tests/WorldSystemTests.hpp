#pragma once
#include "TestCommon.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/world/LevelManager.hpp"
#include "game/systems/world/PortalSystem.hpp"
#include "game/systems/world/MovementStanceSystem.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/data/TagRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/scene/SceneManager.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/Common.hpp"
#include "game/components/Buff.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/data/AstrolabeRegistry.hpp"
#include "game/components/Progression.hpp"
#include <entt/entt.hpp>
#include <chrono>
#include <thread>

namespace NoMoreDay {

TEST_CASE("Stats Recalculation and Modifiers") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<PlayerTag>(entity);
    registry.emplace<CombatStats>(entity);
    registry.emplace<PrimaryStats>(entity, PrimaryStats{.strength = 10.0f, .vitality = 15.0f});
    registry.emplace<StatsDirty>(entity);

    SUBCASE("Basic Derivation") {
        StatsSystem::update(registry);
        const auto& combat = registry.get<CombatStats>(entity);
        CHECK(combat.armor == doctest::Approx(20.0f)); // 10 Str * 2
    }

    SUBCASE("Modifier Stacking") {
        ModifierList mods;
        mods.modifiers.push_back({StatType::MaxHealth, ModifierMode::Flat, 50.0f});
        mods.modifiers.push_back({StatType::MaxHealth, ModifierMode::PercentAdd, 10.0f});
        registry.emplace<ModifierList>(entity, mods);
        
        StatsSystem::update(registry);
        const auto& combat = registry.get<CombatStats>(entity);
        float base_hp = Constants::Combat::DEFAULT_MAX_HEALTH;
        float expected = (base_hp + 15.0f * 15.0f + 50.0f) * 1.1f;
        CHECK(combat.max_health == doctest::Approx(expected));
    }
}

TEST_CASE("Biome and Level Management") {
    LoggerScope scope;
    auto& registry = BiomeRegistry::Get();
    registry.LoadFromJSON("assets/data/biomes.json");

    SUBCASE("Biome Data Retrieval") {
        CHECK(registry.HasBiome("town"));
        const auto& town = registry.GetBiome("town");
        CHECK(town.id == "town");
        CHECK(town.isSafeZone == true);
    }

    SUBCASE("LevelManager Biome Loading") {
        LevelManager levelManager;
        levelManager.loadNewLevel("town", 50, 50);
        CHECK(levelManager.getCurrentBiome() == "town");
    }
}

TEST_CASE("PortalSystem and Scene Transitions") {
    entt::registry registry;
    LevelManager lm;
    SceneManager sm(lm, registry);
    PortalSystem ps(sm);
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 100.0f, 100.0f);
    
    auto portal = registry.create();
    PortalComponent pc;
    pc.targetBiome = "town";
    pc.targetLevel = 1;
    registry.emplace<PortalComponent>(portal, pc);
    registry.emplace<Position>(portal, 100.0f, 100.0f);
    
    ps.Update(registry, 0.1f);
    CHECK(sm.IsTransitioning() == true);
}

TEST_CASE("Movement Stance Logic") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<MovementStanceComponent>(player);
    registry.emplace<CombatStats>(player);

    SUBCASE("Transition to Sword Riding") {
        auto& vel = registry.get<Velocity>(player);
        vel.vx = 300.0f;
        MovementStanceSystem::Update(registry, 2.1f);
        auto& stance = registry.get<MovementStanceComponent>(player);
        CHECK(stance.stance == MovementStance::SwordRiding);
    }
}

TEST_CASE("Tag System Verification") {
    SUBCASE("Bitmask Ops") {
        Tag comb = Tag::Physical | Tag::Fire;
        CHECK(HasTag(comb, Tag::Physical));
        CHECK(HasTag(comb, Tag::Fire));
    }

    SUBCASE("String Parsing") {
        auto res = TagFromString("physical");
        REQUIRE(res.has_value());
        CHECK(res.value() == Tag::Physical);
    }
}

TEST_CASE("Persistence - State Survival") {
    LoggerScope scope;
    entt::registry registry;
    LevelManager levelManager;
    SceneManager sceneManager(levelManager, registry);
    BiomeRegistry::Get().LoadFromJSON("assets/data/biomes.json");
    
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<PersistentTag>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    
    sceneManager.RequestTransition("cave", 1);
    sceneManager.Update(1.0f); // FADE_OUT
    sceneManager.Update(0.1f); // LOADING
    
    CHECK(registry.valid(player));
}

TEST_CASE("AssetLoadingSystem initialization") {
    LoggerScope scope;
    ResourceManager resourceManager;
    resourceManager.SetHeadless(true);
    
    AssetLoadingSystem::Initialize(resourceManager);
    Texture2D loaded = AssetLoadingSystem::LoadUITexture(123, "dummy_path");
    CHECK(loaded.id == 1);
    AssetLoadingSystem::Shutdown();
}

} // namespace NoMoreDay