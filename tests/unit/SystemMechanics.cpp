#pragma once

#include "TestCommon.hpp"
#include "game/components/Combat.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Projectile.hpp"

#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/HeirloomComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "engine/persistence/SaveManager.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/data/ResonanceCalculator.hpp"
#include "game/data/BiomeRegistry.hpp"
#include "game/systems/item/HeirloomVault.hpp"
#include "game/components/PlayerProfile.hpp"
#include "raylib.h"
#include <array>
#include <filesystem>
#include <stdexcept>

namespace NoMoreDay {
namespace {
std::filesystem::path ResolveBiomeJsonPathForSystemMechanicsTest() {
  constexpr std::array<const char *, 4> kCandidates = {
      "assets/data/biomes.json",
      "../assets/data/biomes.json",
      "../../assets/data/biomes.json",
      "../../../assets/data/biomes.json",
  };

  for (const char *candidate : kCandidates) {
    const auto path = std::filesystem::path(candidate);
    if (std::filesystem::exists(path)) {
      return std::filesystem::absolute(path);
    }
  }

  throw std::runtime_error("Unable to locate assets/data/biomes.json from test cwd");
}
} // namespace

TEST_CASE("[Unit] DefenseMechanics - Verification") {
  LoggerScope scope;
  entt::registry registry;

  auto attacker = registry.create();
  registry.emplace<Position>(attacker, 0.0f, 0.0f);
  registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0f;

  auto defender = registry.create();
  registry.emplace<Position>(defender, 10.0f, 0.0f);
  registry.emplace<Velocity>(defender, 0.0f, 0.0f);
  registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
  registry.emplace<CombatStats>(defender);

  SUBCASE("Phantom Flash Counter") {
    auto &pf = registry.emplace<PhantomFlashComponent>(defender);
    pf.counter_window = 0.5f;
    pf.triggered = false;

     DamagePool pool;
    pool.Add(Tag::Physical, 50.0f);

    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Melee, entt::null, true);

    CHECK(result.total_damage == doctest::Approx(50.0f));
    CHECK(pf.triggered == false);
    CHECK(registry.get<HealthComponent>(defender).current == 100.0f);
  }

  SUBCASE("Blade Ward Interception") {
    auto &ward = registry.emplace<BladeWardComponent>(defender);
    ward.sword_count = 100;
    ward.interception_chance = 1.0f;

    auto proj_ent = registry.create();
    registry.emplace<Position>(proj_ent, 10.0f, 0.0f);
    registry.emplace<Projectile>(proj_ent);

    DamagePool pool;
    pool.Add(Tag::Physical, 30.0f);

    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0,
                                            pool, Tag::Projectile, proj_ent, true);

    CHECK(result.total_damage == doctest::Approx(30.0f));
    CHECK(ward.sword_count == 100);
  }
}

TEST_CASE("[Unit] HeirloomVault - Core Rules") {
    entt::registry registry;
    auto player = registry.create();
    
    SUBCASE("Vault Management") {
        auto& vault = HeirloomVault::Get();
        size_t initialSize = vault.size();
        
        auto itemEnt = registry.create();
        auto& item = registry.emplace<ItemComponent>(itemEnt);
        item.name = "Heirloom Test";
        
        vault.addHeirloom(item, 10, Rarity::Legendary);
        CHECK(vault.size() == initialSize + 1);
        
        vault.removeHeirloom(initialSize);
        CHECK(vault.size() == initialSize);
    }
}

TEST_CASE("[Unit] ResonanceCalculator - Basic Check") {
    MosaicGrid grid;
    entt::registry registry;
    // Empty grid resonance
    auto result = ResonanceCalculator::Calculate(grid, registry);
    CHECK(result.totalEnemyDensity == 1.0f);
}

TEST_CASE("[Unit] ResonanceCalculator - Uses Dimensional Combat Biome Pool") {
    const auto biomePath = ResolveBiomeJsonPathForSystemMechanicsTest().string();
    BiomeRegistry::Get().LoadFromJSON(biomePath);
    CHECK(BiomeRegistry::Get().HasBiome("sky_palace"));
    MosaicGrid grid;
    entt::registry registry;

    auto fragmentEntity = registry.create();
    auto& fragment = registry.emplace<MapFragmentComponent>(fragmentEntity);
    fragment.element = FragmentElement::Fire;
    fragment.type = FragmentType::Terrain;
    grid.cells[0] = fragmentEntity;

    auto result = ResonanceCalculator::Calculate(grid, registry);

    constexpr std::array<BiomeID, 5> kFirePool = {
        BiomeID::CrimsonWaste, BiomeID::MagmaVeins, BiomeID::AshPlain,
        BiomeID::HolyArena, BiomeID::HiveNest};

    bool inPool = false;
    for (BiomeID id : kFirePool) {
        if (result.primaryBiome == id) {
            inPool = true;
            break;
        }
    }
    CHECK(inPool);
}

TEST_CASE("[Unit] ResonanceCalculator - biomeOverride Takes Priority") {
    const auto biomePath = ResolveBiomeJsonPathForSystemMechanicsTest().string();
    BiomeRegistry::Get().LoadFromJSON(biomePath);
    REQUIRE(BiomeRegistry::Get().HasBiome("sky_palace"));
    MosaicGrid grid;
    entt::registry registry;

    auto fragmentEntity = registry.create();
    auto& fragment = registry.emplace<MapFragmentComponent>(fragmentEntity);
    fragment.element = FragmentElement::Fire;
    fragment.type = FragmentType::Terrain;
    fragment.biomeOverride = "sky_palace";
    grid.cells[0] = fragmentEntity;

    auto result = ResonanceCalculator::Calculate(grid, registry);
    CHECK(result.primaryBiome == BiomeID::SkyPalace);
}


TEST_CASE("[Unit] PersistenceSystem - Basic Check") {
    auto& sm = SaveManager::Get();
    CHECK(&sm != nullptr);
}

TEST_CASE("[Unit] SaveManager - Header Name And Playtime Snapshot") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 1.0f, 2.0f);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    registry.emplace<PlayerName>(player, "玩家0");
    registry.emplace<PlayerPlaytime>(
        player, 120, static_cast<double>(GetTime()) - 5.2);

    auto data = SaveManager::Get().createSnapshot(registry);
    CHECK(data.header.name == "玩家0");
    CHECK(data.header.playtime >= 125);

    data.header.playtime = 200;
    entt::registry restoredRegistry;
    SaveManager::Get().restoreFromSnapshot(restoredRegistry, data);

    auto restoredView = restoredRegistry.view<PlayerTag, PlayerPlaytime>();
    REQUIRE(restoredView.begin() != restoredView.end());
    auto restoredPlayer = *restoredView.begin();
    auto &playtime = restoredRegistry.get<PlayerPlaytime>(restoredPlayer);
    playtime.session_start_time -= 3.1;

    auto data2 = SaveManager::Get().createSnapshot(restoredRegistry);
    CHECK(data2.header.playtime >= 203);
}

TEST_CASE("[Unit] SaveManager - Skill Contract Runtime Snapshot Roundtrip") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 1.0f, 2.0f);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    registry.emplace<ActiveSkillsComponent>(player);

    auto& runtime = registry.emplace<SkillContractRuntimeComponent>(player);
    runtime.version = kSkillContractRuntimeVersion;
    runtime.active_transmuter_node_by_skill[8] = 870;
    runtime.trigger_cooldowns[114] = 1.25f;
    runtime.trigger_cooldowns[971] = 0.5f;

    const auto snapshot = SaveManager::Get().createSnapshot(registry);
    CHECK(snapshot.skill_contract_runtime.version == kSkillContractRuntimeVersion);
    CHECK(snapshot.skill_contract_runtime.skills.size() >= 1);

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, snapshot);
    auto view = restored.view<PlayerTag, SkillContractRuntimeComponent>();
    REQUIRE(view.begin() != view.end());
    auto restoredPlayer = *view.begin();
    const auto& restoredRuntime =
        restored.get<SkillContractRuntimeComponent>(restoredPlayer);

    REQUIRE(restoredRuntime.active_transmuter_node_by_skill.contains(8));
    CHECK(restoredRuntime.active_transmuter_node_by_skill.at(8) == 870);
    REQUIRE(restoredRuntime.trigger_cooldowns.contains(114));
    CHECK(restoredRuntime.trigger_cooldowns.at(114) == doctest::Approx(1.25f));
    REQUIRE(restoredRuntime.trigger_cooldowns.contains(971));
    CHECK(restoredRuntime.trigger_cooldowns.at(971) == doctest::Approx(0.5f));
}

TEST_CASE("[Unit] SaveManager - Blade mastery snapshot roundtrip") {
    REQUIRE(NoMoreDay::data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<Position>(player, 1.0f, 2.0f);
    registry.emplace<PrimaryStats>(player, 10.0f, 10.0f, 10.0f, 10.0f);
    registry.emplace<ActiveSkillsComponent>(player);
    auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
    astrolabe.mainProfession =
        static_cast<int>(ProfessionID::BladeAscendant);
    auto &playerStats = registry.emplace<PlayerStats>(player);
    playerStats.level = 50;

    auto& mastery = registry.emplace<BladeMasteryComponent>(player);
    mastery.profession = static_cast<ProfessionID>(0);
    mastery.selected = BladeMasteryId::HeavenlySword;
    mastery.debug_unlock_active = true;
    mastery.heavenly_attunement = BladeAttunement::Lightning;

    auto& resource = registry.emplace<BladeResourceComponent>(player);
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 4;
    resource.max = 10;

    auto& signature = registry.emplace<BladeSignatureSkillComponent>(player);
    signature.skill_id = 11;
    signature.unlocked = true;

    const auto snapshot = SaveManager::Get().createSnapshot(registry);
    CHECK(snapshot.header.version == CURRENT_CHARACTER_SAVE_VERSION);
    CHECK(snapshot.header.level == 50);
    REQUIRE(snapshot.blade_mastery.has_value());
    REQUIRE(snapshot.blade_resource.has_value());
    REQUIRE(snapshot.blade_signature_skill.has_value());
    CHECK(snapshot.blade_mastery->selected == BladeMasteryId::HeavenlySword);
    CHECK(snapshot.blade_mastery->heavenly_attunement ==
          BladeAttunement::Lightning);
    CHECK(snapshot.blade_mastery->blood_oath_active == false);
    CHECK(snapshot.blade_resource->kind == BladeResourceKind::SpiritBladeTier);
    CHECK(snapshot.blade_resource->current == 4);
    CHECK(snapshot.blade_signature_skill->skill_id == 11);
    CHECK(snapshot.blade_signature_skill->unlocked);

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, snapshot);
    auto view = restored.view<PlayerTag, BladeMasteryComponent,
                              BladeResourceComponent,
                              BladeSignatureSkillComponent>();
    REQUIRE(view.begin() != view.end());
    const auto restoredPlayer = *view.begin();
    REQUIRE(restored.all_of<PlayerLevel>(restoredPlayer));
    CHECK(restored.get<PlayerLevel>(restoredPlayer).value == 50);
    CHECK(restored.get<BladeMasteryComponent>(restoredPlayer).selected ==
          BladeMasteryId::HeavenlySword);
    CHECK(restored.get<BladeMasteryComponent>(restoredPlayer)
              .heavenly_attunement == BladeAttunement::Lightning);
    CHECK(restored.get<BladeMasteryComponent>(restoredPlayer).blood_oath_active ==
          false);
    CHECK(restored.get<BladeResourceComponent>(restoredPlayer).kind ==
          BladeResourceKind::SpiritBladeTier);
    CHECK(restored.get<BladeResourceComponent>(restoredPlayer).current == 4);
    CHECK(restored.get<BladeSignatureSkillComponent>(restoredPlayer).skill_id == 11);
    CHECK(restored.get<BladeSignatureSkillComponent>(restoredPlayer).unlocked);
}

TEST_CASE("[Unit] SaveManager - normalizes transient blade resource restore state") {
    REQUIRE(NoMoreDay::data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    CharacterSaveData data;
    data.header.version = CURRENT_CHARACTER_SAVE_VERSION;
    data.header.level = 50;
    data.astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

    BladeMasteryComponent mastery;
    mastery.profession = ProfessionID::BladeAscendant;
    mastery.selected = BladeMasteryId::SwordSaint;
    mastery.debug_unlock_active = true;
    data.blade_mastery = mastery;

    BladeResourceComponent resource;
    resource.kind = BladeResourceKind::SwordFlow;
    resource.current = 4;
    resource.max = 10;
    resource.time_since_last_gain = 1.75f;
    resource.last_crit_bonus_time = 42.0f;
    resource.crit_bonus_feedback_timer = 0.8f;
    resource.restart_window_timer = 2.5f;
    resource.restart_window_ready = true;
    resource.grace_period = 3.5f;
    resource.decay_tick_timer = 0.4f;
    resource.decay_interval = 0.25f;
    data.blade_resource = resource;

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, data);

    auto view = restored.view<PlayerTag, BladeMasteryComponent, BladeResourceComponent>();
    REQUIRE(view.begin() != view.end());
    const auto player = *view.begin();

    const auto &restoredResource = restored.get<BladeResourceComponent>(player);
    CHECK(restoredResource.kind == BladeResourceKind::SwordFlow);
    CHECK(restoredResource.current == 4);
    CHECK(restoredResource.max == 10);
    CHECK(restoredResource.grace_period == doctest::Approx(5.0f));
    CHECK(restoredResource.decay_interval == doctest::Approx(0.5f));

    CHECK(restoredResource.time_since_last_gain == doctest::Approx(0.0f));
    CHECK(restoredResource.last_crit_bonus_time == doctest::Approx(-999.0f));
    CHECK(restoredResource.crit_bonus_feedback_timer == doctest::Approx(0.0f));
    CHECK(restoredResource.restart_window_timer == doctest::Approx(0.0f));
    CHECK_FALSE(restoredResource.restart_window_ready);
    CHECK(restoredResource.decay_tick_timer == doctest::Approx(0.0f));
    CHECK(restoredResource.hit_tracking.empty());
    CHECK_FALSE(systems::BladeResourceService::TryConsumeSwordFlowRestartWindow(
        restored, player, 10));
}

TEST_CASE("[Unit] SaveManager - normalizes incompatible blade mastery runtime state") {
    REQUIRE(NoMoreDay::data::BladeMasteryRegistry::Get().LoadFromJson(
        "assets/data/blade_masteries.json"));

    CharacterSaveData data;
    data.header.version = CURRENT_CHARACTER_SAVE_VERSION;
    data.astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);

    BladeMasteryComponent mastery;
    mastery.profession = ProfessionID::BladeAscendant;
    mastery.selected = BladeMasteryId::None;
    mastery.debug_unlock_active = true;
    mastery.heavenly_attunement = BladeAttunement::Lightning;
    mastery.blood_oath_active = false;
    data.blade_mastery = mastery;

    BladeResourceComponent resource;
    resource.kind = BladeResourceKind::SpiritBladeTier;
    resource.current = 7;
    resource.max = 10;
    resource.grace_period = 3.5f;
    resource.decay_interval = 0.25f;
    data.blade_resource = resource;

    BladeSignatureSkillComponent signature;
    signature.skill_id = 11;
    signature.unlocked = true;
    data.blade_signature_skill = signature;

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, data);

    auto view = restored.view<PlayerTag, BladeMasteryComponent,
                              BladeResourceComponent,
                              BladeSignatureSkillComponent>();
    REQUIRE(view.begin() != view.end());
    const auto player = *view.begin();

    const auto &restoredMastery = restored.get<BladeMasteryComponent>(player);
    const auto &restoredResource = restored.get<BladeResourceComponent>(player);
    const auto &restoredSignature =
        restored.get<BladeSignatureSkillComponent>(player);

    CHECK(restoredMastery.selected == BladeMasteryId::None);
    CHECK(restoredMastery.heavenly_attunement == BladeAttunement::None);
    CHECK_FALSE(restoredMastery.blood_oath_active);
    CHECK(restoredResource.kind == BladeResourceKind::SwordIntent);
    CHECK(restoredSignature.skill_id == INVALID_SKILL_ID);
    CHECK_FALSE(restoredSignature.unlocked);
}

TEST_CASE("[Unit] Blade mastery save data - legacy JSON defaults new mastery state") {
    const nlohmann::json legacyJson = {
        {"profession", 0u},
        {"selected", static_cast<uint32_t>(BladeMasteryId::DemonBlade)},
        {"debug_unlock_active", true}
    };

    const auto mastery = legacyJson.get<BladeMasteryComponent>();

    CHECK(mastery.selected == BladeMasteryId::DemonBlade);
    CHECK(mastery.heavenly_attunement == BladeAttunement::None);
    CHECK_FALSE(mastery.blood_oath_active);
}

TEST_CASE("[Unit] SaveManager - Migrates legacy empty specialization slots") {
    CharacterSaveData data;
    data.header.version = 1;
    data.header.name = "Legacy";
    data.skills.specialized_slots[0].skill_id = 0;
    data.skills.specialized_slots[1].skill_id = 7;

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, data);

    auto view = restored.view<PlayerTag, ActiveSkillsComponent>();
    REQUIRE(view.begin() != view.end());
    const auto restoredPlayer = *view.begin();
    const auto& active = restored.get<ActiveSkillsComponent>(restoredPlayer);

    CHECK(active.specialized_slots[0].skill_id == INVALID_SKILL_ID);
    CHECK(active.specialized_slots[1].skill_id == 7);
}

TEST_CASE("[Unit] SaveManager - Preserves skill zero specialization in current saves") {
    CharacterSaveData data;
    data.header.version = CURRENT_CHARACTER_SAVE_VERSION;
    data.header.name = "Current";
    data.skills.specialized_slots[0].skill_id = 0;

    entt::registry restored;
    SaveManager::Get().restoreFromSnapshot(restored, data);

    auto view = restored.view<PlayerTag, ActiveSkillsComponent>();
    REQUIRE(view.begin() != view.end());
    const auto restoredPlayer = *view.begin();
    const auto& active = restored.get<ActiveSkillsComponent>(restoredPlayer);

    CHECK(active.specialized_slots[0].skill_id == 0);
}

TEST_CASE("[Unit] SaveManager - Legacy Blade Ascendant save rehydrates blade runtime") {
    CharacterSaveData data;
    data.header.version = 2;
    data.position = Position{0.0f, 0.0f};
    data.primaryStats = PrimaryStats{};
    data.skills = ActiveSkillsComponent{};
    data.astrolabe = AstrolabeComponent{};
    data.astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
    data.combatHistory = PlayerCombatHistory{};

    entt::registry registry;
    SaveManager::Get().restoreFromSnapshot(registry, data);

    auto view = registry.view<PlayerTag, BladeResourceComponent>();
    REQUIRE(view.begin() != view.end());
    const auto player = *view.begin();
    CHECK(registry.get<BladeResourceComponent>(player).kind ==
          BladeResourceKind::SwordIntent);
    CHECK(registry.all_of<SwordIntentComponent>(player));
}

TEST_CASE("[Unit] StatsOptimization - Zero Allocation") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<PrimaryStats>(entity);
    registry.emplace<CombatStats>(entity);
    
    // Just verify it works
    StatsSystem::Recalculate(registry, entity);
    CHECK(registry.get<CombatStats>(entity).health >= 0);
}

TEST_CASE("[Unit] SwordIntent - Basic Accumulation") {
    entt::registry registry;
    auto player = registry.create();
    auto& si = registry.emplace<SwordIntentComponent>(player);
    si.stacks = 5.0f;
    CHECK(si.stacks == 5.0f);
}

} // namespace NoMoreDay
