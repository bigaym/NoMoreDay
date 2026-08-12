// R6 (UI System Rearchitecture remediation): the legacy immediate-mode Draw is
// gone. The inventory controller now follows the remediation frame order
// (design section 3.1):
//   - Update (host Update phase): interaction phase reading the frame snapshot
//     + injected pointer state. Every gameplay action is enqueued as a
//     GameUiIntent and executed by GameUiCommandHandler in the NEXT gameplay
//     Update phase. The ECS registry is never touched here.
//   - Paint (host PrepareRender phase): registry-free, input-free draw-list
//     emission driven by the snapshot + Update-phase caches.
// The material filter (C-01) is rebuilt only when the snapshot revision, the
// search query or the selected category changed, and the match is a
// no-allocation case-insensitive scan.

#include "game/application/ui/UIInventoryController.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include <entt/entt.hpp>

#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/GameUiIntent.hpp"
#include "game/application/ui/GameUiSnapshot.hpp"
#include "game/application/ui/UiResourceIds.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/RunewordSystem.hpp"
#include "core/utils/FmtBuffer.hpp"
#include "raylib.h"

using namespace NoMoreDay; // NOLINT: legacy file-scope style

namespace NoMoreDay::ui {

namespace {

// Layout constants (unchanged from the legacy Draw so the panel looks the
// same; geometry was verified against the pre-remediation paint).
constexpr float kPanelW = 1220.0f;
constexpr float kPanelH = 760.0f;
constexpr float kPadding = 20.0f;
constexpr float kSectionGap = 24.0f;
constexpr float kSectionHeaderH = 56.0f;
constexpr float kHeaderHeight = 60.0f;
constexpr float kEquipSlotSize = 56.0f;
constexpr float kInvSlotSize = 48.0f;
constexpr float kInvSlotGap = 5.0f;
constexpr float kBagSlotSize = 48.0f;
constexpr int kMaxBagSlots = 4;
constexpr float kRowGap = 36.0f;
constexpr float kAttrBtnSize = 26.0f;
constexpr float kTabW = 110.0f;
constexpr float kTabH = 28.0f;
constexpr float kMaterialRowH = 40.0f;

constexpr UiColor kWhiteTint{255, 255, 255, 255};
constexpr UiColor kBlackText{0, 0, 0, 255};

// Alpha-multiplies a color (paint colors derive from the theme + panel alpha).
[[nodiscard]] UiColor Faded(const UiColor& color, float alpha) noexcept {
  const std::uint8_t a = static_cast<std::uint8_t>(
      static_cast<float>(color.a) * std::clamp(alpha, 0.0f, 1.0f));
  return {color.r, color.g, color.b, a};
}

[[nodiscard]] UiColor ToUiColor(const Color& color) noexcept {
  return {color.r, color.g, color.b, color.a};
}

// Case-insensitive substring match with zero allocation (C-01: the legacy
// code built a lowercase std::string per material entry every frame).
[[nodiscard]] bool ContainsIgnoreCase(const char* text, const char* query) noexcept {
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

} // namespace

UIInventoryController::UIInventoryController(UiRuntime& runtime,
                                             GameUiHost* uiHost)
    : m_runtime(runtime), m_uiHost(uiHost) {
  UiNodeDesc desc;
  desc.id = entt::hashed_string("ui_inventory_panel");
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
  // Default category: "All" (MaterialCategory::Count is a sentinel).
  m_selectedCategory = MaterialCategory::Count;
  m_equipmentSlotAnims.resize(11);
  // R6: panel origin is now instance state so dragging persists across frames
  // (legacy re-centered every Draw).
  m_panelX = (UI_REF_WIDTH - kPanelW) * 0.5f;
  m_panelY = (UI_REF_HEIGHT - kPanelH) * 0.5f;
  // U8 contract: the node is created visible (the per-frame Update reconciles
  // it with the panel visibility); LeaveGameplay hides it explicitly.
}

void UIInventoryController::EnterGameplay() {
  m_inGameplay = true;
  ResetSessionState();
  SetNodeVisible(true);
}

void UIInventoryController::LeaveGameplay() {
  m_inGameplay = false;
  m_visible = false;
  m_alpha = 0.0f;
  ResetSessionState();
  SetNodeVisible(false);
}

void UIInventoryController::SetVisible(bool visible) {
  if (m_visible == visible) {
    return;
  }
  m_visible = visible;
  if (!visible) {
    // R6: closing the panel resets the migrated session page (legacy
    // UIInventory::Toggle close path) and closes any context menu opened from
    // the panel through the hosted overlay.
    m_inventoryPage = 0;
    m_inventoryScrollOffset = 0.0f;
    if (m_uiHost != nullptr) {
      m_uiHost->CloseContextMenu();
    }
  }
}

void UIInventoryController::Toggle() { SetVisible(!m_visible); }

UiId UIInventoryController::NodeId() const noexcept { return m_rootNodeId; }

bool UIInventoryController::IsInGameplay() const noexcept { return m_inGameplay; }

UIDragSession& UIInventoryController::DragSession() noexcept {
  return m_uiHost != nullptr ? m_uiHost->DragSession() : m_localDragSession;
}

void UIInventoryController::EnqueueIntent(GameUiIntent intent) {
  if (m_uiHost != nullptr) {
    m_uiHost->EnqueueIntent(std::move(intent));
  }
}

void UIInventoryController::SetNodeVisible(bool visible) {
  if (m_rootNodeId != kInvalidUiId) {
  (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

void UIInventoryController::ResetSessionState() noexcept {
  m_inventoryPage = 0;
  m_activeTab = 0;
  m_materialScrollOffset = 0.0f;
  m_inventoryScrollOffset = 0.0f;
  m_searchBuffer[0] = '\0';
  m_selectedCategory = MaterialCategory::Count;
  m_isSearchFocused = false;
  m_cachedLowerSearch[0] = '\0';
  m_cachedCategory = MaterialCategory::Count;
  m_filterCacheRevision = 0;
}

UIInventoryController::Layout UIInventoryController::ComputeLayout() const noexcept {
  Layout layout;
  layout.panelX = m_panelX;
  layout.panelY = m_panelY;
  layout.panelW = kPanelW;
  layout.panelH = kPanelH;

  const float leftPanelW = std::min(650.0f, kPanelW * 0.56f);
  const float rightPanelW = kPanelW - leftPanelW - kSectionGap;
  layout.leftPanelX = layout.panelX;
  layout.leftPanelW = leftPanelW;
  layout.rightPanelX = layout.panelX + leftPanelW + kSectionGap;
  layout.rightPanelW = rightPanelW;

  layout.equipX = layout.panelX + kPadding;
  layout.equipY = layout.panelY + kSectionHeaderH + 16.0f;
  layout.equipW = leftPanelW - kPadding * 2.0f;
  layout.equipH = kPanelH - kSectionHeaderH - 26.0f;

  layout.invX = layout.rightPanelX + kPadding;
  layout.tabY = layout.panelY + kSectionHeaderH + 8.0f;
  layout.invY = layout.tabY + kTabH + 28.0f;
  layout.invW = rightPanelW - kPadding * 2.0f;
  layout.invH = kPanelH - (layout.invY - layout.panelY) - 130.0f;
  layout.bottomY = layout.invY + layout.invH + 20.0f;
  layout.bagSlotsY = layout.bottomY + 50.0f;
  return layout;
}

int UIInventoryController::FreeSocketIndex(const GameUiItemView& item) noexcept {
  const int socketCount = static_cast<int>(item.socketCount);
  if (socketCount <= 0) {
    return -1;
  }
  const int slotCount = std::min(socketCount, static_cast<int>(item.sockets.size()));
  for (int i = 0; i < slotCount; ++i) {
    if (item.sockets[static_cast<std::size_t>(i)] == 0) {
      return i;
    }
  }
  return -1;
}

int UIInventoryController::FreeSocketIndex(
    const GameUiEquippedSlotView& item) noexcept {
  const int socketCount = static_cast<int>(item.socketCount);
  if (socketCount <= 0) {
    return -1;
  }
  const int slotCount =
      std::min(socketCount, static_cast<int>(item.sockets.size()));
  for (int i = 0; i < slotCount; ++i) {
    if (item.sockets[static_cast<std::size_t>(i)] == 0) {
      return i;
    }
  }
  return -1;
}

const GameUiItemView* UIInventoryController::FindDisplayedItem(
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

void UIInventoryController::RebuildMaterialFilter(const GameUiSnapshot& snapshot) {
  // Cache key: revision + query + category (design "hot path": no per-frame
  // filteredList / lowercase allocations). The query is lowercased once into
  // the fixed cache buffer; the match scan allocates nothing.
  char lowerSearch[64] = {0};
  std::size_t queryLen = std::strlen(m_searchBuffer);
  queryLen = std::min<std::size_t>(queryLen, sizeof(lowerSearch) - 1);
  for (std::size_t i = 0; i < queryLen; ++i) {
    lowerSearch[i] = static_cast<char>(
        std::tolower(static_cast<unsigned char>(m_searchBuffer[i])));
  }
  lowerSearch[queryLen] = '\0';

  const bool queryChanged = std::strcmp(lowerSearch, m_cachedLowerSearch) != 0;
  const bool categoryChanged = m_selectedCategory != m_cachedCategory;
  const bool revisionChanged = snapshot.revision != m_filterCacheRevision;

  if (!revisionChanged && !queryChanged && !categoryChanged) {
    return;
  }

  m_materialFilterCache.clear();
  // Reserve matches the snapshot's material row count (allocates only when the
  // cache grows; steady state reuses the vector storage).
  const std::size_t materialCount = snapshot.crafting.materials.size();
  if (m_materialFilterCache.capacity() < materialCount) {
    m_materialFilterCache.reserve(materialCount);
  }

  for (const GameUiMaterialView& entry : snapshot.crafting.materials) {
    const MaterialDefinition* def =
        MaterialRegistry::Get().GetMaterial(entry.materialId);
    if (def == nullptr) {
      continue;
    }
    if (m_selectedCategory != MaterialCategory::Count &&
        def->categoryEnum != m_selectedCategory) {
      continue;
    }
    if (queryLen > 0 && !ContainsIgnoreCase(def->name.c_str(), lowerSearch)) {
      continue;
    }
    m_materialFilterCache.push_back(entry);
  }

  m_filterCacheRevision = snapshot.revision;
  std::memcpy(m_cachedLowerSearch, lowerSearch, sizeof(lowerSearch));
  m_cachedCategory = m_selectedCategory;
}

void UIInventoryController::Update(const GameUiSnapshot& snapshot,
                                   const UiInputFrame& input,
                                   float mouseWheel,
                                   const LevelManager& /*levelManager*/) {
  // Alpha animation (instance state; m_visible authoritative).
  const float target = m_visible ? 1.0f : 0.0f;
  if (m_alpha < target) {
    m_alpha = std::min(target, m_alpha + 6.0f * input.deltaSeconds);
  } else if (m_alpha > target) {
    m_alpha = std::max(target, m_alpha - 6.0f * input.deltaSeconds);
  }

  if (!m_visible) {
    if (m_rootNodeId != kInvalidUiId) {
  (void)m_runtime.SetNodeVisible(m_rootNodeId, false);
    }
    return;
  }
  if (m_rootNodeId != kInvalidUiId) {
  (void)m_runtime.SetNodeVisible(m_rootNodeId, true);
  }
  if (!m_inGameplay) {
    return;
  }

  // Panel drag (U8: instance drag state via the shared service).
  {
    const float panelW = kPanelW;
    const float panelH = kPanelH;
    const UIPanelDragInputs dragInputs{
        {input.pointer.logicalPosition.x, input.pointer.logicalPosition.y},
        input.pointer.pressed,
        IsMouseButtonDown(MOUSE_LEFT_BUTTON),
    };
    const UIPanelDragBounds dragBounds{
        panelW, panelH, kHeaderHeight, 50.0f, UI_REF_WIDTH, UI_REF_HEIGHT};
    UIPanelDragService::UpdatePanelDrag(
        m_panelState, UIPanelID::Inventory, m_activeDragPanel, m_panelX,
        m_panelY, dragInputs, dragBounds);
  }

  const bool allowInventoryInput =
      m_uiHost == nullptr || !m_uiHost->IsModalInputCaptured();
  if (!allowInventoryInput) {
    return;
  }

  const Layout layout = ComputeLayout();
  const UiVec2 mouse = input.pointer.logicalPosition;
  const UiRect panelRect{{layout.panelX, layout.panelY},
                         {layout.panelW, layout.panelH}};

  if (panelRect.Contains(mouse) && m_uiHost != nullptr) {
    m_uiHost->SetMouseOverUI(true);
  }

  // Build the slot -> snapshot lookup maps for the paint phase.
  const std::size_t renderCount = std::max<std::size_t>(
      static_cast<std::size_t>(std::max(0, snapshot.inventory.capacity)),
      snapshot.inventory.items.size());
  m_slotToItemIndex.assign(renderCount, -1);
  for (std::size_t i = 0; i < snapshot.inventory.items.size(); ++i) {
    const int idx = snapshot.inventory.items[i].inventoryIndex;
    if (idx >= 0 && idx < static_cast<int>(renderCount)) {
      m_slotToItemIndex[static_cast<std::size_t>(idx)] = static_cast<int>(i);
    }
  }
  m_equipSlotIndex.fill(-1);
  for (std::size_t i = 0; i < snapshot.equipment.size(); ++i) {
    const std::uint8_t slot = snapshot.equipment[i].slotIndex;
    if (slot < m_equipSlotIndex.size()) {
      m_equipSlotIndex[slot] = static_cast<int>(i);
    }
  }
  m_draggedItemDomainId = DragSession().draggedItemDomainId;

  UIDragSession& drag = DragSession();
  const bool isDragging = drag.IsDragging();

  // --- Tabs -----------------------------------------------------------------
  for (int tab = 0; tab < 2; ++tab) {
    const UiRect tabRect{{layout.invX + static_cast<float>(tab) * (kTabW + 8.0f),
                          layout.tabY},
                         {kTabW, kTabH}};
    if (tabRect.Contains(mouse) && input.pointer.pressed) {
      m_activeTab = tab;
    }
  }

  if (m_activeTab == 0) {
    // --- Item grid -----------------------------------------------------------
    const float gridInnerW = layout.invW - 20.0f;
    const int cols = std::max(
        4, static_cast<int>((gridInnerW + kInvSlotGap) /
                            (kInvSlotSize + kInvSlotGap)));
    const int totalRows =
        static_cast<int>((renderCount + static_cast<std::size_t>(cols) - 1) /
                         static_cast<std::size_t>(cols));
    const float contentHeight =
        static_cast<float>(totalRows) * (kInvSlotSize + kInvSlotGap) + 20.0f;
    const UiRect gridViewport{{layout.invX, layout.invY},
                              {layout.invW, layout.invH}};

    if (gridViewport.Contains(mouse)) {
      m_inventoryScrollOffset -= mouseWheel * (kInvSlotSize + kInvSlotGap) * 2.0f;
      const float maxScroll = std::max(0.0f, contentHeight - layout.invH);
      m_inventoryScrollOffset = std::clamp(m_inventoryScrollOffset, 0.0f, maxScroll);
    }

    const float gridStartX = layout.invX + 15.0f;
    const float gridStartY = layout.invY + 15.0f - m_inventoryScrollOffset;

    for (std::size_t i = 0; i < renderCount; ++i) {
      const int row = static_cast<int>(i / static_cast<std::size_t>(cols));
      const int col = static_cast<int>(i % static_cast<std::size_t>(cols));
      const float x = gridStartX + static_cast<float>(col) * (kInvSlotSize + kInvSlotGap);
      const float y = gridStartY + static_cast<float>(row) * (kInvSlotSize + kInvSlotGap);
      if (y + kInvSlotSize < layout.invY || y > layout.invY + layout.invH) {
        continue; // Cull rows outside the scissor region.
      }
      const UiRect slotRect{{x, y}, {kInvSlotSize, kInvSlotSize}};
      const bool hovered = gridViewport.Contains(mouse) && slotRect.Contains(mouse);
      const int itemIdx = m_slotToItemIndex[i];
      const GameUiItemView* item =
          itemIdx >= 0 ? &snapshot.inventory.items[static_cast<std::size_t>(itemIdx)]
                       : nullptr;

      if (hovered) {
        if (m_uiHost != nullptr) {
          m_uiHost->SetHoveredItemDomain(
              item != nullptr ? item->domainId : kInvalidDomainId);
        }
      }

      const bool rmbPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
      if (rmbPressed && hovered && item != nullptr &&
          drag.draggedItemDomainId == 0 && m_uiHost != nullptr) {
        // Right-click opens the context menu through the hosted overlay.
        m_uiHost->OpenContextMenuDomain(item->domainId, true,
                                        static_cast<int>(i),
                                        EquipmentSlot::None);
      } else if (input.pointer.pressed && hovered && item != nullptr) {
        drag.draggedItemDomainId = item->domainId;
        drag.isDraggingFromInventory = true;
        drag.dragSourceInventoryIndex = static_cast<int>(i);
        drag.dragSourceEquipmentSlot = EquipmentSlot::None;
        drag.dragSourceBagSlotIndex = -1;
      } else if (input.pointer.released && isDragging && hovered && item != nullptr) {
        const GameUiItemView* dragItem =
            FindDisplayedItem(snapshot, drag.draggedItemDomainId);
        if (dragItem != nullptr && drag.draggedItemDomainId != item->domainId) {
          const bool dragIsRune =
              RunewordSystem::isRune(dragItem->itemId);
          if (dragIsRune) {
            const int freeIdx = FreeSocketIndex(*item);
            if (freeIdx >= 0) {
              // Socket the rune into the target item.
              GameUiIntent intent;
              intent.sourceNode = m_rootNodeId;
              intent.kind = GameUiIntentKind::SocketRune;
              intent.payload.targetDomainId = item->domainId;
              intent.payload.sourceDomainId = drag.draggedItemDomainId;
              intent.payload.socketIndex = static_cast<std::uint8_t>(freeIdx);
              intent.payload.itemSource =
                  drag.isDraggingFromInventory
                      ? static_cast<std::uint8_t>(GameUiItemSource::Inventory)
                      : (drag.dragSourceEquipmentSlot != EquipmentSlot::None
                             ? static_cast<std::uint8_t>(GameUiItemSource::Equipment)
                             : static_cast<std::uint8_t>(GameUiItemSource::Bag));
              intent.payload.sourceSlot =
                  drag.isDraggingFromInventory
                      ? drag.dragSourceInventoryIndex
                      : (drag.dragSourceEquipmentSlot != EquipmentSlot::None
                             ? static_cast<std::int32_t>(drag.dragSourceEquipmentSlot)
                             : drag.dragSourceBagSlotIndex);
              EnqueueIntent(std::move(intent));
              drag.Clear();
              continue;
            }
            if (item->socketCount > 0 && m_uiHost != nullptr) {
              m_uiHost->ShowMessageBox("No free socket");
              drag.Clear();
              continue;
            }
          }

          // Drop / swap into the grid slot.
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          if (drag.isDraggingFromStash) {
            intent.kind = GameUiIntentKind::StashWithdraw;
            intent.payload.sourceTab = drag.dragSourceStashTab;
            intent.payload.sourceSlot = drag.dragSourceStashSlot;
            intent.payload.targetSlot = static_cast<std::int32_t>(i);
            intent.payload.stashTarget =
        static_cast<std::uint8_t>(drag.dragSourceStashType);
          } else if (drag.isDraggingFromInventory) {
            intent.kind = GameUiIntentKind::SwapItems;
            intent.payload.sourceSlot = drag.dragSourceInventoryIndex;
            intent.payload.targetSlot = static_cast<std::int32_t>(i);
          } else if (drag.dragSourceEquipmentSlot != EquipmentSlot::None) {
            intent.kind = GameUiIntentKind::UnequipItem;
            intent.payload.sourceDomainId = drag.draggedItemDomainId;
            intent.payload.equipmentSlot =
                static_cast<std::uint8_t>(drag.dragSourceEquipmentSlot);
            intent.payload.targetSlot = static_cast<std::int32_t>(i);
          } else if (drag.dragSourceBagSlotIndex != -1) {
            intent.kind = GameUiIntentKind::BagUnequip;
            intent.payload.bagAction =
                static_cast<std::uint8_t>(GameUiBagAction::Unequip);
            intent.payload.sourceSlot = drag.dragSourceBagSlotIndex;
            intent.payload.targetSlot = static_cast<std::int32_t>(i);
          }
          if (intent.kind != GameUiIntentKind::PickupItem) {
            EnqueueIntent(std::move(intent));
          }
          drag.Clear();
        }
      }
    }
  } else {
    // --- Materials tab -------------------------------------------------------
    const UiRect searchRect{{layout.invX + 10.0f, layout.invY},
                            {200.0f, 28.0f}};
    if (searchRect.Contains(mouse) && input.pointer.pressed) {
      m_isSearchFocused = true;
    } else if (input.pointer.pressed &&
               !searchRect.Contains(mouse)) {
      m_isSearchFocused = false;
    }

    if (m_isSearchFocused) {
      // Legacy parity: text input reads raylib directly (headless-safe: the
      // calls return neutral values without a window).
      int ch = GetCharPressed();
      while (ch > 0) {
        const std::size_t len = std::strlen(m_searchBuffer);
        if ((ch >= 32 && ch <= 125) && len < sizeof(m_searchBuffer) - 1) {
          m_searchBuffer[len] = static_cast<char>(ch);
          m_searchBuffer[len + 1] = '\0';
        }
        ch = GetCharPressed();
      }
      if (IsKeyPressed(KEY_BACKSPACE)) {
        const std::size_t len = std::strlen(m_searchBuffer);
        if (len > 0) {
          m_searchBuffer[len - 1] = '\0';
        }
      }
    }

    // Category filter row.
    struct CategoryDef {
      const char* label;
      MaterialCategory category;
    };
    static constexpr CategoryDef kCategories[] = {
        {"All", MaterialCategory::Count},
        {"Ore", MaterialCategory::Mineral},
        {"Fragment", MaterialCategory::Fragment},
        {"Rune", MaterialCategory::Rune},
    };
    float catX = layout.invX + 220.0f;
    for (const CategoryDef& def : kCategories) {
      const float btnW =
          static_cast<float>(std::strlen(def.label)) * 11.0f + 24.0f;
      const UiRect catRect{{catX, layout.invY + 2.0f}, {btnW, 24.0f}};
      if (catRect.Contains(mouse) && input.pointer.pressed) {
        m_selectedCategory = def.category;
        m_materialScrollOffset = 0.0f;
      }
      catX += btnW + 8.0f;
    }

    // C-01: rebuild the filter cache only on revision/query/category change.
    RebuildMaterialFilter(snapshot);

    const float listTopY = layout.invY + 28.0f + 10.0f;
    const float listH = layout.invH - (listTopY - layout.invY);
    const float contentHeight =
        static_cast<float>(m_materialFilterCache.size()) * kMaterialRowH + 10.0f;
    const UiRect listViewport{{layout.invX, listTopY},
                              {layout.invW, listH}};
    if (listViewport.Contains(mouse)) {
      m_materialScrollOffset -= mouseWheel * 80.0f;
      const float maxScroll =
          std::max(0.0f, contentHeight - listH);
      m_materialScrollOffset =
          std::clamp(m_materialScrollOffset, 0.0f, maxScroll);
    }
  }

  // --- Equipment slots ------------------------------------------------------
  {
    const float centerX = layout.equipX + layout.equipW * 0.5f;
    const float topY = layout.equipY + 56.0f;
    const float leftColX = centerX - 150.0f;
    const float rightColX = centerX + 94.0f;
    const float centerColX = centerX - 28.0f;

    struct SlotDef {
      EquipmentSlot slot;
      float x;
      float y;
    };
    const SlotDef slotDefs[11] = {
        {EquipmentSlot::Neck, centerColX - 18.0f, topY + 34.0f},
        {EquipmentSlot::Head, leftColX, topY + 86.0f},
        {EquipmentSlot::Shoulder, rightColX, topY + 86.0f},
        {EquipmentSlot::Chest, leftColX, topY + 164.0f},
        {EquipmentSlot::Hands, rightColX, topY + 164.0f},
        {EquipmentSlot::MainHand, leftColX, topY + 242.0f},
        {EquipmentSlot::OffHand, rightColX, topY + 242.0f},
        {EquipmentSlot::Ring1, leftColX, topY + 320.0f},
        {EquipmentSlot::Ring2, rightColX, topY + 320.0f},
        {EquipmentSlot::Legs, centerColX, topY + 392.0f},
        {EquipmentSlot::Feet, centerColX, topY + 468.0f},
    };

    for (int i = 0; i < 11; ++i) {
      const SlotDef& def = slotDefs[i];
      const UiRect slotRect{{def.x, def.y}, {kEquipSlotSize, kEquipSlotSize}};
      const bool hovered = slotRect.Contains(mouse);
      ElementAnim& anim = m_equipmentSlotAnims[static_cast<std::size_t>(i)];
      anim.hoverValue += (hovered ? 1.0f : 0.0f - anim.hoverValue) * 15.0f *
                         input.deltaSeconds;
      anim.hoverValue = std::clamp(anim.hoverValue, 0.0f, 1.0f);

      const int equipIdx =
          m_equipSlotIndex[static_cast<std::size_t>(def.slot)];
      const GameUiEquippedSlotView* item =
          equipIdx >= 0 ? &snapshot.equipment[static_cast<std::size_t>(equipIdx)]
                        : nullptr;

      if (hovered && item != nullptr && !isDragging && m_uiHost != nullptr) {
        m_uiHost->SetHoveredItemDomain(item->domainId);
      }

      const bool rmbPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
      if (rmbPressed && hovered && item != nullptr && !isDragging &&
          m_uiHost != nullptr) {
        m_uiHost->OpenContextMenuDomain(item->domainId, false, -1, def.slot);
      } else if (hovered && input.pointer.pressed && item != nullptr) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
          // Quick unequip (legacy Shift+LMB path) -> intent.
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::UnequipItem;
          intent.payload.sourceDomainId = item->domainId;
          intent.payload.equipmentSlot =
              static_cast<std::uint8_t>(def.slot);
          intent.payload.targetSlot = -1;
          EnqueueIntent(std::move(intent));
        } else {
          drag.draggedItemDomainId = item->domainId;
          drag.isDraggingFromInventory = false;
          drag.dragSourceInventoryIndex = -1;
          drag.dragSourceEquipmentSlot = def.slot;
          drag.dragSourceBagSlotIndex = -1;
        }
      } else if (input.pointer.released && isDragging && hovered) {
        // Drop onto an equipment slot.
        const GameUiItemView* dragItem =
            FindDisplayedItem(snapshot, drag.draggedItemDomainId);
        if (dragItem != nullptr && drag.draggedItemDomainId != kInvalidDomainId) {
          const bool dragIsRune = RunewordSystem::isRune(dragItem->itemId);
          if (dragIsRune && item != nullptr) {
            const int freeIdx = FreeSocketIndex(*item);
            if (freeIdx >= 0) {
              GameUiIntent intent;
              intent.sourceNode = m_rootNodeId;
              intent.kind = GameUiIntentKind::SocketRune;
              intent.payload.targetDomainId = item->domainId;
              intent.payload.sourceDomainId = drag.draggedItemDomainId;
              intent.payload.socketIndex = static_cast<std::uint8_t>(freeIdx);
              intent.payload.itemSource =
                  drag.isDraggingFromInventory
                      ? static_cast<std::uint8_t>(GameUiItemSource::Inventory)
                      : static_cast<std::uint8_t>(GameUiItemSource::Equipment);
              intent.payload.sourceSlot =
                  drag.isDraggingFromInventory
                      ? drag.dragSourceInventoryIndex
                      : static_cast<std::int32_t>(drag.dragSourceEquipmentSlot);
              EnqueueIntent(std::move(intent));
              drag.Clear();
              continue;
            }
            if (item->socketCount > 0 && m_uiHost != nullptr) {
              m_uiHost->ShowMessageBox("No free socket");
              drag.Clear();
              continue;
            }
          }

          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::EquipItem;
          intent.payload.sourceDomainId = drag.draggedItemDomainId;
          intent.payload.equipmentSlot = static_cast<std::uint8_t>(def.slot);
          if (drag.isDraggingFromInventory && drag.dragSourceInventoryIndex != -1) {
            intent.payload.itemSource =
                static_cast<std::uint8_t>(GameUiItemSource::Inventory);
            intent.payload.sourceSlot = drag.dragSourceInventoryIndex;
          } else if (drag.dragSourceEquipmentSlot != EquipmentSlot::None &&
                     drag.dragSourceEquipmentSlot != def.slot) {
            intent.payload.itemSource =
                static_cast<std::uint8_t>(GameUiItemSource::Equipment);
            intent.payload.sourceSlot =
                static_cast<std::int32_t>(drag.dragSourceEquipmentSlot);
          } else if (drag.dragSourceBagSlotIndex != -1) {
            intent.payload.itemSource =
                static_cast<std::uint8_t>(GameUiItemSource::Bag);
            intent.payload.sourceSlot = drag.dragSourceBagSlotIndex;
          }
          EnqueueIntent(std::move(intent));
          drag.Clear();
        }
      }
    }
  }

  // --- Bottom bar: gold, sort, bag slots -------------------------------------
  {
    const UiRect sortRect{{layout.invX + layout.invW - 150.0f,
                           layout.bottomY - 5.0f},
                          {140.0f, 36.0f}};
    if (sortRect.Contains(mouse) && input.pointer.pressed) {
      GameUiIntent intent;
      intent.sourceNode = m_rootNodeId;
      intent.kind = GameUiIntentKind::OrganizeInventory;
      EnqueueIntent(std::move(intent));
    }

    const float bagSlotSize = kBagSlotSize;
    for (int i = 0; i < kMaxBagSlots; ++i) {
      const float x = layout.invX + 5.0f +
                      static_cast<float>(i) * (bagSlotSize + 15.0f);
      const UiRect slotRect{{x, layout.bagSlotsY}, {bagSlotSize, bagSlotSize}};
      const bool hovered = slotRect.Contains(mouse);
      const GameUiBagSlotView& bag = snapshot.inventory.bagSlots[i];
      const bool hasBag = bag.domainId != kInvalidDomainId;

      if (hovered && hasBag && !isDragging && m_uiHost != nullptr) {
        m_uiHost->SetHoveredItemDomain(bag.domainId);
      }

      if (hovered && input.pointer.pressed && hasBag &&
          drag.draggedItemDomainId == 0) {
        drag.draggedItemDomainId = bag.domainId;
        drag.isDraggingFromInventory = false;
        drag.dragSourceInventoryIndex = -1;
        drag.dragSourceEquipmentSlot = EquipmentSlot::None;
        drag.dragSourceBagSlotIndex = i;
      } else if (hovered && input.pointer.released && isDragging &&
                 drag.dragSourceBagSlotIndex != i) {
        const GameUiItemView* dragItem =
            FindDisplayedItem(snapshot, drag.draggedItemDomainId);
        if (dragItem != nullptr &&
            dragItem->itemType == static_cast<std::uint8_t>(ItemType::Bag)) {
          GameUiIntent intent;
          intent.sourceNode = m_rootNodeId;
          intent.kind = GameUiIntentKind::BagEquip;
          intent.payload.sourceDomainId = drag.draggedItemDomainId;
          intent.payload.targetSlot = i;
          intent.payload.itemSource =
              drag.isDraggingFromInventory
                  ? static_cast<std::uint8_t>(GameUiItemSource::Inventory)
                  : static_cast<std::uint8_t>(GameUiItemSource::Bag);
          intent.payload.sourceSlot =
              drag.isDraggingFromInventory
                  ? drag.dragSourceInventoryIndex
                  : drag.dragSourceBagSlotIndex;
          EnqueueIntent(std::move(intent));
          drag.Clear();
        }
      }
    }
  }
}

void UIInventoryController::Paint(UiDrawList& drawList,
                                  const UiViewport& /*viewport*/,
                                  const GameUiSnapshot& snapshot) const {
  if (!m_visible || !m_inGameplay || m_alpha <= 0.001f ||
      m_rootNodeId == kInvalidUiId) {
    return;
  }
  const float alpha = m_alpha;
  const Layout layout = ComputeLayout();
  const UiColor& themeBg = ToUiColor(UIRenderer::GetTheme().panelBackground);
  const UiColor& themeBorder = ToUiColor(UIRenderer::GetTheme().panelBorder);
  const UiColor& themeText = ToUiColor(UIRenderer::GetTheme().textPrimary);
  const UiColor& themeHighlight =
      ToUiColor(UIRenderer::GetTheme().textHighlight);
  const UiColor& themeBtn = ToUiColor(UIRenderer::GetTheme().buttonNormal);

  // Panel frame.
  drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId,
                    {{layout.panelX, layout.panelY},
                     {layout.panelW, layout.panelH}},
                    Faded(themeBg, alpha));
  drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId,
                      {{layout.panelX, layout.panelY},
                       {layout.panelW, layout.panelH}},
                      Faded(themeBorder, alpha), 2.0f);

  // Left section (equipment).
  drawList.FillRect(
      UiDrawLayer::Panels, m_rootNodeId,
      {{layout.leftPanelX + 1.0f, layout.panelY + 1.0f},
       {layout.leftPanelW - 2.0f, kPanelH - 2.0f}},
      Faded(themeBg, alpha * 0.5f));
  drawList.Text(UiDrawLayer::Panels, m_rootNodeId,
                "Equipment", {layout.panelX + 20.0f, layout.panelY + 18.0f},
                30.0f, Faded(themeHighlight, alpha), kGlobalFontResourceId);

  // Right section (inventory).
  drawList.FillRect(
      UiDrawLayer::Panels, m_rootNodeId,
      {{layout.rightPanelX + 1.0f, layout.panelY + 1.0f},
       {layout.rightPanelW - 2.0f, kPanelH - 2.0f}},
      Faded(themeBg, alpha * 0.5f));
  drawList.Text(UiDrawLayer::Panels, m_rootNodeId,
                "Inventory", {layout.rightPanelX + 20.0f, layout.panelY + 18.0f},
                30.0f, Faded(themeText, alpha), kGlobalFontResourceId);
  drawList.Text(UiDrawLayer::Panels, m_rootNodeId,
                "Press I / ESC to close",
                {layout.rightPanelX + layout.rightPanelW - 200.0f,
                 layout.panelY + 18.0f},
                16.0f, Faded(themeText, alpha), kGlobalFontResourceId);

  // Equipment slots.
  {
    const float centerX = layout.equipX + layout.equipW * 0.5f;
    const float topY = layout.equipY + 56.0f;
    const float leftColX = centerX - 150.0f;
    const float rightColX = centerX + 94.0f;
    const float centerColX = centerX - 28.0f;

    struct SlotDef {
      EquipmentSlot slot;
      float x;
      float y;
      const char* label;
    };
    const SlotDef slotDefs[11] = {
        {EquipmentSlot::Neck, centerColX - 18.0f, topY + 34.0f, "Neck"},
        {EquipmentSlot::Head, leftColX, topY + 86.0f, "Head"},
        {EquipmentSlot::Shoulder, rightColX, topY + 86.0f, "Shoulder"},
        {EquipmentSlot::Chest, leftColX, topY + 164.0f, "Chest"},
        {EquipmentSlot::Hands, rightColX, topY + 164.0f, "Hands"},
        {EquipmentSlot::MainHand, leftColX, topY + 242.0f, "Main"},
        {EquipmentSlot::OffHand, rightColX, topY + 242.0f, "Off"},
        {EquipmentSlot::Ring1, leftColX, topY + 320.0f, "Ring 1"},
        {EquipmentSlot::Ring2, rightColX, topY + 320.0f, "Ring 2"},
        {EquipmentSlot::Legs, centerColX, topY + 392.0f, "Legs"},
        {EquipmentSlot::Feet, centerColX, topY + 468.0f, "Feet"},
    };

    for (int i = 0; i < 11; ++i) {
      const SlotDef& def = slotDefs[i];
      const float animHover =
          m_equipmentSlotAnims[static_cast<std::size_t>(i)].hoverValue;
      const float slotScale = 1.0f + 0.08f * animHover;
      const float offset = (kEquipSlotSize * (slotScale - 1.0f)) * 0.5f;
      const UiRect slotRect{{def.x - offset, def.y - offset},
                            {kEquipSlotSize * slotScale, kEquipSlotSize * slotScale}};

  const int equipIdx =
      m_equipSlotIndex[static_cast<std::size_t>(def.slot)];
  const GameUiEquippedSlotView* item =
      equipIdx >= 0
          ? &snapshot.equipment[static_cast<std::size_t>(equipIdx)]
          : nullptr;

      drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId, slotRect,
                        Faded(ToUiColor(UIRenderer::GetTheme().slotBackground),
                              alpha));
      if (item != nullptr) {
        drawList.StrokeRect(
            UiDrawLayer::Panels, m_rootNodeId, slotRect,
            Faded(ToUiColor(UIRenderer::GetRarityColor(
                      static_cast<Rarity>(item->rarity))),
                  alpha),
            2.0f);
        if (item->textureId != 0) {
          const float pad = 4.0f;
          drawList.Image(
              UiDrawLayer::Panels, m_rootNodeId,
              {{slotRect.origin.x + pad, slotRect.origin.y + pad},
               {slotRect.size.x - pad * 2.0f, slotRect.size.y - pad * 2.0f}},
              item->textureId, Faded(kWhiteTint, alpha));
        }
        if (item->quantity > 1) {
          char buf[16] = {0};
          utils::FormatToBuffer(buf, "{}", item->quantity);
          drawList.Text(UiDrawLayer::Panels, m_rootNodeId, buf,
                        {slotRect.Right() - 18.0f, slotRect.Bottom() - 16.0f},
                        12.0f, Faded(themeText, alpha), kGlobalFontResourceId);
        }
      } else {
        drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId, slotRect,
                            Faded(themeBorder, alpha), 1.0f);
      }
      drawList.Text(UiDrawLayer::Panels, m_rootNodeId, def.label,
                    {def.x + kEquipSlotSize + 10.0f, def.y + (kEquipSlotSize - 16.0f) * 0.5f},
                    16.0f, Faded(animHover > 0.5f ? themeHighlight : themeText,
                                 alpha),
                    kGlobalFontResourceId);
    }
  }

  // Tabs.
  for (int tab = 0; tab < 2; ++tab) {
    const bool selected = m_activeTab == tab;
    const UiRect tabRect{{layout.invX + static_cast<float>(tab) * (kTabW + 8.0f),
                          layout.tabY},
                         {kTabW, kTabH}};
    drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId, tabRect,
                      selected ? Faded(themeHighlight, alpha) : Faded(themeBtn, alpha));
    drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId, tabRect,
                        Faded(themeBorder, alpha), 1.0f);
    const char* label = tab == 0 ? "Items" : "Materials";
    drawList.Text(UiDrawLayer::Panels, m_rootNodeId, label,
                  {tabRect.origin.x + 10.0f,
                   tabRect.origin.y + (kTabH - 16.0f) * 0.5f},
                  16.0f, selected ? kBlackText : Faded(themeText, alpha),
                  kGlobalFontResourceId);
  }

  if (m_activeTab == 0) {
    // Item grid.
    const float gridInnerW = layout.invW - 20.0f;
    const int cols = std::max(
        4, static_cast<int>((gridInnerW + kInvSlotGap) /
                            (kInvSlotSize + kInvSlotGap)));
    const std::size_t renderCount = std::max<std::size_t>(
        static_cast<std::size_t>(std::max(0, snapshot.inventory.capacity)),
        snapshot.inventory.items.size());

    drawList.PushClip({{layout.invX, layout.invY}, {layout.invW, layout.invH}});
    const float gridStartX = layout.invX + 15.0f;
    const float gridStartY = layout.invY + 15.0f - m_inventoryScrollOffset;
    for (std::size_t i = 0; i < renderCount; ++i) {
      const int row = static_cast<int>(i / static_cast<std::size_t>(cols));
      const int col = static_cast<int>(i % static_cast<std::size_t>(cols));
      const float x = gridStartX + static_cast<float>(col) * (kInvSlotSize + kInvSlotGap);
      const float y = gridStartY + static_cast<float>(row) * (kInvSlotSize + kInvSlotGap);
      if (y + kInvSlotSize < layout.invY || y > layout.invY + layout.invH) {
        continue;
      }
      const UiRect slotRect{{x, y}, {kInvSlotSize, kInvSlotSize}};
      const int itemIdx = m_slotToItemIndex[i];
      const GameUiItemView* item =
          itemIdx >= 0
              ? &snapshot.inventory.items[static_cast<std::size_t>(itemIdx)]
              : nullptr;
      // Hide the item under the cursor in its source slot (drag preview).
      if (item != nullptr &&
          item->domainId == m_draggedItemDomainId) {
        item = nullptr;
      }
      drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId, slotRect,
                        Faded(ToUiColor(UIRenderer::GetTheme().slotBackground),
                              alpha));
      if (item != nullptr) {
        drawList.StrokeRect(
            UiDrawLayer::Panels, m_rootNodeId, slotRect,
            Faded(ToUiColor(UIRenderer::GetRarityColor(
                      static_cast<Rarity>(item->rarity))),
                  alpha),
            1.0f);
        if (item->textureId != 0) {
          const float pad = 4.0f;
          drawList.Image(UiDrawLayer::Panels, m_rootNodeId,
                         {{x + pad, y + pad},
                          {kInvSlotSize - pad * 2.0f, kInvSlotSize - pad * 2.0f}},
                         item->textureId, Faded(kWhiteTint, alpha));
        }
        if (item->quantity > 1) {
          char buf[16] = {0};
          utils::FormatToBuffer(buf, "{}", item->quantity);
          drawList.Text(UiDrawLayer::Panels, m_rootNodeId, buf,
                        {slotRect.Right() - 18.0f, slotRect.Bottom() - 16.0f},
                        12.0f, Faded(themeText, alpha), kGlobalFontResourceId);
        }
      } else {
        drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId, slotRect,
                            Faded(themeBorder, alpha), 1.0f);
      }
    }
    drawList.PopClip();
  } else {
    // Materials list.
    const float listTopY = layout.invY + 28.0f + 10.0f;
    const float listH = layout.invH - (listTopY - layout.invY);
    drawList.PushClip({{layout.invX, listTopY}, {layout.invW, listH}});
    const float listStartY = listTopY + 5.0f - m_materialScrollOffset;
    for (std::size_t i = 0; i < m_materialFilterCache.size(); ++i) {
      const float y = listStartY + static_cast<float>(i) * kMaterialRowH;
      if (y + kMaterialRowH < listTopY || y > listTopY + listH) {
        continue;
      }
      const GameUiMaterialView& entry = m_materialFilterCache[i];
      const MaterialDefinition* def =
          MaterialRegistry::Get().GetMaterial(entry.materialId);
      if (def == nullptr) {
        continue;
      }
      const UiRect rowRect{{layout.invX + 2.0f, y},
                           {layout.invW - 4.0f, kMaterialRowH - 4.0f}};
      drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId, rowRect,
                        Faded(themeBtn, alpha * 0.15f));
      drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId, rowRect,
                          Faded(themeBorder, alpha * 0.5f), 1.0f);
      drawList.Text(UiDrawLayer::Panels, m_rootNodeId, def->name.c_str(),
                    {rowRect.origin.x + 8.0f, rowRect.origin.y + 8.0f}, 20.0f,
                    Faded(ToUiColor(UIRenderer::GetRarityColor(def->rarity)),
                          alpha),
                    kGlobalFontResourceId);
      char qtyBuf[16] = {0};
      utils::FormatToBuffer(qtyBuf, "x{}", entry.count);
      drawList.Text(UiDrawLayer::Panels, m_rootNodeId, qtyBuf,
                    {rowRect.Right() - 80.0f, rowRect.origin.y + 8.0f}, 20.0f,
                    Faded(themeHighlight, alpha), kGlobalFontResourceId);
    }
    drawList.PopClip();
  }

  // Bottom bar: gold, sort button, bag slots.
  {
    char goldBuf[32] = {0};
    utils::FormatToBuffer(goldBuf, "Gold: {}", snapshot.inventory.gold);
    drawList.Text(UiDrawLayer::Panels, m_rootNodeId, goldBuf,
                  {layout.invX + 5.0f, layout.bottomY}, 20.0f,
                  Faded(themeHighlight, alpha), kGlobalFontResourceId);

    const UiRect sortRect{{layout.invX + layout.invW - 150.0f,
                           layout.bottomY - 5.0f},
                          {140.0f, 36.0f}};
    drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId, sortRect,
                      Faded(themeBtn, alpha));
    drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId, sortRect,
                        Faded(themeBorder, alpha), 1.0f);
    drawList.Text(UiDrawLayer::Panels, m_rootNodeId, "Sort",
                  {sortRect.origin.x + 8.0f,
                   sortRect.origin.y + (36.0f - 18.0f) * 0.5f},
                  18.0f, Faded(themeText, alpha), kGlobalFontResourceId);

    drawList.Text(UiDrawLayer::Panels, m_rootNodeId, "Bag slots",
                  {layout.invX + 5.0f, layout.bagSlotsY - 25.0f}, 18.0f,
                  Faded(themeText, alpha), kGlobalFontResourceId);
    for (int i = 0; i < kMaxBagSlots; ++i) {
      const float x = layout.invX + 5.0f +
                      static_cast<float>(i) * (kBagSlotSize + 15.0f);
      const UiRect slotRect{{x, layout.bagSlotsY}, {kBagSlotSize, kBagSlotSize}};
      const GameUiBagSlotView& bag = snapshot.inventory.bagSlots[i];
      const bool hasBag = bag.domainId != kInvalidDomainId;
      drawList.FillRect(UiDrawLayer::Panels, m_rootNodeId, slotRect,
                        Faded(ToUiColor(UIRenderer::GetTheme().slotBackground),
                              alpha));
      if (hasBag) {
        drawList.StrokeRect(
            UiDrawLayer::Panels, m_rootNodeId, slotRect,
            Faded(ToUiColor(UIRenderer::GetRarityColor(
                      static_cast<Rarity>(bag.rarity))),
                  alpha),
            1.0f);
        if (bag.textureId != 0) {
          const float pad = 4.0f;
          drawList.Image(UiDrawLayer::Panels, m_rootNodeId,
                         {{x + pad, layout.bagSlotsY + pad},
                          {kBagSlotSize - pad * 2.0f, kBagSlotSize - pad * 2.0f}},
                         bag.textureId, Faded(kWhiteTint, alpha));
        }
      } else {
        drawList.StrokeRect(UiDrawLayer::Panels, m_rootNodeId, slotRect,
                            Faded(themeBorder, alpha), 1.0f);
      }
      drawList.Text(UiDrawLayer::Panels, m_rootNodeId, "Slot",
                    {x + 2.0f, layout.bagSlotsY + kBagSlotSize + 4.0f}, 12.0f,
                    Faded(themeText, alpha), kGlobalFontResourceId);
    }
  }
}

} // namespace NoMoreDay::ui
