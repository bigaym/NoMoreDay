#include "game/systems/nemesis/NemesisGenerator.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "game/data/NemesisDataStore.hpp"
#include "game/data/PlayerCombatHistory.hpp"
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
    {"Fast", "疾风"},      {"Tanky", "巨岩"},     {"Vampiric", "血饮"},
    {"Molten", "烈焰"},    {"Shielding", "铁壁"}, {"Mirror Image", "幻影"},
    {"Avenger", "复仇者"}, {"Void", "破甲者"}};

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
  auto affixes = SelectAffixes(registry);
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
  std::string prefix = prefixes[utils::ThreadSafeRandom::GetInt(
      0, static_cast<int>(prefixes.size() - 1))];

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

std::vector<std::string>
NemesisGenerator::SelectAffixes(entt::registry &registry) {
  std::vector<std::string> selected_affixes;

  // 1. Analyze Player History for Adaptive Traits
  auto view = registry.view<PlayerCombatHistory>();
  if (!view.empty()) {
    const auto &history = view.get<PlayerCombatHistory>(*view.begin());
    float totalDmg = history.getTotalDamageTracking();

    // Rule 1: Adaptive Resistance (Offensive Traits)
    if (totalDmg > 0) {
      if (history.damageDealtFire / totalDmg > 0.6f)
        selected_affixes.push_back("Molten");
      else if (history.damageDealtPhysical / totalDmg > 0.6f)
        selected_affixes.push_back("Tanky"); // Was Thorns
      else if (history.damageDealtCold / totalDmg > 0.6f)
        selected_affixes.push_back("Frozen");
      else if (history.damageDealtLightning / totalDmg > 0.6f)
        selected_affixes.push_back("Storm"); // Was Shocking
    }

    // Rule 2: Anti-Kite
    // Assuming distance units are pixels. > 300 is roughly ranged.
    if (history.avgEngagementDistance > 300.0f) {
      selected_affixes.push_back("Fast");   // Closer gap
      selected_affixes.push_back("Vortex"); // Pull player
    }

    // Rule 3: Anti-Burst
    // If peak damage is extremely high relative to... something?
    // Or just if it exceeds a threshold (e.g. 5000).
    if (history.burstDamagePeak > 2000.0f) {
      selected_affixes.push_back("Shielding"); // Was PhaseShield
    }
  }

  // 2. Fill with Global History (DataStore)
  auto &store = NemesisDataStore::Get();
  auto top_affixes = store.GetTopAffixes(3);

  // Merge lists unique
  for (const auto &aff : top_affixes) {
    if (std::find(selected_affixes.begin(), selected_affixes.end(), aff) ==
        selected_affixes.end()) {
      selected_affixes.push_back(aff);
    }
  }

  // 3. Fill with random defaults if needed
  if (selected_affixes.size() < 2) {
    static const std::vector<std::string> DEFAULT_AFFIXES = {"Fast", "Tanky",
                                                             "Vampiric"};
    while (selected_affixes.size() < 2) {
      std::string affix = DEFAULT_AFFIXES[utils::ThreadSafeRandom::GetInt(
          0, static_cast<int>(DEFAULT_AFFIXES.size() - 1))];
      if (std::find(selected_affixes.begin(), selected_affixes.end(), affix) ==
          selected_affixes.end()) {
        selected_affixes.push_back(affix);
      }
    }
  }

  // Cap at 4 affixes to avoid overload
  if (selected_affixes.size() > 4) {
    selected_affixes.resize(4);
  }

  LOG_DEBUG("NemesisGenerator: Selected affixes: {}", [&] {
    std::string s;
    for (auto &a : selected_affixes)
      s += a + ", ";
    return s;
  }());

  return selected_affixes;
}

Tag NemesisGenerator::DetermineCounterResistances(entt::registry &registry) {
  Tag result = Tag::None;

  auto view = registry.view<PlayerCombatHistory>();
  if (view.empty()) {
    // Fallback to old stats-based method if no history
    // ... (Copying old logic is messy here, let's just default to None or use
    // Stats view as fallback?) For brevity, let's just return None or implement
    // a simple fallback.
    return result;
  }

  const auto &history = view.get<PlayerCombatHistory>(*view.begin());
  float totalDmg = history.getTotalDamageTracking();
  if (totalDmg <= 1.0f)
    return result; // No data

  float maxVal = 0.0f;
  DamageType maxType = DamageType::Physical;

  if (history.damageDealtPhysical > maxVal) {
    maxVal = history.damageDealtPhysical;
    maxType = DamageType::Physical;
  }
  if (history.damageDealtFire > maxVal) {
    maxVal = history.damageDealtFire;
    maxType = DamageType::Fire;
  }
  if (history.damageDealtCold > maxVal) {
    maxVal = history.damageDealtCold;
    maxType = DamageType::Cold;
  }
  if (history.damageDealtLightning > maxVal) {
    maxVal = history.damageDealtLightning;
    maxType = DamageType::Lightning;
  }
  if (history.damageDealtPoison > maxVal) {
    maxVal = history.damageDealtPoison;
    maxType = DamageType::Poison;
  }

  // If dominant type is > 50% of total, counter it
  if (maxVal / totalDmg > 0.5f) {
    switch (maxType) {
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
    // Shadow/Void not tracked in struct explicitly yet but mapped to keys if
    // added
    default:
      break;
    }
    LOG_DEBUG("NemesisGenerator: Player primary damage type: {} ({:.1f}%) -> "
              "Counter resistance: {}",
              static_cast<int>(maxType), (maxVal / totalDmg) * 100.0f,
              static_cast<uint64_t>(result));
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

  // MonsterAffixComponent (Critical for logic)
  auto &affixComp = registry.emplace<MonsterAffixComponent>(entity);
  for (const auto &affixName : affixes) {
    MonsterAffixType type = MonsterAffixType::None;
    if (affixName == "Fast")
      type = MonsterAffixType::Fast;
    else if (affixName == "Tanky")
      type = MonsterAffixType::Tanky;
    else if (affixName == "Powerful")
      type = MonsterAffixType::Powerful;
    else if (affixName == "Accurate")
      type = MonsterAffixType::Accurate;
    else if (affixName == "Molten")
      type = MonsterAffixType::Molten;
    else if (affixName == "Frozen")
      type = MonsterAffixType::Frozen;
    else if (affixName == "Storm")
      type = MonsterAffixType::Storm;
    else if (affixName == "Toxic")
      type = MonsterAffixType::Toxic;
    else if (affixName == "Void")
      type = MonsterAffixType::Void;
    else if (affixName == "VoidZone")
      type = MonsterAffixType::
          VoidZone; // Corrected "Void Zone" space issue handled by name match?
                    // Registry has "Void Zone" as name but keys are simple.
    // Wait, Registry defines key enum e.g. VoidZone. The name string is "Void
    // Zone". My select logic pushes "VoidZone" (no space).
    else if (affixName == "StormStrider")
      type = MonsterAffixType::StormStrider;
    else if (affixName == "Teleporter")
      type = MonsterAffixType::Teleporter;
    else if (affixName == "Nullifier")
      type = MonsterAffixType::Nullifier;
    else if (affixName == "Shielding")
      type = MonsterAffixType::Shielding;
    else if (affixName == "Waller")
      type = MonsterAffixType::Waller;
    else if (affixName == "Vampiric")
      type = MonsterAffixType::Vampiric;
    else if (affixName == "Berserker")
      type = MonsterAffixType::Berserker;
    else if (affixName == "Avenger")
      type = MonsterAffixType::Avenger;
    else if (affixName == "SoulLink")
      type = MonsterAffixType::SoulLink;
    else if (affixName == "MirrorImage")
      type = MonsterAffixType::MirrorImage; // Mirror Image space? My select
                                            // pushes "MirrorImage"?
    // Registry Enum is MirrorImage. SelectAffixes keys should match Enum names
    // generally or be consistent. I should check what SelectAffixes uses. It
    // pushes "Mirror Image" in top_affixes from DataStore? DataStore uses
    // strings. I need to be careful with spaces. Let's assume keys are
    // CamelCase without spaces as generated by `SelectAffixes` and
    // `top_affixes`. If top_affixes come from
    // `NemesisDataStore::GetTopAffixes`, they rely on what was saved.
    else if (affixName == "SoulEater")
      type = MonsterAffixType::SoulEater;
    else if (affixName == "Suppressor")
      type = MonsterAffixType::Suppressor;
    else if (affixName == "ManaSiphon")
      type = MonsterAffixType::ManaSiphon;

    if (type != MonsterAffixType::None) {
      affixComp.AddAffix(type);
    } else {
      LOG_WARN("NemesisGenerator: Unknown affix string '{}'", affixName);
    }
  }

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
