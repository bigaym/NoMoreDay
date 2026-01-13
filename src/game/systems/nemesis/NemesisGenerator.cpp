#include "game/systems/nemesis/NemesisGenerator.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/data/NemesisDataStore.hpp"
#include "game/systems/nemesis/FactionAggroSystem.hpp"
#include <algorithm>
#include <random>

namespace NoMoreDay {

// Nemesis name prefixes based on faction
static const std::vector<std::string> FACTION_PREFIXES[] = {
    {"幽骨", "亡魂", "冥府"}, // Undead
    {"虚空", "裂隙", "异界"}, // Void
    {"腐化", "堕落", "瘟疫"}, // Corrupted
    {"邪典", "暗影", "诅咒"}  // Cultist
};

// Nemesis name suffixes based on top affixes
static const std::unordered_map<std::string, std::string> AFFIX_SUFFIXES = {
    {"Fast", "疾风"},      {"Tanky", "巨岩"},      {"Vampiric", "血饮"},
    {"Molten", "烈焰"},    {"Stone Skin", "铁壁"}, {"Mirror Image", "幻影"},
    {"Avenger", "复仇者"}, {"Sunder", "破甲者"}};

bool NemesisGenerator::SpawnNemesisIfReady(entt::registry &registry,
                                           const Position &spawnPos) {
  FactionType faction;
  if (!FactionAggroSystem::ShouldSpawnNemesis(registry, faction)) {
    return false;
  }

  // Check if there's already an active Nemesis
  auto view = registry.view<NemesisTag>();
  if (!view.empty()) {
    LOG_DEBUG(
        "NemesisGenerator: Active Nemesis already exists, skipping spawn");
    return false;
  }

  SpawnNemesis(registry, faction, spawnPos);
  FactionAggroSystem::ConsumeNemesisTrigger(registry, faction);

  return true;
}

entt::entity NemesisGenerator::SpawnNemesis(entt::registry &registry,
                                            FactionType faction,
                                            const Position &spawnPos) {
  auto affixes = SelectAffixes();
  auto resistances = DetermineCounterResistances(registry);

  // Get evolution tier from data store
  int evolution_tier = 1;
  auto &store = NemesisDataStore::Get();
  if (store.active_nemesis.has_value() &&
      store.active_nemesis->faction == faction) {
    evolution_tier = store.active_nemesis->evolution_tier;
  }

  auto entity = CreateNemesisEntity(registry, faction, affixes, resistances,
                                    spawnPos, evolution_tier);

  // Update data store
  NemesisData data;
  data.nemesis_id = store.GenerateNemesisId();
  data.faction = faction;
  data.affixes = affixes;
  data.resistances = resistances;
  data.evolution_tier = evolution_tier;
  data.display_name = GenerateDisplayName(faction, affixes);
  data.is_active = true;
  store.active_nemesis = data;

  LOG_INFO("NemesisGenerator: Spawned '{}' (Tier {}) at ({:.0f}, {:.0f})",
           data.display_name, evolution_tier, spawnPos.x, spawnPos.y);

  return entity;
}

std::string
NemesisGenerator::GenerateDisplayName(FactionType faction,
                                      const std::vector<std::string> &affixes) {
  // Pick a random faction prefix
  size_t faction_idx = static_cast<size_t>(faction);
  if (faction_idx >= 4) { // Safety check for FACTION_PREFIXES size
      faction_idx = 0;
  }
  const auto &prefixes = FACTION_PREFIXES[faction_idx];
  std::string prefix = prefixes[utils::ThreadSafeRandom::GetInt(0, static_cast<int>(prefixes.size() - 1))];

  // Build suffix from top affix
  std::string suffix = "·复仇者"; // Default
  if (!affixes.empty()) {
    auto it = AFFIX_SUFFIXES.find(affixes[0]);
    if (it != AFFIX_SUFFIXES.end()) {
      suffix = "·" + it->second;
    }
  }

  return prefix + suffix;
}

std::vector<std::string> NemesisGenerator::SelectAffixes() {
  auto &store = NemesisDataStore::Get();
  auto top_affixes = store.GetTopAffixes(3);

  // If not enough history, add some random default affixes
  if (top_affixes.size() < 2) {
    static const std::vector<std::string> DEFAULT_AFFIXES = {"Fast", "Tanky",
                                                             "Vampiric"};

    while (top_affixes.size() < 2) {
      std::string affix = DEFAULT_AFFIXES[utils::ThreadSafeRandom::GetInt(0, static_cast<int>(DEFAULT_AFFIXES.size() - 1))];
      if (std::find(top_affixes.begin(), top_affixes.end(), affix) ==
          top_affixes.end()) {
        top_affixes.push_back(affix);
      }
    }
  }

  LOG_DEBUG("NemesisGenerator: Selected affixes: {}", [&] {
    std::string s;
    for (auto &a : top_affixes)
      s += a + ", ";
    return s;
  }());

  return top_affixes;
}

Tag NemesisGenerator::DetermineCounterResistances(entt::registry &registry) {
  // Find player and analyze their damage profile
  Tag result = Tag::None;

  auto view = registry.view<PlayerTag, CombatStats>();
  for (auto [entity, stats] : view.each()) {
    // Find the damage type with highest multiplier
    float max_mult = 0.0f;
    int max_idx = 0;

    for (size_t i = 0; i < stats.damage_multipliers.size(); ++i) {
      float total =
          stats.damage_multipliers[i] * (1.0f + stats.damage_percent_add[i]);
      if (total > max_mult) {
        max_mult = total;
        max_idx = static_cast<int>(i);
      }
    }

    // Map DamageType index to Tag
    switch (static_cast<DamageType>(max_idx)) {
    case DamageType::Physical:
      result = Tag::Physical;
      break;
    case DamageType::Fire:
      result = Tag::Fire;
      break;
    case DamageType::Cold:
      result = Tag::Cold;
      break;
    case DamageType::Lightning:
      result = Tag::Lightning;
      break;
    case DamageType::Poison:
      result = Tag::Poison;
      break;
    case DamageType::Shadow:
      result = Tag::Shadow; // Shadow -> Sword for this game
      break;
    default:
      break;
    }

    LOG_DEBUG("NemesisGenerator: Player primary damage type: {} -> Counter "
              "resistance: {}",
              GetDamageTypeName(static_cast<DamageType>(max_idx)),
              static_cast<uint64_t>(result));
    break;
  }

  return result;
}

entt::entity NemesisGenerator::CreateNemesisEntity(
    entt::registry &registry, FactionType faction,
    const std::vector<std::string> &affixes, Tag resistances,
    const Position &pos, int evolution_tier) {

  auto entity = registry.create();

  // Position
  registry.emplace<Position>(entity, pos.x, pos.y);
  registry.emplace<Velocity>(entity);

  // Nemesis components
  auto &nemesis_comp =
      registry.emplace<NemesisComponent>(entity, faction, evolution_tier);
  nemesis_comp.evolved_affixes = affixes;
  nemesis_comp.counter_resistances = resistances;
  nemesis_comp.display_name = GenerateDisplayName(faction, affixes);
  nemesis_comp.nemesis_id = NemesisDataStore::Get().GenerateNemesisId();
  registry.emplace<NemesisTag>(entity);

  // Faction
  registry.emplace<FactionComponent>(entity, faction);

  // Enemy state with Nemesis-specific settings
  auto &enemy_state = registry.emplace<EnemyStateComponent>(entity);
  enemy_state.raceType = EnemyRace::DEMON; // Nemesis uses demon type
  enemy_state.archetypeType = EnemyArchetype::ASSASSIN;
  enemy_state.detectionRange = 500.0f; // Long detection range
  enemy_state.attackRange = 60.0f;
  enemy_state.speed = 120.0f;                        // Fast
  enemy_state.level = 20 + (evolution_tier - 1) * 5; // Scale with tier

  // Rarity = NEMESIS (we'll add this enum value)
  auto &rarity = registry.emplace<EnemyRarityComponent>(entity);
  rarity.rarity = EnemyRarityComponent::BOSS; // Use BOSS for now
  rarity.affixes = affixes;

  // AI - Hunter Mode
  auto &ai = registry.emplace<AIComponent>(entity);
  ai.aiType = AIType::NEMESIS_HUNTER; // Use specialized hunter mode
  ai.detectionRange = 1000.0f;        // Nemesis sees farther
  ai.attackRange = 60.0f;
  ai.speed = 120.0f;
  ai.isAggressive = true;

  // Combat stats - scaled for Nemesis
  float stat_mult = nemesis_comp.GetStatMultiplier();
  auto &combat = registry.emplace<CombatStats>(entity);
  combat.max_health = 5000.0f * stat_mult;
  combat.health = combat.max_health;
  combat.armor = 200.0f * stat_mult;
  combat.damage_multipliers[0] = 2.5f * stat_mult; // High damage
  combat.attack_speed = 1.5f;
  combat.move_speed = 120.0f;

  // Apply counter resistances
  if (HasTag(resistances, Tag::Fire)) {
    combat.resistances[static_cast<int>(DamageType::Fire)] = 0.75f;
  }
  if (HasTag(resistances, Tag::Cold)) {
    combat.resistances[static_cast<int>(DamageType::Cold)] = 0.75f;
  }
  if (HasTag(resistances, Tag::Lightning)) {
    combat.resistances[static_cast<int>(DamageType::Lightning)] = 0.75f;
  }
  if (HasTag(resistances, Tag::Shadow)) {
    combat.resistances[static_cast<int>(DamageType::Shadow)] = 0.75f;
  }
  if (HasTag(resistances, Tag::Physical)) {
    combat.armor = 500.0f * stat_mult; // Extra armor vs physical
  }

  // Health component
  auto &health = registry.emplace<HealthComponent>(entity);
  health.max = combat.max_health;
  health.current = combat.max_health;

  // Enemy tag for general enemy systems
  registry.emplace<EnemyTag>(entity);

  // Sprite (placeholder - should be loaded from texture)
  auto &sprite = registry.emplace<ColorComponent>(entity);
  sprite.color = {220, 20, 60, 255}; // Crimson color
  // sprite.width = 48.0f;
  // sprite.height = 48.0f;

  return entity;
}

} // namespace NoMoreDay
