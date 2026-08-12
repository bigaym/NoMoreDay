#pragma once

// Frame-scoped, read-only panel view model for the Game UI (design §3.2).
//
// Hard constraints (design §3.2, remediation plan R1):
//  - This header is pure data: no entt::registry / entt::entity, no raylib,
//    no component pointers/references and no writable gameplay objects.
//  - Gameplay entities are referenced exclusively by stable integer domain
//    ids (the numeric form of the entity id, entt::to_integral in practice).
//  - Values are plain POD / std::vector; the UI never receives a registry
//    reference and never copies the whole registry.
//  - Dynamic text prefers numeric values, text/resource ids or pre-formatted
//    bounded caches; item names are NOT duplicated here (the render side
//    resolves itemId/textureId through the item template tables).
//  - UI session state (panel open/tab/search/drag/scroll/hover/modal) belongs
//    to the controllers / UiRuntime and never enters the snapshot, never
//    enters the save file and never writes back to gameplay.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace NoMoreDay::ui {

// Sentinel domain ids: 0 = "no entity" (entt::null round-trips to 0).
inline constexpr std::uint64_t kInvalidDomainId = 0;
// Sentinel skill id (NoMoreDay::INVALID_SKILL_ID is a gameplay constant and
// must not leak into this header).
inline constexpr std::uint32_t kInvalidSkillId = 0xFFFFFFFFu;

// Where a pickup candidate comes from. World items are discovered by the
// snapshot builder; UiNode targets are introduced by later panel migrations.
enum class GameUiPickupSource : std::uint8_t {
  World,  // Ground item entity found in the world.
  UiNode, // Item handle surfaced through a UI tree node (reserved).
};

// A single pickup candidate shown to the UI.
// domainId is the stable domain identifier (the numeric form of the item
// entity id); it must be re-validated (entity validity, distance, capacity)
// by the command handler before any gameplay change (design §6.2).
struct GameUiPickupSnapshot {
  std::uint64_t domainId = kInvalidDomainId;
  float distance = 0.0f; // World-space distance from the player.
  GameUiPickupSource source = GameUiPickupSource::World;
};

// One affix of an item, as a value snapshot (the gameplay Affix struct also
// carries vector payloads that are not needed by the panels).
struct GameUiAffixView {
  std::uint16_t type = 0; // NoMoreDay::AffixType underlying value.
  float value = 0.0f;
  std::int32_t tier = 0;
  bool isPrefix = false;
  bool isLegendary = false;
};

// Read-only item display data shared by the inventory / equipment / stash /
// crafting / tooltip / context-menu panels. Names and dynamic text are not
// duplicated; the render side resolves itemId/textureId through the item
// template tables, and affix descriptions are rebuilt from the bounded affix
// cache exactly as the existing panels display them.
struct GameUiItemView {
  std::uint64_t domainId = kInvalidDomainId;
  std::uint32_t itemId = 0;   // Item template id (name/texture lookup).
  std::uint32_t textureId = 0;
  std::uint32_t quantity = 1;
  std::uint32_t maxStack = 1;
  std::uint8_t rarity = 0;    // NoMoreDay::Rarity value.
  std::uint8_t itemType = 0;  // NoMoreDay::ItemType value.
  std::uint8_t slot = 0;      // NoMoreDay::EquipmentSlot value.
  std::uint8_t socketCount = 0;
  std::int32_t itemLevel = 0;
  std::int32_t forgingPotential = 0;
  std::int32_t legendaryPotential = 0;
  bool isLocked = false;
  bool isTwoHanded = false;
  // Placement within the player's inventory (index in InventoryComponent.items
  // or bag_slots); -1 when the item is not in the player inventory.
  std::int32_t inventoryIndex = -1;
  std::int32_t bagSlotIndex = -1;
  // R6: socket contents (rune item ids per socket; 0 = free). The inventory
  // controller needs the first free socket index to build the SocketRune
  // intent without touching the ECS (the command handler re-validates).
  std::array<std::uint32_t, 4> sockets{};
  // Bounded per-item caches (empty when the item has no affixes).
  std::vector<GameUiAffixView> affixes;
  std::vector<GameUiAffixView> implicits;
  // R8: tooltip payload resolved by the builder from the item component +
  // template tables (design §3.2: names are NOT duplicated for every item,
  // only when the item is requested into displayedItems — see the builder
  // tooltip-fill step). attack/defense/bagCapacity back the tooltip stat rows.
  std::string name;
  std::string description;
  float attack = 0.0f;
  float defense = 0.0f;
  std::int32_t bagCapacity = 0;
  // R8: rune names of filled sockets (bounded; 0 = free socket). The item
  // tooltip's "Runes:" row is rebuilt from these without touching the ECS.
  std::array<std::array<char, 40>, 4> socketRuneNames{};
};

// One grouped summon row (HUD top-left widget). R5: replaces the per-frame
// std::map aggregation with a builder-provided bounded group list. Defined
// before GameUiPlayerSnapshot (which stores the group vector).
struct GameUiSummonGroupView {
  std::uint32_t key = 0;           // skill_id != 0 ? skill_id : archetype_id.
  std::uint32_t skillId = 0;
  std::uint32_t archetypeId = 0;   // SummonArchetype value.
  std::uint32_t iconId = 0;
  std::uint32_t count = 0;
  float maxLifeRatio = 0.0f;
};

// Player HUD / character basic fields (HealthComponent / PlayerStats /
// InventoryComponent / CombatStats).
struct GameUiPlayerSnapshot {
  bool hasPlayer = false;
  std::uint64_t domainId = kInvalidDomainId;
  float health = 0.0f;
  float maxHealth = 0.0f;
  float mana = 0.0f;
  float maxMana = 0.0f;
  float barrier = 0.0f;
  float maxBarrier = 0.0f;
  std::int32_t level = 1;
  float currentXp = 0.0f;
  float requiredXp = 100.0f;
  std::int32_t availableAttributePoints = 0;
  std::int32_t availableSkillPoints = 0;
  std::int32_t inventoryUsed = 0;     // Occupied inventory slots.
  std::int32_t inventoryCapacity = 0; // Total inventory capacity.
  std::int32_t gold = 0;
  // R6: player avatar texture id (resolved by the builder from the player
  // SpriteComponent; 0 = none). Used by the character panel paint path.
  std::uint32_t avatarTextureId = 0;
  // Blade-resource widget (HUD): raw values only; the label/text resolution
  // stays on the HUD side (numeric values are preferred over text caches).
  bool hasBladeResource = false;
  std::uint8_t bladeResourceKind = 0; // BladeResourceKind value.
  std::int32_t bladeResourceCurrent = 0;
  std::int32_t bladeResourceMax = 0;
  // R5: blade-mastery state (BladeMasteryComponent value snapshot).
  bool hasMastery = false;
  std::uint8_t masterySelected = 0;     // BladeMasteryId value.
  std::uint8_t heavenlyAttunement = 0;  // BladeAttunement value.
  bool bloodOathActive = false;
  // R5: blade-runtime feedback cues (BladeResourceComponent runtime fields).
  bool restartWindowReady = false;
  float restartWindowTimer = 0.0f;
  float critBonusFeedbackTimer = 0.0f;
  // R5: active field windows resolved by the builder (read-only query).
  float heavenlyFieldDuration = 0.0f;
  bool bloodSeaHasVoidKeystone = false;
  float bloodSeaMiasmaBonus = 0.0f;
  // R5: sword-intent fallback widget (SwordIntentComponent, used when no
  // BladeResourceComponent is present).
  bool hasSwordIntent = false;
  std::int32_t swordIntentStacks = 0;
  std::int32_t swordIntentMaxStacks = 0;
  // Summon status (HUD).
  bool hasSummon = false;
  std::uint32_t summonCount = 0;
  // R5: player world position (minimap marker + monster hover anchors).
  bool hasWorldPosition = false;
  float worldX = 0.0f;
  float worldY = 0.0f;
  // R5: per-key summon groups (grouped by skill_id / archetype_id, max life
  // ratio kept). The display name is resolved by the HUD side from
  // archetypeId/skillId (numeric ids preferred over text caches).
  std::vector<GameUiSummonGroupView> summonGroups;
};

// Character panel combat stats (CombatStats, value snapshot; the panel shows
// the effective numbers, so raw/effective pairs are resolved by the builder).
struct GameUiCharacterStatsView {
  float strength = 0.0f;
  float dexterity = 0.0f;
  float intelligence = 0.0f;
  float vitality = 0.0f;
  float effectiveStrength = 0.0f;
  float effectiveDexterity = 0.0f;
  float effectiveIntelligence = 0.0f;
  float effectiveVitality = 0.0f;
  float health = 0.0f;
  float maxHealth = 0.0f;
  float mana = 0.0f;
  float maxMana = 0.0f;
  float barrier = 0.0f;
  float maxBarrier = 0.0f;
  float minWeaponDamage = 0.0f;
  float maxWeaponDamage = 0.0f;
  float critChance = 0.0f;
  float critDamage = 0.0f;
  float attackSpeed = 0.0f;
  float castSpeed = 0.0f;
  float accuracy = 0.0f;
  float armor = 0.0f;
  float armorDr = 0.0f;
  float dodgeRating = 0.0f;
  float dodgeChance = 0.0f;
  float blockChance = 0.0f;
  float blockRating = 0.0f;
  float blockEffect = 0.0f;
  float damageReduction = 0.0f;
  float thorns = 0.0f;
  float healthRegen = 0.0f;
  float manaRegen = 0.0f;
  float lifeSteal = 0.0f;
  float lifeOnHit = 0.0f;
  float manaOnHit = 0.0f;
  float moveSpeed = 0.0f;
  float magicFind = 0.0f;
  float cooldownReduction = 0.0f;
  float pickupRange = 0.0f;
  float goldBonus = 0.0f;
  float experienceGainMult = 0.0f;
  std::array<float, 6> resistances{}; // NoMoreDay::DamageType order.
  // R6: damage-composition + raw/effective pairs shown by the character panel
  // tab content (copied by the builder so the paint path never reads the ECS).
  float armorPen = 0.0f;
  std::array<float, 6> flatDamage{};
  std::array<float, 6> damageMultipliers{};
  float rawAttackSpeed = 0.0f;
  float rawBlockChance = 0.0f;
  float rawMoveSpeed = 0.0f;
  float rawCooldownReduction = 0.0f;
  std::array<float, 6> rawResistances{};
  float durationScale = 0.0f;
  float areaScale = 0.0f;
};

// One world enemy health bar (MonsterHealthBarController data source).
struct GameUiMonsterHealthView {
  std::uint64_t domainId = kInvalidDomainId;
  float current = 0.0f;
  float max = 0.0f;
  bool isElite = false; // EnemyRarityComponent presence.
  // R5: world-space position (overhead bar transform + hover pick).
  float worldX = 0.0f;
  float worldY = 0.0f;
  // R5: enemy identity for the target widget text.
  std::uint8_t raceType = 0;    // EnemyRace::Type value.
  std::uint8_t rarity = 0;      // EnemyRarityComponent::Rarity value.
  float radius = 0.0f;          // Radius component (hover pick), 0 = default.
  // R5: monster affixes (bounded: at most 4, the gameplay cap).
  std::array<std::uint8_t, 4> affixTypes{};
  std::uint8_t affixCount = 0;
};

// R6: one bag extension slot with its display data (the inventory panel
// paints bag slots from the snapshot without touching the ECS).
struct GameUiBagSlotView {
  std::uint64_t domainId = kInvalidDomainId;
  std::uint32_t itemId = 0; // Item template id (name/texture lookup).
  std::uint32_t textureId = 0;
  std::uint8_t rarity = 0;  // NoMoreDay::Rarity value.
  std::uint32_t quantity = 1;
};

// Inventory panel view: occupied slots plus the bag extension slots.
struct GameUiInventoryView {
  std::int32_t capacity = 0;
  std::int32_t used = 0;
  std::int32_t gold = 0;
  // Occupied inventory slots (each carries its inventoryIndex; empty slots
  // are implicit and rendered from capacity).
  std::vector<GameUiItemView> items;
  // R6: bag extension slots with display data (the inventory panel paints
  // them from the snapshot; the old uint64-only form forced a registry read
  // in the draw path).
  std::array<GameUiBagSlotView, 4> bagSlots{};
};

// One equipped slot (EquipmentComponent.slots). R6: carries the display data
// the character/inventory panels need to paint from the snapshot alone.
struct GameUiEquippedSlotView {
  std::uint8_t slotIndex = 0; // NoMoreDay::EquipmentSlot value.
  std::uint64_t domainId = kInvalidDomainId;
  std::uint32_t itemId = 0;   // Item template id (name/texture lookup).
  std::uint32_t textureId = 0;
  std::uint8_t rarity = 0;    // NoMoreDay::Rarity value.
  std::uint32_t quantity = 1;
  std::uint8_t itemType = 0;  // NoMoreDay::ItemType value.
  bool isLocked = false;
  std::uint8_t socketCount = 0;
  // Socket contents (0 = free socket; needed to build the SocketRune intent
  // from the snapshot without touching the ECS).
  std::array<std::uint32_t, 4> sockets{};
};

// One occupied stash slot (lightweight: icon/rarity/quantity; full display
// data is resolved through the displayed-items section on hover).
struct GameUiStashSlotView {
  std::int32_t slotIndex = -1;
  std::uint64_t domainId = kInvalidDomainId;
  std::uint32_t textureId = 0;
  std::uint8_t rarity = 0;
  std::uint32_t quantity = 0;
  // R7: search-match flag computed by the builder from the UI search query
  // (options.stashSearchQuery). The panel dims non-matching slots; the query
  // and the name matching live in the builder so the controller never needs
  // registry/name access in the paint path.
  bool matchesSearch = true;
};

// Stash panel view (per-tab metadata + occupied slots).
struct GameUiStashTabView {
  std::uint8_t tabType = 0; // NoMoreDay::StashTabType value.
  std::uint32_t iconId = 0;
  std::uint32_t color = 0xFFFFFFFFu;
  // R7: bounded tab-name cache (design §3.2: no std::string copies in the
  // snapshot; the panel tab strip only needs the display label).
  std::array<char, 16> name{};
  std::vector<GameUiStashSlotView> slots;
};

struct GameUiStashView {
  std::vector<GameUiStashTabView> tabs;
  std::int32_t unlockedTabs = 0;
  std::int32_t nextUnlockCost = 0;
};

// Crafting panel view: the domain ids of the selected forge/merge/salvage
// targets plus the material bank. The targets are UI session state fed into
// the builder via GameUiSnapshotOptions; their full display data is resolved
// through the displayed-items section.
struct GameUiMaterialView {
  std::uint32_t materialId = 0;
  std::uint32_t count = 0;
};

// R7: one salvage yield range for the salvage panel preview (material id and
// min/max quantities derived from the item's affix tiers; computed by the
// builder so the panel never builds per-frame vectors).
struct GameUiSalvageYieldView {
  std::uint32_t materialId = 0;
  std::int32_t min = 0;
  std::int32_t max = 0;
};

struct GameUiCraftingView {
  std::uint64_t forgeTarget = kInvalidDomainId;
  std::uint64_t mergeBase = kInvalidDomainId;
  std::uint64_t mergeFodder = kInvalidDomainId;
  std::uint64_t mergeCatalyst = kInvalidDomainId;
  std::uint64_t salvageItem = kInvalidDomainId;
  std::vector<GameUiMaterialView> materials;
  // R7: builder-computed salvage yield preview (bounded by the item's affix
  // count; empty when no salvage target is selected).
  std::vector<GameUiSalvageYieldView> salvageYield;
};

// Skill hotbar view (ActiveSkillsComponent.slots).
struct GameUiSkillBarSlotView {
  std::uint32_t skillId = kInvalidSkillId;
  std::uint32_t slotIndex = 0;
  float cooldown = 0.0f;        // Remaining cooldown seconds.
  std::int32_t currentCharges = 0;
  // R5: display data resolved by the builder from the static skill table
  // (numeric/resource ids preferred over text caches).
  std::uint32_t iconId = 0;
  float manaCost = 0.0f;
  std::int32_t maxCharges = 1;
  float cooldownMax = 0.0f;
};

struct GameUiSkillBarView {
  std::vector<GameUiSkillBarSlotView> slots;
  std::int32_t availableTalentPoints = 0;
};

// One active buff row (hotbar buff strip). R5: builder-resolved display data
// from ActiveEffectsComponent + the static buff visual table.
struct GameUiBuffView {
  std::uint8_t buffType = 0;    // BuffType value.
  float remaining = 0.0f;
  float duration = 0.0f;
  std::int32_t stacks = 0;
  bool isDebuff = false;
  std::uint32_t iconAssetId = 0; // BuffVisualData.icon_asset id (0 = none).
  // R8: bounded buff-tooltip text (the gameplay BuffEffect carries dynamic
  // std::string fields; the builder copies + truncates them so the tooltip
  // paint path never touches the ECS).
  std::array<char, 64> name{};
  std::array<char, 128> description{};
};

// Skill tree / skill hub view: learned skills and their levels.
struct GameUiLearnedSkillView {
  std::uint32_t skillId = kInvalidSkillId;
  std::int32_t level = 0;
};

// R8: one specialized (talent) skill slot with its allocation summary. The
// talent-tree painter/interaction resolves node state from allocatedPoints
// (bounded by the gameplay node cap) without touching the ECS.
struct GameUiSpecializedSlotView {
  std::uint32_t skillId = kInvalidSkillId;
  std::int32_t level = 0;
  std::uint32_t iconAssetId = 0;
  // Per-node talent allocation (nodeId -> points); copied from
  // SpecializedSkill.allocated_points (bounded).
  std::vector<std::pair<std::uint32_t, std::int32_t>> allocatedPoints;
};

// R8: one mastery card's display state (resolved by the builder so the hub
// paint/interaction never touches the ECS).
struct GameUiMasteryCardView {
  std::uint8_t masteryId = 0;       // BladeMasteryId value.
  std::uint32_t signatureSkillId = 0; // profile.signature_skill_id.
  std::int32_t unlockLevel = 0;
  std::int32_t debugUnlockLevelOverride = 0;
  bool unlocked = false;
  bool selected = false;
  bool debugUnlocked = false;
  std::array<char, 48> name{};      // profile name (display).
};

// R8: builder-computed mutual-keystone exclusion list for one specialized
// skill (SkillSystem::IsNodeExcludedByMutualKeystone, resolved per skill so
// the talent-tree interaction never touches the ECS).
struct GameUiSkillExcludedNodes {
  std::uint32_t skillId = kInvalidSkillId;
  std::vector<std::uint32_t> nodeIds;
};

struct GameUiSkillTreeView {
  std::int32_t availableSkillPoints = 0;
  std::vector<GameUiLearnedSkillView> skills;
  // R8: talent-point budget + specialized slots + mastery hub state.
  std::int32_t availableTalentPoints = 0;
  std::array<GameUiSpecializedSlotView, 5> specializedSlots{};
  std::vector<GameUiMasteryCardView> masteryCards;
  // R8: signature skills currently locked for the hub (data-driven unlock
  // gate; the hub dims their cards / disables assignment).
  std::vector<std::uint32_t> lockedSignatureSkills;
  std::uint8_t selectedMastery = 0;    // BladeMasteryId value (0 = none).
  std::uint8_t heavenlyAttunement = 0; // BladeAttunement value.
  bool hasBladeProfession = false;
  bool debugUnlockEnabled = false;
  // R8: mutual-keystone exclusions per specialized skill (empty when no
  // specialized skill is present).
  std::vector<GameUiSkillExcludedNodes> excludedBySkill;
};

// Astrolabe panel view (AstrolabeComponent value snapshot).
struct GameUiAstrolabeView {
  bool present = false;
  std::int32_t availablePoints = 0;
  std::int32_t mainProfession = -1;
  std::array<std::int32_t, 6> professionAffinity{};
  std::vector<std::uint32_t> activatedNodes; // Stable node ids.
  // R8: profession vow state + per-node point values (the custom painter
  // resolves node status from these without touching the ECS).
  bool hasVow = false;
  std::vector<std::pair<std::uint32_t, std::int32_t>> nodePoints;
};

// Overlay/tooltip view: the hovered target (UI session state resolved through
// the displayed-items section) plus the notifications from the previous
// Update's GameUiResult queue.
struct GameUiTooltipView {
  std::uint64_t hoveredItem = kInvalidDomainId;
  std::uint32_t hoveredSkillId = kInvalidSkillId;
};

// A message for the UI to surface (pickup failures, errors, ...). Produced by
// the command handler and carried into the next snapshot/notification channel.
struct GameUiNotification {
  std::string message;
};

// Minimap view (R5): builder-resolved world/zone data. The fog texture stays
// a controller-owned GPU resource; the paint path reads only this value model.
// Defined before GameUiSnapshot (which stores the minimap field).
struct GameUiMinimapView {
  // Kill progress toward the next-level portal.
  std::uint32_t currentMapKills = 0;
  std::uint32_t killRequirement = 0; // NEXT_LEVEL_PORTAL_KILL_REQUIREMENT.
  // Active dimensional state (registry ctx, resolved by the builder).
  bool dimensionalActive = false;
  std::int32_t dimensionalDepth = 0;
  std::int32_t dimensionalBaseLevel = 0;
  // Next-level portal world position (0/0 + hasPortal when absent).
  bool hasNextLevelPortal = false;
  float nextLevelPortalX = 0.0f;
  float nextLevelPortalY = 0.0f;
};

// Aggregated frame-scoped read model handed to the UI each Update.
struct GameUiSnapshot {
  std::uint64_t revision = 0; // Incremented by the builder on every Build.
  GameUiPlayerSnapshot player;
  GameUiCharacterStatsView characterStats;
  std::vector<GameUiMonsterHealthView> monsters;
  // R5: active buffs for the hotbar buff strip (bounded by the gameplay
  // effect cap; the builder only copies the display-relevant fields).
  std::vector<GameUiBuffView> buffs;
  GameUiInventoryView inventory;
  std::vector<GameUiEquippedSlotView> equipment;
  GameUiStashView stash;
  GameUiCraftingView crafting;
  GameUiSkillBarView skillBar;
  GameUiSkillTreeView skillTree;
  GameUiAstrolabeView astrolabe;
  GameUiTooltipView tooltip;
  // R5: minimap view (player/enemy world positions live on player/monsters;
  // this carries the zone/kill/portal data the minimap needs).
  GameUiMinimapView minimap;
  // Bounded display-data cache for the UI session targets (hover/drag/
  // crafting forge/salvage/merge slots); deduplicated by domain id.
  std::vector<GameUiItemView> displayedItems;
  std::vector<GameUiPickupSnapshot> pickups;
  std::vector<GameUiNotification> notifications;
};

// UI session display requests fed into the builder. These are UI-local state
// (hover/drag/crafting targets/active stash tab) provided by the host; they
// are input to the build step, never stored in the snapshot and never written
// back to gameplay.
struct GameUiSnapshotOptions {
  std::uint64_t hoveredItem = kInvalidDomainId;
  std::uint64_t draggedItem = kInvalidDomainId;
  // R6: the item the context menu is open on (resolved into displayedItems so
  // the overlay paints the menu from the snapshot view model).
  std::uint64_t contextMenuItem = kInvalidDomainId;
  std::uint64_t forgeTarget = kInvalidDomainId;
  std::uint64_t mergeBase = kInvalidDomainId;
  std::uint64_t mergeFodder = kInvalidDomainId;
  std::uint64_t mergeCatalyst = kInvalidDomainId;
  std::uint64_t salvageItem = kInvalidDomainId;
  std::int32_t stashActiveTab = 0;
  // R7: active stash container type (NoMoreDay::StashType value) so the
  // builder snapshots the correct Personal/Shared tab set.
  std::uint32_t stashType = 0;
  // R7: stash search query (borrowed controller buffer, valid for the build
  // call only; empty/null = no search filter). The builder computes the
  // per-slot matchesSearch flags from it.
  const char* stashSearchQuery = nullptr;
};

} // namespace NoMoreDay::ui
