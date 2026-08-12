#pragma once

// UI -> gameplay request contract (design §6.2, remediation plan R1).
//
// An intent is a single tagged POD payload: it carries the issuing UI node, a
// kind and a flat payload of source/target slots, quantity, tab, affix index,
// enums and boolean flags — never gameplay object references, registry
// addresses, component pointers/references or cross-frame entt::entity values.
// GameUiCommandHandler re-validates the domain ids against the live registry
// (entity validity, ownership, distance, capacity, slot/tab/index, domain
// preconditions) and only then mutates the world during the Update phase.
//
// This header is pure data: standard library and UiRuntimeTypes value types
// only. No EnTT, no raylib, no gameplay headers.

#include <cstdint>
#include <string>
#include <vector>

#include "game/application/ui/UiRuntimeTypes.hpp"

namespace NoMoreDay::ui {

// Every gameplay action the UI can request. Panel-local behaviour (selecting a
// crafting target, toggling panels, editing quantity text, searching, drag
// preview, closing overlays) is UI-local and must NOT create an intent.
enum class GameUiIntentKind : std::uint8_t {
  // Ground / inventory / equipment / bag.
  PickupItem,
  EquipItem,
  UnequipItem,
  UseItem,
  DropItem,
  DestroyItem,
  LockItem,
  UnlockItem,
  OrganizeInventory,
  MoveItem,   // Move an inventory slot into an empty slot.
  SwapItems,  // Swap two occupied inventory slots.
  BagEquip,   // Equip a bag into a bag slot.
  BagUnequip, // Unequip a bag back into the inventory.
  SocketRune,
  UnsocketRune,
  // Stash.
  StashTransfer,    // Move an item between stash tabs / slots.
  StashDeposit,     // Inventory item -> stash.
  StashWithdraw,    // Stash item -> inventory.
  StashUnlockTab,
  StashSort,
  StashAutoDeposit,
  // Crafting / salvage.
  CraftAffixUpgrade,
  CraftChaos,
  CraftRefine,
  CraftAddAffix,
  CraftFuse,
  CraftSalvage,
  CraftBatchSalvage,
  // Character.
  ConfirmAttributeAllocation,
  // Skill / mastery / astrolabe (R8). These kinds cover every gameplay write
  // the skill UI can request: hotbar/specialized assignment, talent reset and
  // allocation, mastery selection, heavenly-sword attunement, the hub debug
  // unlock override and astrolabe node/vow mutations. Panel-local state
  // (selection, drag, hover, layout editing) stays in the controllers.
  SkillAssign,              // Assign a skill to a hotbar or specialized slot.
  SkillUnassign,            // Clear a specialized slot (drops its talents).
  SkillResetTalents,        // Reset all talent points of a specialized skill.
  SkillAllocateTalentPoint, // Spend one talent point on a tree node.
  SkillSelectMastery,       // Select / change the blade mastery.
  SkillSetAttunement,       // Set the heavenly-sword attunement element.
  SkillSetDebugUnlock,      // Toggle the hub debug unlock override.
  AstrolabeAddPoint,        // Spend one astrolabe point on a talent node.
  AstrolabeTakeVow,         // Take the irreversible profession vow.
  Count,
};

// Source location of the item the intent refers to (used for ownership /
// authority checks).
enum class GameUiItemSource : std::uint8_t {
  Inventory,
  Equipment,
  Bag,
  Stash,
  Ground,
  CraftingSlot,
};

// Bag operation payload discriminator.
enum class GameUiBagAction : std::uint8_t { Equip, Unequip };

// Which stash the intent targets.
enum class GameUiStashTarget : std::uint8_t { Personal, Shared };

// Skill container a SkillAssign/SkillUnassign targets.
enum class GameUiSkillTarget : std::uint8_t { Hotbar, Specialized };

// Flat tagged payload for all intent kinds. Only the fields relevant to the
// kind are meaningful; the rest stay at their defaults. All locations are
// indices (slot/tab) or stable integer domain ids — no pointers, no entities.
struct GameUiIntentPayload {
  // Item/entity references (stable domain ids; kInvalidDomainId = none).
  std::uint64_t sourceDomainId = 0;
  std::uint64_t targetDomainId = 0;
  std::uint64_t catalystDomainId = 0; // Legendary-fusion catalyst.

  // Location indices.
  std::int32_t sourceSlot = -1; // Inventory/bag/stash/equipment slot index.
  std::int32_t targetSlot = -1;
  std::int32_t sourceTab = -1;  // Stash tab index.
  std::int32_t targetTab = -1;
  std::uint8_t equipmentSlot = 0; // NoMoreDay::EquipmentSlot value (unequip).
  std::uint8_t socketIndex = 0;   // Socket slot for socket/unsocket.
  std::int32_t quantity = 1;      // Stack quantity for drop/destroy/use.

  // Crafting.
  std::int32_t affixIndex = -1; // Affix index for upgrade/chaos/refine.

  // Enum discriminators (underlying values, see the enums above).
  std::uint8_t itemSource = 0; // GameUiItemSource value.
  std::uint8_t stashTarget = 0; // GameUiStashTarget value.
  std::uint8_t bagAction = 0;   // GameUiBagAction value.
  std::uint8_t sortMode = 0;    // NoMoreDay::StashSortMode value.

  // Crafting add-affix request.
  std::uint16_t affixType = 0; // NoMoreDay::AffixType value.
  bool isPrefix = false;

  // Batch salvage filter (mirrors the crafting panel's SalvageFilter; the
  // filter evaluation is owned by SalvageSystem, not the UI).
  std::uint32_t salvageRarityMask = 0;
  bool keepIfTier6Plus = false;
  bool excludeLocked = true;

  // Attribute allocation (ConfirmAttributeAllocation).
  std::int32_t allocationStrength = 0;
  std::int32_t allocationDexterity = 0;
  std::int32_t allocationIntelligence = 0;
  std::int32_t allocationVitality = 0;

  // Skill / mastery / astrolabe (R8). skillId uses the game skill id
  // (0 = none, matching empty skill slots); sourceSlot is the hotbar or
  // specialized slot index for SkillAssign/SkillUnassign.
  std::uint32_t skillId = 0;
  std::uint8_t skillTarget = 0;     // GameUiSkillTarget value.
  std::uint8_t masteryId = 0;       // NoMoreDay::BladeMasteryId value.
  std::uint8_t attunementElement = 0; // NoMoreDay::BladeAttunement value.
  std::uint32_t astrolabeNodeId = 0;  // Astrolabe talent node id.
  std::uint8_t professionId = 0;    // NoMoreDay::ProfessionID value.

  // Generic flag (reserved for future kinds).
  bool flag = false;
};

struct GameUiIntent {
  UiId sourceNode = kInvalidUiId; // UI node that issued the intent.
  GameUiIntentKind kind = GameUiIntentKind::PickupItem;
  GameUiIntentPayload payload;
};

// Structured failure reason. Success is reported through `success`; `code`
// gives the panel a stable machine-readable reason for the failure.
enum class GameUiResultCode : std::uint8_t {
  Success,
  NoPlayer,
  InvalidTarget,
  MissingComponent,
  InvalidSlot,
  InvalidTab,
  InvalidIndex,
  CapacityFull,
  TooFarAway,
  NotOwned,
  Locked,
  NoPermission,
  DomainPrecondition, // Generic domain precondition failure.
  NotInInventory,
  NotEquipped,
  CraftingFailure,
  MaterialMissing,
};

// Outcome of executing an intent. Notifications carry a user-facing message
// on failure (or other notable results); success leaves it empty. For
// destructive successes (drop/destroy/socket/salvage/fuse/use-consumption)
// `clearedDomainIds` lists the domain ids that no longer exist so the UI
// session (drag/crafting/tooltip targets) can drop them.
struct GameUiResult {
  bool success = false;
  GameUiResultCode code = GameUiResultCode::Success;
  std::string notification;
  std::vector<std::uint64_t> clearedDomainIds;
};

} // namespace NoMoreDay::ui
