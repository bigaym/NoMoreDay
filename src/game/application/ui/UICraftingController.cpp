#include "game/application/ui/UICraftingController.hpp"

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/systems/item/MaterialRegistry.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace NoMoreDay::ui {

namespace {

// Runtime node id for the crafting panel root (hashed once at compile time).
inline constexpr UiId kUICraftingRootNode =
    static_cast<UiId>(entt::hashed_string("ui_crafting_panel").value());

// Panel layout (logical pixels; the viewport maps them to screen space).
inline constexpr float kPanelW = 600.0f;
inline constexpr float kPanelH = 700.0f;
inline constexpr float kHeaderHeight = 60.0f;
inline constexpr float kTabW = 120.0f;
inline constexpr float kTabH = 32.0f;
inline constexpr float kCloseSize = 28.0f;
inline constexpr float kSlotSize = 80.0f;
inline constexpr float kMergeSlotSize = 64.0f;
inline constexpr float kRowH = 50.0f;
inline constexpr float kBtnW = 60.0f;
inline constexpr float kBtnH = 30.0f;
inline constexpr int kMaxAffixSlots = 2; // 2 prefixes + 2 suffixes visible.

// Paint helpers (mirror the R6 inventory controller's anonymous namespace).
constexpr UiColor kWhiteTint{255, 255, 255, 255};
constexpr UiColor kBlackText{0, 0, 0, 255};

UiColor Faded(const UiColor& color, float alpha) {
  const auto scale = static_cast<std::uint8_t>(std::clamp(
      alpha, 0.0f, 1.0f) * static_cast<float>(color.a));
  return {color.r, color.g, color.b, scale};
}

UiColor ToUiColor(const Color& color) {
  return {color.r, color.g, color.b, color.a};
}

bool IsSalvageableView(const GameUiItemView& view) {
  if (view.rarity < static_cast<std::uint8_t>(Rarity::Magic)) {
    return false;
  }
  const auto type = static_cast<ItemType>(view.itemType);
  if (type != ItemType::Weapon && type != ItemType::Armor &&
      type != ItemType::Shield && type != ItemType::Jewelry) {
    return false;
  }
  return view.legendaryPotential <= 0;
}

} // namespace

UICraftingController::UICraftingController(UiRuntime& runtime, GameUiHost* uiHost)
    : m_runtime(runtime), m_uiHost(uiHost) {
  // The salvage filter defaults need the Rarity enumerators, which are only
  // complete in this translation unit; apply them up front so the migrated
  // session state is valid even before the first EnterGameplay.
  ResetSessionState();

  // R7: panel origin is instance state (dragging persists across frames).
  m_panelX = (UI_REF_WIDTH - kPanelW) * 0.5f;
  m_panelY = (UI_REF_HEIGHT - kPanelH) * 0.5f;

  UiNodeDesc desc;
  desc.id = kUICraftingRootNode;
  desc.parent = kRootUiId;
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.visible = true;
  desc.hitTestVisible = false;
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Panels);
  desc.customPainter = kInvalidUiResourceId;

  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
  }
}

UiId UICraftingController::NodeId() const noexcept {
  return m_rootNodeId;
}

bool UICraftingController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

void UICraftingController::EnterGameplay() {
  ResetSessionState();
  m_inGameplay = true;
  // The crafting panel starts closed on EnterGameplay, mirroring the legacy
  // UICrafting default state (the player opens it with K / context menu).
  SetNodeVisible(m_visible);
}

void UICraftingController::LeaveGameplay() {
  ResetSessionState();
  m_inGameplay = false;
  SetNodeVisible(false);
}

void UICraftingController::SetNodeVisible(bool visible) {
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

void UICraftingController::ResetSessionState() noexcept {
  // R7 (B-01): the session targets are stable integer domain ids
  // (kInvalidDomainId when empty); no entt::entity target is held across
  // operations.
  m_forgeTarget = kInvalidDomainId;
  m_mergeBase = kInvalidDomainId;
  m_mergeFodder = kInvalidDomainId;
  m_mergeCatalyst = kInvalidDomainId;
  m_salvageItem = kInvalidDomainId;
  m_selectedAffixIndex = -1;
  m_showSalvageFilter = false;
  m_currentTab = CraftingTab::Forging;
  m_craftingAlpha = 0.0f;
  m_visible = false;

  // Default salvage filter: magic + rare rarities, all item categories, keep
  // T6+ gear, exclude locked items (legacy SalvageFilter defaults).
  m_salvageFilter.rarityMask =
      (1u << static_cast<std::uint32_t>(Rarity::Magic)) |
      (1u << static_cast<std::uint32_t>(Rarity::Rare));
  m_salvageFilter.categoryMask = 0xFFFFFFFFu;
  m_salvageFilter.keepIfTier6Plus = true;
  m_salvageFilter.excludeLocked = true;
}

void UICraftingController::Toggle() {
  m_visible = !m_visible;
  SetNodeVisible(m_visible);
}

void UICraftingController::Close() {
  m_visible = false;
  SetNodeVisible(false);
}

bool UICraftingController::IsVisible() const noexcept {
  return m_visible;
}

UIDragSession& UICraftingController::DragSession() noexcept {
  // U8: single host-owned drag session across all panels; fall back to a local
  // session in headless tests where the host is absent.
  if (m_uiHost != nullptr) {
    return m_uiHost->DragSession();
  }
  return m_localDragSession;
}

void UICraftingController::OpenMergePanel() {
  m_visible = true;
  m_currentTab = CraftingTab::Merging;
  SetNodeVisible(true);
}

void UICraftingController::SetTargetItem(entt::entity item) {
  // R7: the entity is converted to a stable domain id at the boundary; the
  // controller never stores entt::entity targets (B-01).
  m_forgeTarget =
      item == entt::null ? kInvalidDomainId : entt::to_integral(item);
  m_visible = true; // Auto-open when setting target via context menu.
  SetNodeVisible(true);
}

std::uint64_t UICraftingController::GetForgeTargetDomainId() const noexcept {
  return m_forgeTarget;
}

std::uint64_t UICraftingController::GetMergeBaseDomainId() const noexcept {
  return m_mergeBase;
}

std::uint64_t UICraftingController::GetMergeFodderDomainId() const noexcept {
  return m_mergeFodder;
}

std::uint64_t UICraftingController::GetMergeCatalystDomainId() const noexcept {
  return m_mergeCatalyst;
}

std::uint64_t UICraftingController::GetSalvageItemDomainId() const noexcept {
  return m_salvageItem;
}

void UICraftingController::ClearTargetItem() {
  m_forgeTarget = kInvalidDomainId;
}

void UICraftingController::ClearConsumedTarget(std::uint64_t domainId) {
  if (domainId == kInvalidDomainId) {
    return;
  }
  if (m_forgeTarget == domainId) {
    m_forgeTarget = kInvalidDomainId;
  }
  if (m_mergeBase == domainId) {
    m_mergeBase = kInvalidDomainId;
  }
  if (m_mergeFodder == domainId) {
    m_mergeFodder = kInvalidDomainId;
    m_selectedAffixIndex = -1;
  }
  if (m_mergeCatalyst == domainId) {
    m_mergeCatalyst = kInvalidDomainId;
  }
  if (m_salvageItem == domainId) {
    m_salvageItem = kInvalidDomainId;
  }
}

const GameUiItemView* UICraftingController::FindDisplayedItem(
    const GameUiSnapshot& snapshot, std::uint64_t domainId) noexcept {
  if (domainId == kInvalidDomainId) {
    return nullptr;
  }
  for (const GameUiItemView& view : snapshot.displayedItems) {
    if (view.domainId == domainId) {
      return &view;
    }
  }
  return nullptr;
}

void UICraftingController::EnqueueIntent(GameUiIntent intent) {
  if (m_uiHost != nullptr) {
    m_uiHost->EnqueueIntent(std::move(intent));
  }
}

UICraftingController::Layout UICraftingController::ComputeLayout() const noexcept {
  Layout layout;
  layout.panelX = m_panelX;
  layout.panelY = m_panelY;
  layout.panelW = kPanelW;
  layout.panelH = kPanelH;
  layout.tabY = m_panelY + 20.0f;
  return layout;
}

void UICraftingController::Update(const GameUiSnapshot& snapshot,
                                  const UiInputFrame& input) {
  // Alpha animation toward the authoritative visibility flag.
  const float dt = std::max(input.deltaSeconds, 0.0f);
  const float alphaSpeed = 6.0f;
  if (m_visible) {
    m_craftingAlpha = std::min(1.0f, m_craftingAlpha + dt * alphaSpeed);
  } else {
    m_craftingAlpha = std::max(0.0f, m_craftingAlpha - dt * alphaSpeed);
  }

  if (!m_visible) {
    SetNodeVisible(false);
    return;
  }
  SetNodeVisible(true);
  if (!m_inGameplay) {
    return;
  }

  // R7: drop stale session targets whose item is no longer part of this
  // frame's displayed items (mirrors the legacy registry.valid() cleanup
  // without touching the ECS).
  const uint64_t targets[] = {m_forgeTarget,   m_mergeBase,
                              m_mergeFodder,   m_mergeCatalyst,
                              m_salvageItem};
  for (const uint64_t id : targets) {
    if (id != kInvalidDomainId && FindDisplayedItem(snapshot, id) == nullptr) {
      ClearConsumedTarget(id);
    }
  }

  const Layout layout = ComputeLayout();
  const UiVec2 mouse = input.pointer.logicalPosition;

  // Panel drag (same service as the stash panel).
  {
    NoMoreDay::UIPanelDragInputs dragInputs;
    dragInputs.mousePosition = Vector2{mouse.x, mouse.y};
    dragInputs.isMousePressed = input.pointer.pressed;
    dragInputs.isMouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    NoMoreDay::UIPanelDragBounds dragBounds;
    dragBounds.panelWidth = layout.panelW;
    dragBounds.panelHeight = layout.panelH;
    dragBounds.headerHeight = kHeaderHeight;
    dragBounds.uiRefWidth = static_cast<float>(UI_REF_WIDTH);
    dragBounds.uiRefHeight = static_cast<float>(UI_REF_HEIGHT);
    NoMoreDay::UIPanelDragService::UpdatePanelDrag(
        m_panelState, UIPanelID::Crafting, m_activeDragPanel, m_panelX,
        m_panelY, dragInputs, dragBounds);
  }

  // Modal surfaces (quantity popup, skill tree) capture pointer input.
  if (m_uiHost == nullptr || m_uiHost->IsModalInputCaptured()) {
    return;
  }
  const UiRect panelRect{{layout.panelX, layout.panelY},
                         {layout.panelW, layout.panelH}};
  if (panelRect.Contains(mouse)) {
    m_uiHost->SetMouseOverUI(true);
  }

  UIDragSession& drag = DragSession();

  // Close button.
  {
    const UiRect closeRect{{layout.panelX + layout.panelW - kCloseSize - 12.0f,
                            layout.panelY + 15.0f},
                           {kCloseSize, kCloseSize}};
    if (closeRect.Contains(mouse) && input.pointer.pressed) {
      Toggle();
      return;
    }
  }

  // Tab bar.
  static constexpr const char* kTabLabels[3] = {"词缀锻造", "传奇融合",
                                                "装备分解"};
  for (int i = 0; i < 3; ++i) {
    const UiRect tabRect{
        {layout.panelX + 20.0f + static_cast<float>(i) * (kTabW + 8.0f),
         layout.tabY},
        {kTabW, kTabH}};
    if (tabRect.Contains(mouse) && input.pointer.pressed) {
      m_currentTab = static_cast<CraftingTab>(i);
    }
  }

  if (m_currentTab == CraftingTab::Forging) {
    UpdateForgingTab(snapshot, input, layout, drag);
  } else if (m_currentTab == CraftingTab::Merging) {
    UpdateMergingTab(snapshot, input, layout, drag);
  } else {
    UpdateSalvagingTab(snapshot, input, layout, drag);
  }
}

namespace {

// Rect for the given column/row in the (bounded) affix slot layout.
UiRect AffixRowRect(float x, float y, float w) {
  return {{x, y}, {w, kRowH}};
}

} // namespace

void UICraftingController::UpdateForgingTab(const GameUiSnapshot& snapshot,
                                            const UiInputFrame& input,
                                            const Layout& layout,
                                            UIDragSession& drag) {
  const UiVec2 mouse = input.pointer.logicalPosition;
  const float slotX = layout.panelX + (layout.panelW - kSlotSize) * 0.5f;
  const float slotY = layout.panelY + 80.0f;
  const UiRect slotRect{{slotX, slotY}, {kSlotSize, kSlotSize}};

  // Forge slot drop: only Weapon/Armor/Jewelry/Shield items.
  if (slotRect.Contains(mouse) && input.pointer.released &&
      drag.draggedItemDomainId != kInvalidDomainId) {
    const GameUiItemView* view =
        FindDisplayedItem(snapshot, drag.draggedItemDomainId);
    if (view != nullptr) {
      const auto type = static_cast<ItemType>(view->itemType);
      if (type == ItemType::Weapon || type == ItemType::Armor ||
          type == ItemType::Jewelry || type == ItemType::Shield) {
        m_forgeTarget = view->domainId;
      }
    }
    drag.Clear();
    return;
  }

  const GameUiItemView* forge = FindDisplayedItem(snapshot, m_forgeTarget);
  if (forge == nullptr) {
    return;
  }

  // Affix rows: prefix then suffix (bounded 2+2 slots; no per-frame vector).
  const float listX = layout.panelX + 20.0f;
  const float listW = layout.panelW - 40.0f;
  const float rowStartY = layout.panelY + 220.0f;
  const float btnX = listX + listW - 70.0f;
  const float addBtnX = listX + listW - 90.0f;

  int row = 0;
  auto handleRow = [&](const GameUiAffixView* affix, bool isPrefix, int affixIndex) {
    const float y = rowStartY + static_cast<float>(row) * (kRowH + 6.0f);
    const bool hovered = AffixRowRect(listX, y, listW).Contains(mouse);
    const bool canAfford = forge->forgingPotential > 0;
    const bool canUpgrade =
        affix != nullptr && affix->tier < 5 && canAfford;

    if (affix != nullptr) {
      if (hovered && input.pointer.pressed && !canUpgrade) {
        // Click on a max-tier affix is a no-op (legacy behavior).
      }
      if (canUpgrade && input.pointer.pressed) {
        if (const UiRect r{btnX, y, {kBtnW, kBtnH}}; r.Contains(mouse)) {
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::CraftAffixUpgrade;
          intent.payload.targetDomainId = m_forgeTarget;
          intent.payload.affixIndex = affixIndex;
          EnqueueIntent(std::move(intent));
        } else if (const UiRect r{btnX - 35.0f, y, {30.0f, kBtnH}};
                   r.Contains(mouse)) {
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::CraftChaos;
          intent.payload.targetDomainId = m_forgeTarget;
          intent.payload.affixIndex = affixIndex;
          EnqueueIntent(std::move(intent));
        } else if (const UiRect r{btnX - 70.0f, y, {30.0f, kBtnH}};
                   r.Contains(mouse)) {
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::CraftRefine;
          intent.payload.targetDomainId = m_forgeTarget;
          intent.payload.affixIndex = affixIndex;
          EnqueueIntent(std::move(intent));
        }
      }
    } else {
      // Empty affix slot: add-affix button.
      if (canAfford && input.pointer.pressed) {
        if (const UiRect r{addBtnX, y, {80.0f, kBtnH}}; r.Contains(mouse)) {
          static constexpr AffixType kAddableTypes[6] = {
              AffixType::Strength,       AffixType::Dexterity,
              AffixType::Intelligence,   AffixType::Vitality,
              AffixType::FlatPhysicalDamage, AffixType::AttackSpeed};
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::CraftAddAffix;
          intent.payload.targetDomainId = m_forgeTarget;
          intent.payload.affixType =
              static_cast<std::uint16_t>(kAddableTypes[GetRandomValue(0, 5)]);
          intent.payload.isPrefix = isPrefix;
          EnqueueIntent(std::move(intent));
        }
      }
    }
    ++row;
  };

  const GameUiAffixView* prefixes[kMaxAffixSlots] = {nullptr, nullptr};
  const GameUiAffixView* suffixes[kMaxAffixSlots] = {nullptr, nullptr};
  int prefixCount = 0;
  int suffixCount = 0;
  for (const GameUiAffixView& affix : forge->affixes) {
    if (affix.isPrefix && prefixCount < kMaxAffixSlots) {
      prefixes[prefixCount++] = &affix;
    } else if (!affix.isPrefix && suffixCount < kMaxAffixSlots) {
      suffixes[suffixCount++] = &affix;
    }
  }

  for (int i = 0; i < kMaxAffixSlots; ++i) {
    handleRow(prefixes[i], true, i < prefixCount ? i : -1);
  }
  for (int i = 0; i < kMaxAffixSlots; ++i) {
    handleRow(suffixes[i], false, i < suffixCount ? i : -1);
  }
}

void UICraftingController::UpdateMergingTab(const GameUiSnapshot& snapshot,
                                            const UiInputFrame& input,
                                            const Layout& layout,
                                            UIDragSession& drag) {
  const UiVec2 mouse = input.pointer.logicalPosition;
  const float midX = layout.panelX + layout.panelW * 0.5f;
  const float topY = layout.panelY + 100.0f;
  const UiRect baseRect{{midX - 84.0f, topY}, {kMergeSlotSize, kMergeSlotSize}};
  const UiRect fodderRect{{midX + 20.0f, topY},
                          {kMergeSlotSize, kMergeSlotSize}};
  const UiRect catalystRect{{midX - 32.0f, topY + 168.0f},
                            {kMergeSlotSize, kMergeSlotSize}};

  if (input.pointer.released && drag.draggedItemDomainId != kInvalidDomainId) {
    const GameUiItemView* view =
        FindDisplayedItem(snapshot, drag.draggedItemDomainId);
    if (view != nullptr) {
      if (baseRect.Contains(mouse) && view->legendaryPotential > 0) {
        m_mergeBase = view->domainId;
      } else if (fodderRect.Contains(mouse)) {
        for (const GameUiAffixView& affix : view->affixes) {
          if (affix.tier >= 6) {
            m_mergeFodder = view->domainId;
            m_selectedAffixIndex = -1;
            break;
          }
        }
      } else if (catalystRect.Contains(mouse)) {
        const auto type = static_cast<ItemType>(view->itemType);
        if (type == ItemType::Material || type == ItemType::Consumable) {
          m_mergeCatalyst = view->domainId;
        }
      }
    }
    drag.Clear();
    return;
  }

  // Affix selection from the fodder item.
  const GameUiItemView* fodder = FindDisplayedItem(snapshot, m_mergeFodder);
  if (fodder == nullptr) {
    return;
  }
  const float affixY = topY + 252.0f;
  const float rowW = layout.panelW - 80.0f;
  for (std::size_t i = 0; i < fodder->affixes.size(); ++i) {
    const float y = affixY + 30.0f + static_cast<float>(i) * 45.0f;
    const UiRect row{{layout.panelX + 40.0f, y}, {rowW, 40.0f}};
    if (row.Contains(mouse) && input.pointer.pressed) {
      m_selectedAffixIndex = static_cast<int>(i);
    }
  }

  // Fuse button.
  const UiRect fuseBtn{{midX - 80.0f, layout.panelY + 620.0f}, {160.0f, 50.0f}};
  if (fuseBtn.Contains(mouse) && input.pointer.pressed && m_mergeBase != kInvalidDomainId &&
      m_mergeFodder != kInvalidDomainId && m_mergeCatalyst != kInvalidDomainId &&
      m_selectedAffixIndex >= 0) {
    GameUiIntent intent;
    intent.sourceNode = m_rootNodeId;
    intent.kind = GameUiIntentKind::CraftFuse;
    intent.payload.sourceDomainId = m_mergeBase;
    intent.payload.targetDomainId = m_mergeFodder;
    intent.payload.catalystDomainId = m_mergeCatalyst;
    intent.payload.affixIndex = m_selectedAffixIndex;
    EnqueueIntent(std::move(intent));
  }
}

void UICraftingController::UpdateSalvagingTab(const GameUiSnapshot& snapshot,
                                              const UiInputFrame& input,
                                              const Layout& layout,
                                              UIDragSession& drag) {
  const UiVec2 mouse = input.pointer.logicalPosition;
  const float midX = layout.panelX + layout.panelW * 0.5f;
  const float slotY = layout.panelY + 150.0f;
  const UiRect slotRect{{midX - 40.0f, slotY}, {kSlotSize, kSlotSize}};

  // Salvage slot drop: CanSalvage-equivalent on the view model.
  if (slotRect.Contains(mouse) && input.pointer.released &&
      drag.draggedItemDomainId != kInvalidDomainId) {
    const GameUiItemView* view =
        FindDisplayedItem(snapshot, drag.draggedItemDomainId);
    if (view != nullptr && IsSalvageableView(*view)) {
      m_salvageItem = view->domainId;
    }
    drag.Clear();
    return;
  }

  // Salvage button.
  const UiRect salvageBtn{{midX - 100.0f, layout.panelY + 580.0f},
                          {200.0f, 60.0f}};
  if (salvageBtn.Contains(mouse) && input.pointer.pressed &&
      m_salvageItem != kInvalidDomainId) {
    GameUiIntent intent;
    intent.sourceNode = m_rootNodeId;
    intent.kind = GameUiIntentKind::CraftSalvage;
    intent.payload.targetDomainId = m_salvageItem;
    EnqueueIntent(std::move(intent));
    return;
  }

  // Filter toggle + popup.
  const UiRect filterBtn{{layout.panelX + 20.0f, layout.panelY + 620.0f},
                         {100.0f, 32.0f}};
  if (filterBtn.Contains(mouse) && input.pointer.pressed) {
    m_showSalvageFilter = !m_showSalvageFilter;
    return;
  }
  if (m_showSalvageFilter) {
    const float fx = layout.panelX - 220.0f;
    const float fy = layout.panelY + 100.0f;
    const UiRect popup{{fx, fy}, {200.0f, 300.0f}};
    if (popup.Contains(mouse) && input.pointer.pressed) {
      auto toggle = [&](const UiRect& r, bool& flag) {
        if (r.Contains(mouse)) {
          flag = !flag;
          return true;
        }
        return false;
      };
      bool handled = false;
      handled |= toggle({{fx + 10.0f, fy + 40.0f}, {180.0f, 24.0f}},
                        m_salvageFilter.excludeLocked);
      handled |= toggle({{fx + 10.0f, fy + 70.0f}, {180.0f, 24.0f}},
                        m_salvageFilter.keepIfTier6Plus);
      if (!handled) {
        auto toggleRarity = [&](const UiRect& r, Rarity rarity) {
          if (r.Contains(mouse)) {
            const auto bit = 1u << static_cast<std::uint32_t>(rarity);
            m_salvageFilter.rarityMask ^= bit;
            return true;
          }
          return false;
        };
        handled |= toggleRarity({{fx + 10.0f, fy + 140.0f}, {180.0f, 24.0f}},
                                Rarity::Magic);
        handled |= toggleRarity({{fx + 10.0f, fy + 170.0f}, {180.0f, 24.0f}},
                                Rarity::Rare);
        handled |= toggleRarity({{fx + 10.0f, fy + 200.0f}, {180.0f, 24.0f}},
                                Rarity::Epic);
      }
      (void)handled;
      return;
    }
  }

  // Batch salvage button.
  const UiRect batchBtn{{midX - 100.0f, layout.panelY + 620.0f}, {200.0f, 32.0f}};
  if (batchBtn.Contains(mouse) && input.pointer.pressed) {
    GameUiIntent intent;
    intent.sourceNode = m_rootNodeId;
    intent.kind = GameUiIntentKind::CraftBatchSalvage;
    intent.payload.salvageRarityMask = m_salvageFilter.rarityMask;
    intent.payload.keepIfTier6Plus = m_salvageFilter.keepIfTier6Plus;
    intent.payload.excludeLocked = m_salvageFilter.excludeLocked;
    EnqueueIntent(std::move(intent));
  }
}

void UICraftingController::Paint(UiDrawList& drawList, const UiViewport& viewport,
                                 const GameUiSnapshot& snapshot) const {
  (void)viewport;
  if (!m_visible || !m_inGameplay || m_craftingAlpha <= 0.001f ||
      m_rootNodeId == kInvalidUiId) {
    return;
  }

  const Layout layout = ComputeLayout();
  const float alpha = m_craftingAlpha;
  const UiDrawLayer layer = UiDrawLayer::Panels;
  const UiId node = m_rootNodeId;
  const UITheme& theme = UIRenderer::GetTheme();
  const UiColor panelBg = ToUiColor(theme.panelBackground);
  const UiColor panelBorder = ToUiColor(theme.panelBorder);
  const UiColor textPrimary = ToUiColor(theme.textPrimary);
  const UiColor textSecondary = ToUiColor(theme.textSecondary);
  const UiColor textHighlight = ToUiColor(theme.textHighlight);
  const UiColor slotBg = ToUiColor(theme.slotBackground);
  const UiColor btnNormal = ToUiColor(theme.buttonNormal);
  const UiColor btnHover = ToUiColor(theme.buttonHover);

  // Panel frame.
  drawList.FillRect(layer, node, {{layout.panelX, layout.panelY},
                                  {layout.panelW, layout.panelH}},
                    Faded(panelBg, alpha));
  drawList.StrokeRect(layer, node, {{layout.panelX, layout.panelY},
                                    {layout.panelW, layout.panelH}},
                      Faded(panelBorder, alpha), 1.0f);

  // Title.
  drawList.Text(layer, node, "合成锻造",
                {layout.panelX + 20.0f, layout.panelY + 22.0f}, 26.0f,
                Faded(textHighlight, alpha), kGlobalFontResourceId);

  // Close button.
  const UiRect closeRect{{layout.panelX + layout.panelW - kCloseSize - 12.0f,
                          layout.panelY + 15.0f},
                         {kCloseSize, kCloseSize}};
  drawList.FillRect(layer, node, closeRect, Faded(btnNormal, alpha));
  drawList.StrokeRect(layer, node, closeRect, Faded(panelBorder, alpha), 1.0f);
  drawList.Text(layer, node, "X",
                {closeRect.origin.x + 8.0f, closeRect.origin.y + 5.0f}, 18.0f,
                Faded(textPrimary, alpha), kGlobalFontResourceId);

  // Tab bar.
  static constexpr const char* kTabLabels[3] = {"词缀锻造", "传奇融合",
                                                "装备分解"};
  for (int i = 0; i < 3; ++i) {
    const UiRect tabRect{
        {layout.panelX + 20.0f + static_cast<float>(i) * (kTabW + 8.0f),
         layout.tabY},
        {kTabW, kTabH}};
    const bool active = m_currentTab == static_cast<CraftingTab>(i);
    // Active tab: gold tint (legacy theme accent).
    const UiColor tabColor = active ? UiColor{230, 191, 38, 255} : btnNormal;
    drawList.FillRect(layer, node, tabRect, Faded(tabColor, alpha));
    drawList.StrokeRect(layer, node, tabRect, Faded(panelBorder, alpha), 1.0f);
    drawList.Text(layer, node, kTabLabels[i],
                  {tabRect.origin.x + 20.0f, tabRect.origin.y + 8.0f}, 18.0f,
                  Faded(active ? kBlackText : textPrimary, alpha),
                  kGlobalFontResourceId);
  }

  if (m_currentTab == CraftingTab::Forging) {
    PaintForgingTab(drawList, snapshot, layout, alpha);
  } else if (m_currentTab == CraftingTab::Merging) {
    PaintMergingTab(drawList, snapshot, layout, alpha);
  } else {
    PaintSalvagingTab(drawList, snapshot, layout, alpha);
  }
}

namespace {

UiRect SlotPaintRect(float x, float y, float size) {
  return {{x, y}, {size, size}};
}

} // namespace

void UICraftingController::PaintForgingTab(UiDrawList& drawList,
                                           const GameUiSnapshot& snapshot,
                                           const Layout& layout,
                                           float alpha) const {
  const UiDrawLayer layer = UiDrawLayer::Panels;
  const UiId node = m_rootNodeId;
  const UITheme& theme = UIRenderer::GetTheme();
  const UiColor slotBg = ToUiColor(theme.slotBackground);
  const UiColor panelBorder = ToUiColor(theme.panelBorder);
  const UiColor textPrimary = ToUiColor(theme.textPrimary);
  const UiColor textSecondary = ToUiColor(theme.textSecondary);
  const UiColor btnNormal = ToUiColor(theme.buttonNormal);

  const float slotX = layout.panelX + (layout.panelW - kSlotSize) * 0.5f;
  const float slotY = layout.panelY + 80.0f;
  const UiRect slotRect = SlotPaintRect(slotX, slotY, kSlotSize);
  drawList.FillRect(layer, node, slotRect, Faded(slotBg, alpha));
  drawList.StrokeRect(layer, node, slotRect, Faded(panelBorder, alpha), 1.0f);

  const GameUiItemView* forge = FindDisplayedItem(snapshot, m_forgeTarget);
  if (forge == nullptr) {
    drawList.Text(layer, node, "放入装备",
                  {slotX + 14.0f, slotY + 30.0f}, 16.0f,
                  Faded(textSecondary, alpha), kGlobalFontResourceId);
    drawList.Text(layer, node,
                  "将装备拖入上方槽位开始锻造（升级、粉碎、重置词缀）",
                  {layout.panelX + 40.0f, layout.panelY + 180.0f}, 16.0f,
                  Faded(textSecondary, alpha), kGlobalFontResourceId);
    return;
  }

  // Item icon + forging potential.
  if (forge->textureId != 0) {
    drawList.Image(layer, node, SlotPaintRect(slotX + 12.0f, slotY + 12.0f,
                                              kSlotSize - 24.0f),
                   forge->textureId, Faded(kWhiteTint, alpha));
  }
  char potential[64];
  std::snprintf(potential, sizeof(potential), "锻造潜力: %d",
                forge->forgingPotential);
  drawList.Text(layer, node, potential,
                {layout.panelX + (layout.panelW - 120.0f) * 0.5f,
                 layout.panelY + 170.0f},
                20.0f, Faded(ToUiColor(SKYBLUE), alpha), kGlobalFontResourceId);

  // Affix rows.
  const float listX = layout.panelX + 20.0f;
  const float listW = layout.panelW - 40.0f;
  const float rowStartY = layout.panelY + 220.0f;
  const float btnX = listX + listW - 70.0f;
  const float addBtnX = listX + listW - 90.0f;

  int row = 0;
  auto paintRow = [&](const GameUiAffixView* affix, bool isPrefix) {
    const float y = rowStartY + static_cast<float>(row) * (kRowH + 6.0f);
    const UiRect rowRect = AffixRowRect(listX, y, listW);
    drawList.FillRect(layer, node, rowRect,
                      Faded(UiColor{40, 40, 45, 255}, alpha * 0.5f));
    drawList.StrokeRect(layer, node, rowRect,
                        Faded(UiColor{128, 128, 128, 255}, alpha), 1.0f);

    if (affix != nullptr) {
      // "T{tier} - {description}"
      char buf[128];
      const Affix tmpAffix{
          static_cast<AffixType>(affix->type), affix->value, affix->tier,
          affix->isPrefix, Tag::None, {}, affix->isLegendary};
      const char* desc = GetAffixDescriptionRef(tmpAffix, false);
      std::snprintf(buf, sizeof(buf), "T%d - %s", affix->tier, desc);
      drawList.Text(layer, node, buf, {listX + 10.0f, y + 14.0f}, 18.0f,
                    Faded(textPrimary, alpha), kGlobalFontResourceId);
      if (affix->tier >= 5) {
        drawList.Text(layer, node, "MAX", {btnX + 14.0f, y + 12.0f}, 16.0f,
                      Faded(ToUiColor(GOLD), alpha), kGlobalFontResourceId);
      } else if (forge->forgingPotential > 0) {
        // 升级 / C / R buttons.
        const UiRect upBtn{btnX, y, {kBtnW, kBtnH}};
        drawList.FillRect(layer, node, upBtn, Faded(btnNormal, alpha));
        drawList.StrokeRect(layer, node, upBtn,
                            Faded(panelBorder, alpha), 1.0f);
        drawList.Text(layer, node, "升级", {btnX + 14.0f, y + 6.0f}, 16.0f,
                      Faded(textPrimary, alpha), kGlobalFontResourceId);
        const UiRect cBtn{btnX - 35.0f, y, {30.0f, kBtnH}};
        drawList.FillRect(layer, node, cBtn, Faded(btnNormal, alpha));
        drawList.StrokeRect(layer, node, cBtn, Faded(panelBorder, alpha), 1.0f);
        drawList.Text(layer, node, "C", {cBtn.origin.x + 8.0f, y + 6.0f},
                      16.0f, Faded(textPrimary, alpha), kGlobalFontResourceId);
        const UiRect rBtn{btnX - 70.0f, y, {30.0f, kBtnH}};
        drawList.FillRect(layer, node, rBtn, Faded(btnNormal, alpha));
        drawList.StrokeRect(layer, node, rBtn, Faded(panelBorder, alpha), 1.0f);
        drawList.Text(layer, node, "R", {rBtn.origin.x + 8.0f, y + 6.0f},
                      16.0f, Faded(textPrimary, alpha), kGlobalFontResourceId);
      }
    } else {
      drawList.Text(layer, node, isPrefix ? "空前缀槽位" : "空后缀槽位",
                    {listX + 10.0f, y + 14.0f}, 18.0f,
                    Faded(textSecondary, alpha), kGlobalFontResourceId);
      if (forge->forgingPotential > 0) {
        const UiRect addBtn{addBtnX, y, {80.0f, kBtnH}};
        drawList.FillRect(layer, node, addBtn, Faded(btnNormal, alpha));
        drawList.StrokeRect(layer, node, addBtn, Faded(panelBorder, alpha),
                            1.0f);
        drawList.Text(layer, node, "添加", {addBtnX + 22.0f, y + 6.0f}, 16.0f,
                      Faded(textPrimary, alpha), kGlobalFontResourceId);
      }
    }
    ++row;
  };

  const GameUiAffixView* prefixes[kMaxAffixSlots] = {nullptr, nullptr};
  const GameUiAffixView* suffixes[kMaxAffixSlots] = {nullptr, nullptr};
  int prefixCount = 0;
  int suffixCount = 0;
  for (const GameUiAffixView& affix : forge->affixes) {
    if (affix.isPrefix && prefixCount < kMaxAffixSlots) {
      prefixes[prefixCount++] = &affix;
    } else if (!affix.isPrefix && suffixCount < kMaxAffixSlots) {
      suffixes[suffixCount++] = &affix;
    }
  }

  for (int i = 0; i < kMaxAffixSlots; ++i) {
    paintRow(prefixes[i], true);
  }
  for (int i = 0; i < kMaxAffixSlots; ++i) {
    paintRow(suffixes[i], false);
  }
}

void UICraftingController::PaintMergingTab(UiDrawList& drawList,
                                           const GameUiSnapshot& snapshot,
                                           const Layout& layout,
                                           float alpha) const {
  const UiDrawLayer layer = UiDrawLayer::Panels;
  const UiId node = m_rootNodeId;
  const UITheme& theme = UIRenderer::GetTheme();
  const UiColor slotBg = ToUiColor(theme.slotBackground);
  const UiColor panelBorder = ToUiColor(theme.panelBorder);
  const UiColor textPrimary = ToUiColor(theme.textPrimary);
  const UiColor textSecondary = ToUiColor(theme.textSecondary);

  const float midX = layout.panelX + layout.panelW * 0.5f;
  const float topY = layout.panelY + 100.0f;

  auto paintSlot = [&](float x, float y, uint64_t domainId,
                       const char* label, const char* title) {
    const UiRect rect = SlotPaintRect(x, y, kMergeSlotSize);
    drawList.FillRect(layer, node, rect, Faded(slotBg, alpha));
    drawList.StrokeRect(layer, node, rect, Faded(panelBorder, alpha), 1.0f);
    drawList.Text(layer, node, title, {x, y - 22.0f}, 18.0f,
                  Faded(textPrimary, alpha), kGlobalFontResourceId);
    if (domainId == kInvalidDomainId) {
      drawList.Text(layer, node, label, {x + 6.0f, y + 22.0f}, 14.0f,
                    Faded(textSecondary, alpha), kGlobalFontResourceId);
    } else {
      const GameUiItemView* view = FindDisplayedItem(snapshot, domainId);
      if (view != nullptr && view->textureId != 0) {
        drawList.Image(layer, node, SlotPaintRect(x + 8.0f, y + 8.0f,
                                                  kMergeSlotSize - 16.0f),
                       view->textureId, Faded(kWhiteTint, alpha));
      }
    }
  };

  paintSlot(midX - 84.0f, topY, m_mergeBase, "放入暗金(LP > 0)", "暗金基底");
  paintSlot(midX + 20.0f, topY, m_mergeFodder, "放入崇高(T6+)", "崇高物品");
  paintSlot(midX - 32.0f, topY + 168.0f, m_mergeCatalyst, "放入时空核心",
            "传奇核心");

  // Affix selection list.
  const GameUiItemView* fodder = FindDisplayedItem(snapshot, m_mergeFodder);
  if (fodder != nullptr) {
    drawList.Text(layer, node, "选择要转移并保留的词缀:",
                  {layout.panelX + 40.0f, topY + 252.0f}, 18.0f,
                  Faded(textPrimary, alpha), kGlobalFontResourceId);
    const float rowW = layout.panelW - 80.0f;
    for (std::size_t i = 0; i < fodder->affixes.size(); ++i) {
      const GameUiAffixView& affix = fodder->affixes[i];
      const float y = topY + 282.0f + static_cast<float>(i) * 45.0f;
      const UiRect row{{layout.panelX + 40.0f, y}, {rowW, 40.0f}};
      const bool selected = static_cast<int>(i) == m_selectedAffixIndex;
      drawList.FillRect(layer, node, row,
                        selected ? Faded(UiColor{230, 60, 60, 255}, alpha * 0.3f)
                                 : Faded(UiColor{40, 40, 45, 255}, alpha * 0.5f));
      drawList.StrokeRect(layer, node, row,
                          Faded(selected ? UiColor{230, 60, 60, 255}
                                         : UiColor{128, 128, 128, 255},
                                alpha),
                          1.0f);
      char buf[128];
      const Affix tmpAffix{
          static_cast<AffixType>(affix.type), affix.value, affix.tier,
          affix.isPrefix, Tag::None, {}, affix.isLegendary};
      const char* desc = GetAffixDescriptionRef(tmpAffix, true);
      std::snprintf(buf, sizeof(buf), "%s", desc);
      drawList.Text(layer, node, buf, {row.origin.x + 10.0f, y + 10.0f}, 18.0f,
                    Faded(ToUiColor(GetAffixTierColor(affix.tier)), alpha),
                    kGlobalFontResourceId);
    }
  }

  // Fuse button.
  const UiRect fuseBtn{{midX - 80.0f, layout.panelY + 620.0f}, {160.0f, 50.0f}};
  const bool canFuse = m_mergeBase != kInvalidDomainId &&
                       m_mergeFodder != kInvalidDomainId &&
                       m_mergeCatalyst != kInvalidDomainId &&
                       m_selectedAffixIndex >= 0;
  drawList.FillRect(layer, node, fuseBtn,
                    Faded(canFuse ? ToUiColor({200, 40, 40, 255})
                                  : ToUiColor(theme.buttonNormal),
                          alpha));
  drawList.StrokeRect(layer, node, fuseBtn, Faded(panelBorder, alpha), 1.0f);
  drawList.Text(layer, node, "开始融合", {fuseBtn.origin.x + 40.0f,
                                          fuseBtn.origin.y + 14.0f},
                24.0f,
                Faded(canFuse ? kWhiteTint : textSecondary, alpha),
                kGlobalFontResourceId);
}

void UICraftingController::PaintSalvagingTab(UiDrawList& drawList,
                                             const GameUiSnapshot& snapshot,
                                             const Layout& layout,
                                             float alpha) const {
  const UiDrawLayer layer = UiDrawLayer::Panels;
  const UiId node = m_rootNodeId;
  const UITheme& theme = UIRenderer::GetTheme();
  const UiColor slotBg = ToUiColor(theme.slotBackground);
  const UiColor panelBorder = ToUiColor(theme.panelBorder);
  const UiColor textPrimary = ToUiColor(theme.textPrimary);
  const UiColor textSecondary = ToUiColor(theme.textSecondary);
  const UiColor btnNormal = ToUiColor(theme.buttonNormal);

  const float midX = layout.panelX + layout.panelW * 0.5f;
  const float slotY = layout.panelY + 150.0f;
  const UiRect slotRect = SlotPaintRect(midX - 40.0f, slotY, kSlotSize);
  drawList.FillRect(layer, node, slotRect, Faded(slotBg, alpha));
  drawList.StrokeRect(layer, node, slotRect, Faded(panelBorder, alpha), 1.0f);

  const GameUiItemView* salvage = FindDisplayedItem(snapshot, m_salvageItem);
  if (salvage == nullptr) {
    drawList.Text(layer, node, "放入分解物品",
                  {midX - 48.0f, slotY + 30.0f}, 16.0f,
                  Faded(textSecondary, alpha), kGlobalFontResourceId);
  } else if (salvage->textureId != 0) {
    drawList.Image(layer, node, SlotPaintRect(midX - 28.0f, slotY + 12.0f,
                                              kSlotSize - 24.0f),
                   salvage->textureId, Faded(kWhiteTint, alpha));
  }

  // Yield preview (builder-computed snapshot data; no per-frame vectors).
  if (salvage != nullptr && !snapshot.crafting.salvageYield.empty()) {
    drawList.Text(layer, node, "分解产出预估:",
                  {midX - 56.0f, slotY + 140.0f}, 20.0f,
                  Faded(ToUiColor(SKYBLUE), alpha), kGlobalFontResourceId);
    const float startX = midX -
                         static_cast<float>(snapshot.crafting.salvageYield.size()) *
                             31.5f;
    for (std::size_t i = 0; i < snapshot.crafting.salvageYield.size(); ++i) {
      const GameUiSalvageYieldView& yield = snapshot.crafting.salvageYield[i];
      const float x = startX + static_cast<float>(i) * 63.0f;
      const float y = slotY + 180.0f;
      const UiRect matRect = SlotPaintRect(x, y, 48.0f);
      drawList.FillRect(layer, node, matRect, Faded(slotBg, alpha));
      drawList.StrokeRect(layer, node, matRect, Faded(panelBorder, alpha), 1.0f);
      char range[32];
      std::snprintf(range, sizeof(range), "%d~%d", yield.min, yield.max);
      drawList.Text(layer, node, range, {x + 6.0f, y + 34.0f}, 12.0f,
                    Faded(ToUiColor(SKYBLUE), alpha), kGlobalFontResourceId);
    }
  }

  // Salvage button.
  const UiRect salvageBtn{{midX - 100.0f, layout.panelY + 580.0f},
                          {200.0f, 60.0f}};
  const bool canSalvage = m_salvageItem != kInvalidDomainId;
  drawList.FillRect(layer, node, salvageBtn,
                    Faded(canSalvage ? ToUiColor({200, 40, 40, 255})
                                     : ToUiColor(theme.buttonNormal),
                          alpha));
  drawList.StrokeRect(layer, node, salvageBtn, Faded(panelBorder, alpha), 1.0f);
  drawList.Text(layer, node, "开始分解装备",
                {salvageBtn.origin.x + 40.0f, salvageBtn.origin.y + 18.0f},
                24.0f,
                Faded(canSalvage ? kWhiteTint : textSecondary, alpha),
                kGlobalFontResourceId);

  // Filter toggle + popup.
  const UiRect filterBtn{{layout.panelX + 20.0f, layout.panelY + 620.0f},
                         {100.0f, 32.0f}};
  drawList.FillRect(layer, node, filterBtn,
                    Faded(m_showSalvageFilter ? UiColor{200, 40, 40, 255}
                                              : ToUiColor(theme.buttonNormal),
                          alpha));
  drawList.StrokeRect(layer, node, filterBtn, Faded(panelBorder, alpha), 1.0f);
  drawList.Text(layer, node, "筛选设置", {filterBtn.origin.x + 16.0f,
                                          filterBtn.origin.y + 7.0f},
                16.0f, Faded(textPrimary, alpha), kGlobalFontResourceId);

  if (m_showSalvageFilter) {
    const float fx = layout.panelX - 220.0f;
    const float fy = layout.panelY + 100.0f;
    drawList.FillRect(layer, node, {{fx, fy}, {200.0f, 300.0f}},
                      Faded(UiColor{40, 40, 50, 255}, alpha * 0.9f));
    drawList.StrokeRect(layer, node, {{fx, fy}, {200.0f, 300.0f}},
                        Faded(ToUiColor(GOLD), alpha), 1.0f);
    drawList.Text(layer, node, "分解过滤器", {fx + 55.0f, fy + 8.0f}, 18.0f,
                  Faded(ToUiColor(GOLD), alpha), kGlobalFontResourceId);

    auto paintOption = [&](const char* label, bool value, float yOff) {
      const UiRect r{{fx + 10.0f, fy + yOff}, {180.0f, 24.0f}};
      drawList.FillRect(layer, node, r,
                        Faded(value ? UiColor{200, 40, 40, 255}
                                    : ToUiColor(theme.buttonNormal),
                              alpha));
      drawList.StrokeRect(layer, node, r, Faded(panelBorder, alpha), 1.0f);
      drawList.Text(layer, node, label, {r.origin.x + 8.0f, r.origin.y + 4.0f},
                    14.0f, Faded(textPrimary, alpha), kGlobalFontResourceId);
    };
    paintOption("排除已锁定", m_salvageFilter.excludeLocked, 40.0f);
    paintOption("保留 T6+ 装备", m_salvageFilter.keepIfTier6Plus, 70.0f);
    drawList.Text(layer, node, "稀有度限制:", {fx + 10.0f, fy + 110.0f}, 14.0f,
                  Faded(textSecondary, alpha), kGlobalFontResourceId);
    auto paintRarity = [&](const char* label, Rarity rarity, float yOff) {
      const bool on = (m_salvageFilter.rarityMask &
                       (1u << static_cast<std::uint32_t>(rarity))) != 0;
      const UiRect r{{fx + 10.0f, fy + yOff}, {180.0f, 24.0f}};
      drawList.FillRect(layer, node, r,
                        Faded(on ? ToUiColor(UIRenderer::GetRarityColor(rarity))
                                 : ToUiColor(theme.buttonNormal),
                              alpha));
      drawList.StrokeRect(layer, node, r, Faded(panelBorder, alpha), 1.0f);
      drawList.Text(layer, node, label, {r.origin.x + 8.0f, r.origin.y + 4.0f},
                    14.0f, Faded(textPrimary, alpha), kGlobalFontResourceId);
    };
    paintRarity("Magic (蓝色)", Rarity::Magic, 140.0f);
    paintRarity("Rare (黄色)", Rarity::Rare, 170.0f);
    paintRarity("Exalted (紫色)", Rarity::Epic, 200.0f);
  }

  // Batch salvage button.
  const UiRect batchBtn{{midX - 100.0f, layout.panelY + 620.0f}, {200.0f, 32.0f}};
  drawList.FillRect(layer, node, batchBtn, Faded(btnNormal, alpha));
  drawList.StrokeRect(layer, node, batchBtn, Faded(panelBorder, alpha), 1.0f);
  drawList.Text(layer, node, "按过滤器批量分解",
                {batchBtn.origin.x + 24.0f, batchBtn.origin.y + 7.0f}, 16.0f,
                Faded(textPrimary, alpha), kGlobalFontResourceId);
}

} // namespace NoMoreDay::ui
