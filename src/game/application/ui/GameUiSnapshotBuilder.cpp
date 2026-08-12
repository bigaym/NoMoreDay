#include "game/application/ui/GameUiSnapshotBuilder.hpp"

#include <entt/entt.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "game/foundation/components/AIComponent.hpp" // EnemyTag
#include "game/foundation/components/Buff.hpp"         // ActiveEffectsComponent
#include "game/foundation/components/Common.hpp"       // PlayerTag, HealthComponent, Position
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp" // Affix
#include "game/foundation/components/MapComponent.hpp" // PortalComponent, PortalType
#include "game/foundation/components/MaterialBankComponent.hpp"
#include "game/foundation/components/PlayerState.hpp" // PlayerStats
#include "game/foundation/components/Progression.hpp" // AstrolabeComponent
#include "game/foundation/components/SkillDefs.hpp"   // ActiveSkillsComponent, BladeResourceComponent, SummonComponent
#include "game/foundation/components/StashComponent.hpp"
#include "game/foundation/components/Stats.hpp" // CombatStats, PrimaryStats
#include "game/foundation/components/WorldState.hpp" // ActiveDimensionalState (registry ctx)
#include "game/foundation/data/BuffRegistry.hpp"     // read-side buff visual table
#include "game/foundation/data/BladeMasteryData.hpp" // BladeMasteryId / BladeAttunement
#include "game/foundation/data/BladeMasteryRegistry.hpp" // mastery card source
#include "game/foundation/data/MonsterAffixRegistry.hpp" // MonsterAffixComponent
#include "game/foundation/data/SkillRegistry.hpp"    // read-side skill table
#include "game/systems/item/SharedStash.hpp" // Shared stash tab source (R7)
#include "game/systems/item/StashSystem.hpp" // nextUnlockCost authority (R7)
#include "game/systems/skill/BladeMasteryService.hpp" // mastery unlock state (R8)
#include "game/systems/skill/SkillSystem.hpp" // mutual-keystone exclusions (R8)
#include "game/systems/world/EnemyConstants.hpp" // NEXT_LEVEL_PORTAL_KILL_REQUIREMENT

namespace NoMoreDay::ui {
namespace {

// Same world-space pickup threshold used by the mouse click pickup path in
// UISystem::Draw (UISystem.cpp:681, distSq <= 180.0f * 180.0f).
inline constexpr float kPickupRange = 180.0f;

// Occupied inventory slots: non-null entries in the items vector.
int CountUsedSlots(const NoMoreDay::InventoryComponent& inventory) {
  int used = 0;
  for (const entt::entity slot : inventory.items) {
    if (slot != entt::null) {
      ++used;
    }
  }
  return used;
}

std::uint64_t ToDomainId(entt::entity entity) {
  return entity == entt::null ? kInvalidDomainId : entt::to_integral(entity);
}

entt::entity ToEntity(std::uint64_t domainId) {
  return domainId == kInvalidDomainId ? entt::null
                                      : entt::entity(domainId);
}

// Zero-allocation case-insensitive substring match used for the stash search
// filter (R7: the builder computes the per-slot matchesSearch flag from the
// UI query so the panel never reads registry/name data in the paint path).
bool ContainsIgnoreCase(const char* text, const char* query) {
  if (query == nullptr || query[0] == '\0') {
    return true;
  }
  const std::size_t queryLen = std::strlen(query);
  if (queryLen == 0) {
    return true;
  }
  const std::size_t textLen = std::strlen(text);
  if (textLen < queryLen) {
    return false;
  }
  for (std::size_t i = 0; i + queryLen <= textLen; ++i) {
    bool match = true;
    for (std::size_t j = 0; j < queryLen; ++j) {
      if (std::tolower(static_cast<unsigned char>(text[i + j])) !=
          std::tolower(static_cast<unsigned char>(query[j]))) {
        match = false;
        break;
      }
    }
    if (match) {
      return true;
    }
  }
  return false;
}

// Value snapshot of one affix (gameplay Affix carries vector payloads that
// the panels do not need).
GameUiAffixView ToAffixView(const NoMoreDay::Affix& affix) {
  GameUiAffixView view;
  view.type = static_cast<std::uint16_t>(affix.type);
  view.value = affix.value;
  view.tier = affix.tier;
  view.isPrefix = affix.isPrefix;
  view.isLegendary = affix.isLegendary;
  return view;
}

// Read-only item display data. inventoryIndex/bagSlotIndex describe the
// placement inside the player inventory (-1 when not in it).
GameUiItemView ToItemView(const entt::registry& registry,
                          entt::entity entity, int inventoryIndex,
                          int bagSlotIndex) {
  GameUiItemView view;
  view.domainId = ToDomainId(entity);
  view.inventoryIndex = inventoryIndex;
  view.bagSlotIndex = bagSlotIndex;
  const auto* item = registry.try_get<const NoMoreDay::ItemComponent>(entity);
  if (item == nullptr) {
    return view;
  }
  view.itemId = item->id;
  view.textureId = static_cast<std::uint32_t>(item->textureId);
  view.quantity =
      static_cast<std::uint32_t>(std::max(0, item->quantity));
  view.maxStack =
      static_cast<std::uint32_t>(std::max(1, item->maxStack));
  view.rarity = static_cast<std::uint8_t>(item->rarity);
  view.itemType = static_cast<std::uint8_t>(item->type);
  view.slot = static_cast<std::uint8_t>(item->slot);
  view.socketCount = static_cast<std::uint8_t>(
      std::max(0, std::min(255, item->socketCount)));
  // R6: socket contents (first free socket index is needed by the inventory
  // controller to build the SocketRune intent without touching the ECS).
  // entt::null round-trips to 0 = free socket.
  for (std::size_t i = 0; i < view.sockets.size() && i < item->sockets.size();
       ++i) {
    view.sockets[i] =
        static_cast<std::uint32_t>(entt::to_integral(item->sockets[i]));
  }
  view.itemLevel = item->itemLevel;
  view.forgingPotential = item->forgingPotential;
  view.legendaryPotential = item->legendaryPotential;
  view.isLocked = item->isLocked;
  view.isTwoHanded = item->isTwoHanded;
  view.affixes.reserve(item->affixes.size());
  for (const auto& affix : item->affixes) {
    view.affixes.push_back(ToAffixView(affix));
  }
  view.implicits.reserve(item->implicits.size());
  for (const auto& affix : item->implicits) {
    view.implicits.push_back(ToAffixView(affix));
  }
  return view;
}

// R8: fills the tooltip payload of a displayed-items view (name, description,
// stat rows and filled-socket rune names). Only invoked for the bounded
// displayed-items cache (hover/drag/context/crafting targets), never for the
// per-frame inventory/stash item lists (design §3.2: names are not
// duplicated per item).
void FillTooltipData(const entt::registry& registry, GameUiItemView& view) {
  const auto* item = registry.try_get<const NoMoreDay::ItemComponent>(
      ToEntity(view.domainId));
  if (item == nullptr) {
    return;
  }
  view.name = item->name;
  view.description = item->description;
  view.attack = item->attack;
  view.defense = item->defense;
  view.bagCapacity = item->bagCapacity;
  for (std::size_t i = 0;
       i < view.sockets.size() && i < item->sockets.size(); ++i) {
    auto& name = view.socketRuneNames[i];
    name.fill('\0');
    const entt::entity rune = item->sockets[i];
    if (rune == entt::null || !registry.valid(rune)) {
      continue;
    }
    const auto* runeComp = registry.try_get<const NoMoreDay::ItemComponent>(rune);
    if (runeComp == nullptr) {
      continue;
    }
    const std::size_t n = std::min(runeComp->name.size(), name.size() - 1);
    std::memcpy(name.data(), runeComp->name.data(), n);
    name[n] = '\0';
  }
}

} // namespace

template <typename Registry>
GameUiSnapshot GameUiSnapshotBuilder::Build(
    const Registry& registry, const GameUiSnapshotOptions& options) {
  GameUiSnapshot snapshot;
  snapshot.revision = ++m_revision;

  float playerX = 0.0f;
  float playerY = 0.0f;
  bool hasPlayerPosition = false;

  // --- Player + character stats ------------------------------------------
  const auto playerView = registry.template view<const PlayerTag>();
  const bool hasPlayer = playerView.begin() != playerView.end();
  if (hasPlayer) {
    const entt::entity player = playerView.front();
    GameUiPlayerSnapshot& playerSnap = snapshot.player;
    playerSnap.hasPlayer = true;
    playerSnap.domainId = ToDomainId(player);

    if (const auto* health =
            registry.template try_get<HealthComponent>(player)) {
      playerSnap.health = health->current;
      playerSnap.maxHealth = health->max;
    }
    if (const auto* stats = registry.template try_get<PlayerStats>(player)) {
      playerSnap.level = stats->level;
      playerSnap.currentXp = stats->current_xp;
      playerSnap.requiredXp = stats->required_xp;
      playerSnap.availableAttributePoints = stats->available_attribute_points;
      playerSnap.availableSkillPoints = stats->available_skill_points;
    }
    if (const auto* inventory =
            registry.template try_get<const NoMoreDay::InventoryComponent>(
                player)) {
      playerSnap.inventoryUsed = CountUsedSlots(*inventory);
      playerSnap.inventoryCapacity = inventory->capacity;
      playerSnap.gold = inventory->gold;
    }
    if (const auto* position = registry.template try_get<Position>(player)) {
      playerX = position->x;
      playerY = position->y;
      hasPlayerPosition = true;
      playerSnap.hasWorldPosition = true;
      playerSnap.worldX = position->x;
      playerSnap.worldY = position->y;
    }
    // R6: player avatar texture id for the character panel paint path.
    if (const auto* sprite =
            registry.template try_get<const SpriteComponent>(player)) {
      playerSnap.avatarTextureId =
          static_cast<std::uint32_t>(sprite->texture.id);
    }

    // HUD widgets: blade resource + summon status (raw values only).
    if (const auto* blade =
            registry.template try_get<const BladeResourceComponent>(player)) {
      playerSnap.hasBladeResource = true;
      playerSnap.bladeResourceKind = static_cast<std::uint8_t>(blade->kind);
      playerSnap.bladeResourceCurrent = blade->current;
      playerSnap.bladeResourceMax = blade->max;
      // R5: runtime feedback cues (restart window / crit feedback).
      playerSnap.restartWindowReady = blade->restart_window_ready;
      playerSnap.restartWindowTimer = blade->restart_window_timer;
      playerSnap.critBonusFeedbackTimer = blade->crit_bonus_feedback_timer;
    }
    // R5: blade-mastery state (label/detail text resolution stays on the HUD
    // side; the builder copies the numeric enum values only).
    if (const auto* mastery =
            registry.template try_get<const BladeMasteryComponent>(player)) {
      playerSnap.hasMastery = true;
      playerSnap.masterySelected = static_cast<std::uint8_t>(mastery->selected);
      playerSnap.heavenlyAttunement =
          static_cast<std::uint8_t>(mastery->heavenly_attunement);
      playerSnap.bloodOathActive = mastery->blood_oath_active;
    }
    // R5: sword-intent fallback widget (used when no blade resource exists).
    if (const auto* intent =
            registry.template try_get<const SwordIntentComponent>(player)) {
      playerSnap.hasSwordIntent = true;
      playerSnap.swordIntentStacks = intent->stacks;
      playerSnap.swordIntentMaxStacks = intent->max_stacks;
    }
    // R5: active field windows (read-only query, same semantics as the HUD's
    // FindActiveHeavenlyFieldDuration / FindActiveBloodSeaField helpers).
    {
      const auto fieldView =
          registry.template view<const HeavenlySwordFieldComponent>();
      for (const entt::entity entity : fieldView) {
        const auto& field =
            fieldView.template get<const HeavenlySwordFieldComponent>(entity);
        if (field.owner == player &&
            field.duration > playerSnap.heavenlyFieldDuration) {
          playerSnap.heavenlyFieldDuration = field.duration;
        }
      }
      const auto bloodView =
          registry.template view<const BloodSeaFieldComponent>();
      for (const entt::entity entity : bloodView) {
        const auto& field =
            bloodView.template get<const BloodSeaFieldComponent>(entity);
        if (field.owner == player) {
          playerSnap.bloodSeaHasVoidKeystone = field.has_void_keystone;
          playerSnap.bloodSeaMiasmaBonus = field.miasma_duration_bonus;
        }
      }
    }
    // R5: summon groups (replaces the per-frame std::map aggregation). Grouped
    // by skill_id / archetype_id; keeps count + max life ratio + icon id.
    {
      const auto summonView = registry.template view<const SummonComponent>();
      for (const entt::entity summon : summonView) {
        const auto& s = summonView.template get<const SummonComponent>(summon);
        if (s.owner != player) {
          continue;
        }
        const std::uint32_t key =
            (s.skill_id != 0) ? s.skill_id : s.archetype_id;
        GameUiSummonGroupView* group = nullptr;
        for (auto& candidate : playerSnap.summonGroups) {
          if (candidate.key == key) {
            group = &candidate;
            break;
          }
        }
        if (group == nullptr) {
          GameUiSummonGroupView view;
          view.key = key;
          view.skillId = s.skill_id;
          view.archetypeId = s.archetype_id;
          view.iconId = s.icon_id;
          view.count = 0;
          view.maxLifeRatio = 0.0f;
          playerSnap.summonGroups.push_back(view);
          group = &playerSnap.summonGroups.back();
        }
        ++group->count;
        const float ratio = s.max_lifetime > 0.0f
                                ? std::clamp(s.lifetime / s.max_lifetime,
                                             0.0f, 1.0f)
                                : 0.0f;
        group->maxLifeRatio = std::max(group->maxLifeRatio, ratio);
      }
    }
    playerSnap.hasSummon = !playerSnap.summonGroups.empty();
    for (const auto& group : playerSnap.summonGroups) {
      playerSnap.summonCount += group.count;
    }

    // Combat stats (mana/barrier on the HUD, full numbers on the panel).
    if (const auto* stats =
            registry.template try_get<const CombatStats>(player)) {
      playerSnap.mana = stats->mana;
      playerSnap.maxMana = stats->max_mana;
      playerSnap.barrier = stats->barrier;
      playerSnap.maxBarrier = stats->max_barrier;

      GameUiCharacterStatsView& cs = snapshot.characterStats;
      cs.health = stats->health;
      cs.maxHealth = stats->max_health;
      cs.mana = stats->mana;
      cs.maxMana = stats->max_mana;
      cs.barrier = stats->barrier;
      cs.maxBarrier = stats->max_barrier;
      cs.effectiveStrength = stats->effective_strength;
      cs.effectiveDexterity = stats->effective_dexterity;
      cs.effectiveIntelligence = stats->effective_intelligence;
      cs.effectiveVitality = stats->effective_vitality;
      cs.minWeaponDamage = stats->min_weapon_damage;
      cs.maxWeaponDamage = stats->max_weapon_damage;
      cs.critChance = stats->crit_chance;
      cs.critDamage = stats->crit_damage;
      cs.attackSpeed = stats->attack_speed;
      cs.castSpeed = stats->cast_speed;
      cs.accuracy = stats->accuracy;
      cs.armor = stats->armor;
      cs.armorDr = stats->effective_armor_dr;
      cs.dodgeRating = stats->dodge_rating;
      cs.dodgeChance = stats->effective_dodge;
      cs.blockChance = stats->block_chance;
      cs.blockRating = stats->block_rating;
      cs.blockEffect = stats->effective_block_eff;
      cs.damageReduction = stats->damage_reduction;
      cs.thorns = stats->thorns;
      cs.healthRegen = stats->health_regen;
      cs.manaRegen = stats->mana_regen;
      cs.lifeSteal = stats->life_steal;
      cs.lifeOnHit = stats->life_on_hit;
      cs.manaOnHit = stats->mana_on_hit;
      cs.moveSpeed = stats->move_speed;
      cs.magicFind = stats->magic_find;
      cs.cooldownReduction = stats->cooldown_reduction;
      cs.pickupRange = stats->pickup_range;
      cs.goldBonus = stats->gold_bonus;
      cs.experienceGainMult = stats->experience_gain_mult;
      cs.resistances = stats->resistances;
      // R6: damage composition + raw/effective pairs (character panel paint).
      cs.armorPen = stats->armor_pen;
      cs.flatDamage = stats->flat_damage;
      cs.damageMultipliers = stats->damage_multipliers;
      cs.rawAttackSpeed = stats->raw_attack_speed;
      cs.rawBlockChance = stats->raw_block_chance;
      cs.rawMoveSpeed = stats->raw_move_speed;
      cs.rawCooldownReduction = stats->raw_cooldown_reduction;
      cs.rawResistances = stats->raw_resistances;
      cs.durationScale = stats->duration_scale;
      cs.areaScale = stats->area_scale;
    }
    if (const auto* primary =
            registry.template try_get<const PrimaryStats>(player)) {
      GameUiCharacterStatsView& cs = snapshot.characterStats;
      cs.strength = primary->strength;
      cs.dexterity = primary->dexterity;
      cs.intelligence = primary->intelligence;
      cs.vitality = primary->vitality;
    }
  }

  // --- Inventory / equipment / stash / crafting / skill / astrolabe views --
  std::unordered_map<std::uint64_t, GameUiItemView> inventoryById;
  std::unordered_map<std::uint64_t, GameUiItemView> equipmentById;
  if (hasPlayer) {
    const entt::entity player = playerView.front();
    if (const auto* inventory =
            registry.template try_get<const NoMoreDay::InventoryComponent>(
                player)) {
      GameUiInventoryView& inv = snapshot.inventory;
      inv.capacity = inventory->capacity;
      inv.used = CountUsedSlots(*inventory);
      inv.gold = inventory->gold;
      for (std::size_t i = 0; i < inventory->items.size(); ++i) {
        if (inventory->items[i] == entt::null) {
          continue;
        }
        GameUiItemView view =
            ToItemView(registry, inventory->items[i], static_cast<int>(i), -1);
        inv.items.push_back(view);
        inventoryById.emplace(view.domainId, std::move(view));
      }
      for (std::size_t i = 0; i < inventory->bag_slots.size(); ++i) {
        GameUiBagSlotView slotView;
        slotView.domainId = ToDomainId(inventory->bag_slots[i]);
        if (inventory->bag_slots[i] != entt::null) {
          if (const auto* item = registry.template try_get<
                  const NoMoreDay::ItemComponent>(inventory->bag_slots[i])) {
            slotView.itemId = item->id;
            slotView.textureId = static_cast<std::uint32_t>(item->textureId);
            slotView.rarity = static_cast<std::uint8_t>(item->rarity);
            slotView.quantity = static_cast<std::uint32_t>(
                std::max(0, item->quantity));
          }
        }
        inv.bagSlots[i] = slotView;
      }
    }

    if (const auto* equipment =
            registry.template try_get<const NoMoreDay::EquipmentComponent>(
                player)) {
      snapshot.equipment.reserve(equipment->slots.size());
      for (std::size_t i = 0; i < equipment->slots.size(); ++i) {
        if (equipment->slots[i] == entt::null) {
          continue;
        }
        GameUiEquippedSlotView slotView;
        slotView.slotIndex = static_cast<std::uint8_t>(i);
        slotView.domainId = ToDomainId(equipment->slots[i]);
        // R6: display data for the snapshot-driven paint path.
        if (const auto* item = registry.template try_get<
                const NoMoreDay::ItemComponent>(equipment->slots[i])) {
          slotView.itemId = item->id;
          slotView.textureId = static_cast<std::uint32_t>(item->textureId);
          slotView.rarity = static_cast<std::uint8_t>(item->rarity);
          slotView.quantity =
              static_cast<std::uint32_t>(std::max(0, item->quantity));
          slotView.itemType = static_cast<std::uint8_t>(item->type);
          slotView.isLocked = item->isLocked;
          slotView.socketCount = static_cast<std::uint8_t>(
              std::max(0, std::min(255, item->socketCount)));
          // R6: socket contents for the SocketRune intent (mirrors
          // GameUiItemView::sockets; 0 = free socket).
          for (std::size_t s = 0;
               s < slotView.sockets.size() && s < item->sockets.size(); ++s) {
            slotView.sockets[s] = static_cast<std::uint32_t>(
                entt::to_integral(item->sockets[s]));
          }
        }
        snapshot.equipment.push_back(slotView);

        GameUiItemView view = ToItemView(registry, equipment->slots[i], -1, -1);
        equipmentById.emplace(view.domainId, std::move(view));
      }
    }

    // --- Stash view (R7: Personal/Shared type-aware; nextUnlockCost unified
    // through StashSystem; per-slot matchesSearch from the UI search query).
    {
      const NoMoreDay::StashType stashType =
          static_cast<NoMoreDay::StashType>(options.stashType);
      GameUiStashView& stashView = snapshot.stash;
      const auto fillTab = [&](const NoMoreDay::StashTab& tab,
                               std::size_t tabIndex) {
        (void)tabIndex;
        GameUiStashTabView tabView;
        tabView.tabType = static_cast<std::uint8_t>(tab.type);
        tabView.iconId = tab.iconId;
        tabView.color = tab.color;
        // R7: bounded name cache (design §3.2: no std::string in the snapshot).
        const std::size_t nameLen = std::min<std::size_t>(
            tab.name.size(), tabView.name.size() - 1);
        std::memcpy(tabView.name.data(), tab.name.data(), nameLen);
        tabView.name[static_cast<std::size_t>(nameLen)] = '\0';
        for (std::size_t i = 0; i < tab.items.size(); ++i) {
          if (tab.items[i] == entt::null) {
            continue;
          }
          GameUiStashSlotView slotView;
          slotView.slotIndex = static_cast<int>(i);
          slotView.domainId = ToDomainId(tab.items[i]);
          if (const auto* item =
                  registry.template try_get<const NoMoreDay::ItemComponent>(
                      tab.items[i])) {
            slotView.textureId = static_cast<std::uint32_t>(item->textureId);
            slotView.rarity = static_cast<std::uint8_t>(item->rarity);
            slotView.quantity =
                static_cast<std::uint32_t>(std::max(0, item->quantity));
            // R7: search dimming computed here (builder-owned; the controller
            // keeps only the query buffer, never the registry/name data).
            slotView.matchesSearch =
                options.stashSearchQuery == nullptr ||
                options.stashSearchQuery[0] == '\0' ||
                ContainsIgnoreCase(item->name.c_str(),
                                   options.stashSearchQuery);
          }
          tabView.slots.push_back(slotView);
        }
        stashView.tabs.push_back(std::move(tabView));
      };

      if (stashType == NoMoreDay::StashType::Shared) {
        NoMoreDay::SharedStash& shared = NoMoreDay::SharedStash::Get();
        const int unlocked = shared.getUnlockedTabCount();
        stashView.tabs.reserve(static_cast<std::size_t>(unlocked));
        for (int i = 0; i < unlocked; ++i) {
          if (const NoMoreDay::StashTab* tab = shared.getTab(i)) {
            fillTab(*tab, static_cast<std::size_t>(i));
          }
        }
        stashView.unlockedTabs = unlocked;
      } else if (const auto* stash = registry.template try_get<
                     const PersonalStashComponent>(player)) {
        stashView.tabs.reserve(stash->tabs.size());
        for (std::size_t i = 0; i < stash->tabs.size(); ++i) {
          fillTab(stash->tabs[i], i);
        }
        stashView.unlockedTabs = stash->unlockedTabs;
      }
      // R7: single unlock-cost authority (StashSystem::getNextUnlockCost
      // delegates to the StashConfig table internally; the builder no longer
      // reads the constant table directly).
      stashView.nextUnlockCost =
          NoMoreDay::StashSystem::getNextUnlockCost(registry, stashType);
    }

    if (const auto* materials =
            registry.template try_get<const MaterialBankComponent>(player)) {
      snapshot.crafting.materials.reserve(materials->materials.size());
      for (const auto& entry : materials->materials) {
        GameUiMaterialView view;
        view.materialId = entry.id;
        view.count = static_cast<std::uint32_t>(std::max(0, entry.count));
        snapshot.crafting.materials.push_back(view);
      }
    }
    // Crafting session targets are UI-local state fed through the options.
    snapshot.crafting.forgeTarget = options.forgeTarget;
    snapshot.crafting.mergeBase = options.mergeBase;
    snapshot.crafting.mergeFodder = options.mergeFodder;
    snapshot.crafting.mergeCatalyst = options.mergeCatalyst;
    snapshot.crafting.salvageItem = options.salvageItem;

    // R7: salvage yield preview computed once here (the legacy panel built a
    // per-frame YieldRange vector in the draw phase; the builder owns the
    // computation and the panel paints the ready-made ranges).
    if (options.salvageItem != kInvalidDomainId) {
      const entt::entity itemEntity = ToEntity(options.salvageItem);
      if (const auto* item =
              registry.template try_get<const NoMoreDay::ItemComponent>(
                  itemEntity)) {
        snapshot.crafting.salvageYield.reserve(item->affixes.size());
        for (const auto& affix : item->affixes) {
          if (affix.type == AffixType::Count) {
            continue;
          }
          const std::uint32_t matId =
              (affix.isLegendary || NoMoreDay::IsLegendaryAffix(affix.type))
                  ? 4999u
                  : 4000u + static_cast<std::uint32_t>(affix.type);
          const int tier = static_cast<int>(affix.tier);
          const int min = (tier < 4) ? 0 : (tier - 3);
          const int max = tier;
          bool merged = false;
          for (auto& range : snapshot.crafting.salvageYield) {
            if (range.materialId == matId) {
              range.min += min;
              range.max += max;
              merged = true;
              break;
            }
          }
          if (!merged) {
            GameUiSalvageYieldView range;
            range.materialId = matId;
            range.min = min;
            range.max = max;
            snapshot.crafting.salvageYield.push_back(range);
          }
        }
      }
    }

    if (const auto* active =
            registry.template try_get<const ActiveSkillsComponent>(player)) {
      const auto& slots = active->slots;
      for (std::size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].id == 0) {
          continue;
        }
        GameUiSkillBarSlotView slotView;
        slotView.skillId = slots[i].id;
        slotView.slotIndex = static_cast<std::uint32_t>(i);
        slotView.cooldown = slots[i].cooldown;
        slotView.currentCharges = slots[i].current_charges;
        // R5: resolve the display data once here (read-only static table);
        // the hotbar paint reads only the snapshot.
        if (const auto* skill =
                SkillRegistry::Get().GetSkill(slots[i].id)) {
          slotView.iconId = skill->icon_id;
          slotView.manaCost = skill->mana_cost;
          slotView.maxCharges = skill->max_charges;
          slotView.cooldownMax = skill->cooldown;
        }
        snapshot.skillBar.slots.push_back(slotView);
      }
      snapshot.skillBar.availableTalentPoints = active->available_talent_points;

      // Learned skills: specialized slots first (in slot order), then active
      // slot skills not already present; deduplicated by skill id.
      std::unordered_set<std::uint32_t> seenSkills;
      const auto addLearned = [&](std::uint32_t skillId, int level) {
        if (skillId == INVALID_SKILL_ID || skillId == 0 ||
            !seenSkills.insert(skillId).second) {
          return;
        }
        GameUiLearnedSkillView learned;
        learned.skillId = skillId;
        learned.level = level;
        snapshot.skillTree.skills.push_back(learned);
      };
      for (std::size_t i = 0; i < active->specialized_slots.size(); ++i) {
        const auto& specialized = active->specialized_slots[i];
        if (specialized.skill_id == INVALID_SKILL_ID ||
            i >= snapshot.skillTree.specializedSlots.size()) {
          continue;
        }
        addLearned(specialized.skill_id,
                   1 + std::max(0, specialized.bonus_levels));
        // R8: specialized slot view model (talent-tree paint/interaction
        // data; bounded allocation cache copied from the gameplay map).
        GameUiSpecializedSlotView& slotView =
            snapshot.skillTree.specializedSlots[i];
        slotView.skillId = specialized.skill_id;
        slotView.level = 1 + std::max(0, specialized.bonus_levels);
        if (const auto* skill =
                SkillRegistry::Get().GetSkill(specialized.skill_id)) {
          slotView.iconAssetId = skill->icon_id;
        }
        slotView.allocatedPoints.reserve(specialized.allocated_points.size());
        for (const auto& [nodeId, points] : specialized.allocated_points) {
          slotView.allocatedPoints.emplace_back(nodeId, points);
        }
      }
      for (const auto& slot : active->slots) {
        addLearned(slot.id, 1);
      }

      // R8: talent-point budget for the tree interaction (the panel spends
      // points via SkillAllocateTalentPoint; the handler re-validates).
      snapshot.skillTree.availableTalentPoints = active->available_talent_points;

      // R8: mastery hub state (cards + selection + attunement + debug gate).
      snapshot.skillTree.hasBladeProfession =
          NoMoreDay::systems::BladeMasteryService::HasBladeAscendantProfession(
              registry, player);
      snapshot.skillTree.debugUnlockEnabled =
          NoMoreDay::systems::BladeMasteryService::IsDebugUnlockOverrideEnabled();
      snapshot.skillTree.selectedMastery = static_cast<std::uint8_t>(
          NoMoreDay::systems::BladeMasteryService::GetSelectedMastery(
              registry, player));
      if (const auto* masteryComp =
              registry.template try_get<const BladeMasteryComponent>(player)) {
        snapshot.skillTree.heavenlyAttunement =
            static_cast<std::uint8_t>(masteryComp->heavenly_attunement);
      }
      const auto& masteryProfiles =
          NoMoreDay::data::BladeMasteryRegistry::Get().GetAllProfiles();
      snapshot.skillTree.masteryCards.reserve(masteryProfiles.size());
      for (const auto& profile : masteryProfiles) {
        if (profile.id == NoMoreDay::BladeMasteryId::None) {
          continue;
        }
        GameUiMasteryCardView card;
        card.masteryId = static_cast<std::uint8_t>(profile.id);
        card.signatureSkillId = profile.signature_skill_id;
        card.unlockLevel = profile.unlock_level;
        card.debugUnlockLevelOverride = profile.debug_unlock_level_override;
        // Copy the display name into the fixed buffer (bounded copy).
        const std::size_t nameLen =
            std::min<std::size_t>(profile.name.size(), card.name.size() - 1);
        std::memcpy(card.name.data(), profile.name.data(), nameLen);
        card.name[nameLen] = '\0';
        card.unlocked =
            NoMoreDay::systems::BladeMasteryService::IsMasteryUnlocked(
                registry, player, profile.id);
        card.selected =
            snapshot.skillTree.selectedMastery == card.masteryId;
        card.debugUnlocked =
            card.unlocked && profile.debug_unlock_level_override >= 0;
        snapshot.skillTree.masteryCards.push_back(card);
        if (card.unlocked && profile.signature_skill_id != 0 &&
            !NoMoreDay::systems::BladeMasteryService::IsSignatureSkillUnlocked(
                registry, player, profile.signature_skill_id)) {
          snapshot.skillTree.lockedSignatureSkills.push_back(
              profile.signature_skill_id);
        }
      }

      // R8: mutual-keystone exclusions per specialized skill (the talent-tree
      // interaction needs the exclusion set to gate node clicks; resolved by
      // the builder so the controller never reads the ECS).
      for (std::size_t i = 0; i < active->specialized_slots.size(); ++i) {
        const auto& specialized = active->specialized_slots[i];
        if (specialized.skill_id == INVALID_SKILL_ID) {
          continue;
        }
        const auto* tree =
            SkillRegistry::Get().GetSkillTree(specialized.skill_id);
        if (tree == nullptr) {
          continue;
        }
        GameUiSkillExcludedNodes excluded;
        excluded.skillId = specialized.skill_id;
        for (const auto& [nodeId, node] : tree->nodes) {
          (void)node;
          if (NoMoreDay::SkillSystem::
                  IsNodeExcludedByMutualKeystone(registry, player,
                                                 specialized.skill_id,
                                                 nodeId)) {
            excluded.nodeIds.push_back(nodeId);
          }
        }
        snapshot.skillTree.excludedBySkill.push_back(std::move(excluded));
      }
    }
    if (const auto* stats = registry.template try_get<PlayerStats>(player)) {
      snapshot.skillTree.availableSkillPoints = stats->available_skill_points;
    }

    if (const auto* astro =
            registry.template try_get<const AstrolabeComponent>(player)) {
      GameUiAstrolabeView& view = snapshot.astrolabe;
      view.present = true;
      view.availablePoints = astro->available_points;
      view.mainProfession = astro->mainProfession;
      view.professionAffinity = astro->professionAffinity;
      view.activatedNodes.reserve(astro->activated_nodes.size());
      for (const std::uint32_t nodeId : astro->activated_nodes) {
        view.activatedNodes.push_back(nodeId);
      }
      // R8: vow state + per-node points (the astrolabe custom painter
      // resolves node status without touching the ECS).
      view.hasVow = astro->hasVow();
      view.nodePoints.reserve(astro->nodePoints.size());
      for (const auto& [nodeId, points] : astro->nodePoints) {
        view.nodePoints.emplace_back(nodeId, static_cast<std::int32_t>(points));
      }
    }
  }

  // --- World monsters (health bars / minimap dots) -------------------------
  // Killed/dormant monsters are excluded: the health bars must not paint dead
  // bodies and the minimap dot pass reads the same view-model.
  const auto monsterView =
      registry.template view<const EnemyTag, const HealthComponent>(
          entt::exclude<KilledTag, DormantTag>);
  for (const entt::entity monster : monsterView) {
    const auto& hp = monsterView.template get<const HealthComponent>(monster);
    GameUiMonsterHealthView view;
    view.domainId = ToDomainId(monster);
    view.current = hp.current;
    view.max = hp.max;
    if (const auto* position =
            registry.template try_get<const Position>(monster)) {
      view.worldX = position->x;
      view.worldY = position->y;
    }
    if (const auto* state =
            registry.template try_get<const EnemyStateComponent>(monster)) {
      view.raceType = static_cast<std::uint8_t>(state->raceType);
    }
    if (const auto* rarity =
            registry.template try_get<const EnemyRarityComponent>(monster)) {
      view.isElite = rarity->rarity > EnemyRarityComponent::NORMAL;
      view.rarity = static_cast<std::uint8_t>(rarity->rarity);
    }
    if (const auto* radius = registry.template try_get<const Radius>(monster)) {
      view.radius = radius->value;
    }
    if (const auto* affix =
            registry.template try_get<const MonsterAffixComponent>(monster)) {
      const std::size_t n =
          std::min<std::size_t>(affix->affixes.size(), view.affixTypes.size());
      for (std::size_t i = 0; i < n; ++i) {
        view.affixTypes[i] = static_cast<std::uint8_t>(affix->affixes[i]);
      }
      view.affixCount = static_cast<std::uint8_t>(n);
    }
    snapshot.monsters.push_back(view);
  }

  // --- Active buffs (hotbar buff strip) -----------------------------------
  if (hasPlayer) {
    const entt::entity player = playerView.front();
    if (const auto* effects =
            registry.template try_get<const ActiveEffectsComponent>(player)) {
      snapshot.buffs.reserve(effects->effects.size());
      for (const auto& effect : effects->effects) {
        GameUiBuffView view;
        view.buffType = static_cast<std::uint8_t>(effect.type);
        view.remaining = effect.remaining;
        view.duration = effect.duration;
        view.stacks = effect.stacks;
        view.isDebuff = effect.is_debuff;
        const auto& visual = BuffRegistry::GetVisualData(effect.type);
        view.iconAssetId =
            visual.icon_asset != nullptr
                ? static_cast<std::uint32_t>(visual.icon_asset->id)
                : 0u;
        // R8: bounded buff-tooltip text (the gameplay BuffEffect owns dynamic
        // strings; copied + truncated so the tooltip paint never reads the
        // ECS). CopyToFixed appends a terminating NUL when the source fits.
        const auto copyToFixed = [](const std::string& src, auto& dst) {
          const std::size_t n =
              std::min(src.size(), dst.size() - 1);
          std::memcpy(dst.data(), src.data(), n);
          dst[n] = '\0';
        };
        copyToFixed(effect.name, view.name);
        copyToFixed(effect.description, view.description);
        snapshot.buffs.push_back(view);
      }
    }
  }

  // --- Minimap view --------------------------------------------------------
  if (hasPlayer) {
    const entt::entity player = playerView.front();
    GameUiMinimapView& minimap = snapshot.minimap;
    if (const auto* stats = registry.template try_get<PlayerStats>(player)) {
      minimap.currentMapKills = stats->current_map_kills;
    }
    minimap.killRequirement =
        static_cast<std::uint32_t>(
            NoMoreDay::Constants::Enemy::NEXT_LEVEL_PORTAL_KILL_REQUIREMENT);
    if (registry.ctx().template contains<ActiveDimensionalState>()) {
      const auto& dim =
          registry.ctx().template get<ActiveDimensionalState>();
      minimap.dimensionalActive = dim.isActive;
      minimap.dimensionalDepth = dim.depthLevel;
      minimap.dimensionalBaseLevel = dim.selectedBaseLevel;
    }
    // Next-level portal direction (used by the minimap navigation arrow).
    const auto portalView =
        registry.template view<const PortalComponent, const Position>();
    for (const entt::entity portal : portalView) {
      const auto& comp = portalView.template get<const PortalComponent>(portal);
      if (comp.type != PortalType::NextLevel) {
        continue;
      }
      const auto& pos = portalView.template get<const Position>(portal);
      minimap.hasNextLevelPortal = true;
      minimap.nextLevelPortalX = pos.x;
      minimap.nextLevelPortalY = pos.y;
      break;
    }
  }

  // --- Pickup targets + ground item display cache -------------------------
  std::unordered_map<std::uint64_t, GameUiItemView> groundById;
  if (hasPlayerPosition) {
    const auto itemView =
        registry.template view<const NoMoreDay::ItemComponent, const Position>();
    for (const entt::entity item : itemView) {
      if (!registry.valid(item)) {
        continue; // Defensive: skip stale entities.
      }
      const auto& itemPos = itemView.template get<const Position>(item);
      const float dx = itemPos.x - playerX;
      const float dy = itemPos.y - playerY;
      const float distSq = dx * dx + dy * dy;
      if (distSq > kPickupRange * kPickupRange) {
        continue;
      }

      GameUiPickupSnapshot pickup;
      pickup.domainId = entt::to_integral(item);
      pickup.distance = std::sqrt(distSq);
      pickup.source = GameUiPickupSource::World;
      snapshot.pickups.push_back(pickup);

      GameUiItemView view = ToItemView(registry, item, -1, -1);
      groundById.emplace(view.domainId, std::move(view));
    }

    // Deterministic order: nearest first.
    std::sort(snapshot.pickups.begin(), snapshot.pickups.end(),
              [](const GameUiPickupSnapshot& lhs,
                 const GameUiPickupSnapshot& rhs) {
                return lhs.distance < rhs.distance;
              });
  }

  // --- Displayed items: UI session targets resolved through the item maps --
  // The hover/drag/crafting targets are UI session state (design §3.2: not
  // stored in the snapshot); their display data is resolved here, once, from
  // the read-only item caches above.
  snapshot.tooltip.hoveredItem = options.hoveredItem;
  snapshot.tooltip.hoveredSkillId = kInvalidSkillId;
  {
    std::unordered_set<std::uint64_t> seen;
    const std::array<std::uint64_t, 8> requested = {
        options.hoveredItem,       options.draggedItem,
        options.contextMenuItem,   options.forgeTarget,
        options.mergeBase,         options.mergeFodder,
        options.mergeCatalyst,     options.salvageItem,
    };
    for (const std::uint64_t domainId : requested) {
      if (domainId == kInvalidDomainId || !seen.insert(domainId).second) {
        continue;
      }
      const GameUiItemView* resolved = nullptr;
      auto invIt = inventoryById.find(domainId);
      if (invIt != inventoryById.end()) {
        resolved = &invIt->second;
      }
      if (resolved == nullptr) {
        auto groundIt = groundById.find(domainId);
        if (groundIt != groundById.end()) {
          resolved = &groundIt->second;
        }
      }
      if (resolved == nullptr) {
        auto equipIt = equipmentById.find(domainId);
        if (equipIt != equipmentById.end()) {
          resolved = &equipIt->second;
        }
      }
      if (resolved != nullptr) {
        snapshot.displayedItems.push_back(*resolved);
        // R8: fill the tooltip payload for the bounded displayed-items cache
        // only (names/descriptions/stat rows + rune names; the tooltip paint
        // path then renders entirely from the snapshot).
        FillTooltipData(registry, snapshot.displayedItems.back());
      }
    }
  }

  // notifications are produced by GameUiCommandHandler (U6b); the builder
  // leaves the queue empty for the UI to consume.

  return snapshot;
}

template GameUiSnapshot GameUiSnapshotBuilder::Build<entt::registry>(
    const entt::registry&, const GameUiSnapshotOptions&);

} // namespace NoMoreDay::ui
