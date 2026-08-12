#include "game/application/ui/SkillHotbarController.hpp"

#include "core/logging/Logger.hpp"
#include "engine/resource/AssetLoadingSystem.hpp"
#include "game/application/ui/GameUiHost.hpp"
#include "game/application/ui/UICommon.hpp"
#include "game/application/ui/UiDrawList.hpp"
#include "game/application/ui/UIRenderer.hpp"
#include "game/application/ui/UISystem.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace NoMoreDay::ui {

SkillHotbarController::SkillHotbarController(UiRuntime& runtime,
                                             TooltipController* tooltipController,
                                             GameUiHost* uiHost)
    : m_tooltip(tooltipController), m_uiHost(uiHost), m_runtime(runtime) {
  UiNodeDesc desc;
  desc.id = static_cast<UiId>(entt::hashed_string("ui_skill_hotbar").value());
  desc.parent = kRootUiId;
  // Full-viewport overlay anchor. Draw is immediate-mode raylib (2K reference
  // scaled by UISystem::State.scaleFactor), so the node is a declarative root
  // for future host-driven layout; it always spans the whole viewport.
  desc.layout.kind = UiLayoutKind::Overlay;
  desc.layout.width = UiLength::Fraction(1.0f);
  desc.layout.height = UiLength::Fraction(1.0f);
  desc.layout.horizontalAlignment = UiAlignment::Start;
  desc.layout.verticalAlignment = UiAlignment::Start;
  desc.visible = true;
  // The hotbar must never intercept the gameplay mouse.
  desc.hitTestVisible = false;
  desc.capturePointer = false;
  desc.focusable = false;
  desc.captureKeyboard = false;
  desc.acceptsText = false;
  desc.modal = false;
  desc.zIndex = static_cast<std::int32_t>(UiDrawLayer::Hud);
  desc.customPainter = kInvalidUiResourceId;

  if (m_runtime.CreateNode(desc)) {
    m_rootNodeId = desc.id;
  } else {
    m_rootNodeId = kInvalidUiId;
  }
}

UIDragSession& SkillHotbarController::DragSession() noexcept {
  return m_uiHost ? m_uiHost->DragSession() : m_localDragSession;
}

void SkillHotbarController::EnterGameplay() {
  m_inGameplay = true;
  m_hasPlayerData = false;
  SetVisible(true);
}

void SkillHotbarController::LeaveGameplay() {
  m_inGameplay = false;
  m_hasPlayerData = false;
  SetVisible(false);
}

void SkillHotbarController::SetVisible(bool visible) {
  m_visible = visible;
  if (m_rootNodeId != kInvalidUiId) {
    (void)m_runtime.SetNodeVisible(m_rootNodeId, visible);
  }
}

void SkillHotbarController::Update(entt::registry& registry) {
  m_hasPlayerData = false;
  // Keep the same registry queries as Draw (player data source parity).
  auto view = registry.view<PlayerTag, ActiveSkillsComponent,
                            ActiveEffectsComponent, CombatStats>();
  if (view.begin() == view.end()) {
    return;
  }
  m_hasPlayerData = true;
}

void SkillHotbarController::Draw(entt::registry& registry) {
  DrawHotbar(registry);
  DrawBuffStrip(registry);
}

bool SkillHotbarController::IsVisible() const noexcept { return m_visible; }

bool SkillHotbarController::IsInGameplay() const noexcept {
  return m_inGameplay;
}

bool SkillHotbarController::HasPlayerData() const noexcept {
  return m_hasPlayerData;
}

UiId SkillHotbarController::NodeId() const noexcept { return m_rootNodeId; }

void SkillHotbarController::DrawHotbar(entt::registry& registry) {
  auto view = registry.view<PlayerTag, ActiveSkillsComponent, CombatStats>();
  if (view.begin() == view.end())
    return;

  entt::entity player = view.front();
  const auto &active = view.get<ActiveSkillsComponent>(player);
  const auto &stats = view.get<CombatStats>(player);

  float slotSize = 54.0f;
  float padding = 8.0f;
  float totalW = (slotSize * 5) + (padding * 4);

  // Logic Position: Bottom Center
  float startX = (UI_REF_WIDTH - totalW) / 2.0f;
  float startY = UI_REF_HEIGHT - slotSize - 20.0f;

  const char *labels[] = {"Q", "W", "E", "R", "RMB"};

  for (int i = 0; i < 5; ++i) {
    const auto &slot = active.slots[i];
    float x = startX + i * (slotSize + padding);
    float y = startY;

    Texture2D icon = {0};
    float cooldownRatio = 0.0f;
    float manaCost = 0.0f;
    int maxCharges = 1;
    bool hasEnoughMana = true;

    if (slot.id != 0) {
      const auto *skillData = SkillRegistry::Get().GetSkill(slot.id);
      if (skillData) {
        if (skillData->icon_id != 0) {
          icon = AssetLoadingSystem::GetTexture(skillData->icon_id);
        }

        manaCost = skillData->mana_cost;
        maxCharges = skillData->max_charges;
        hasEnoughMana = stats.mana >= manaCost;

        if (skillData->cooldown > 0) {
          cooldownRatio =
              std::clamp(slot.cooldown / skillData->cooldown, 0.0f, 1.0f);
        }
      }
    }

    bool isHovered = CheckCollisionPointRec(UISystem::GetMousePositionLogic(),
                                            {x, y, slotSize, slotSize});
    bool isPressed = false;
    if (i == 0)
      isPressed = IsKeyDown(KEY_Q);
    else if (i == 1)
      isPressed = IsKeyDown(KEY_W);
    else if (i == 2)
      isPressed = IsKeyDown(KEY_E);
    else if (i == 3)
      isPressed = IsKeyDown(KEY_R);
    else if (i == 4)
      isPressed = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);

    if (isHovered) {
      // U7 group 6-B: hover routes through the hosted tooltip controller
      // (was the legacy hovered-skill-slot write).
      if (m_tooltip) {
        m_tooltip->SetHoveredSkillSlot(i);
      }
      // U8 收尾: mouse-over-UI 门控经 host 实例成员（原 State.isMouseOverUI）。
      if (m_uiHost) {
        m_uiHost->SetMouseOverUI(true);
      }

      // Drop logic
      UIDragSession& drag = DragSession();
      if (drag.isDraggingSkill &&
          IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        auto *activePtr = registry.try_get<ActiveSkillsComponent>(player);
        if (activePtr) {
          activePtr->slots[i].id = drag.draggedSkillId;
          LOG_INFO("Assigned skill {} to hotbar slot {}", drag.draggedSkillId,
                   i);
        }
        drag.isDraggingSkill = false;
        drag.draggedSkillId = INVALID_SKILL_ID;
      }

      // Right-click context menu
      // U8 收尾: 技能右键菜单经 host → overlay 实例（原 State 五字段写）。
      if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        if (m_uiHost) {
          m_uiHost->OpenSkillContextMenu(i);
        }
      }
    }

    UIRenderer::DrawSkillSlot(
        UISystem::GetFont(), x, y, slotSize, icon, labels[i],
        cooldownRatio, slot.cooldown, manaCost, slot.current_charges,
        maxCharges, hasEnoughMana, isHovered, isPressed, 0.8f);
  }
}

void SkillHotbarController::DrawBuffStrip(entt::registry& registry) {
  auto view = registry.view<PlayerTag, ActiveEffectsComponent>();
  if (view.begin() == view.end())
    return;

  entt::entity player = view.front();
  const auto &effects = view.get<ActiveEffectsComponent>(player);
  if (effects.effects.empty())
    return;

  // --- Metrics (Sync with PlayerHUD.cpp) ---
  float slotSize = 54.0f;
  float hotbarPadding = 8.0f;
  float hotbarW = (slotSize * 5) + (hotbarPadding * 4);
  float hotbarLeft = (UI_REF_WIDTH - hotbarW) / 2.0f;
  float hotbarRight = hotbarLeft + hotbarW;

  float barWidth = 450.0f;
  float barMargin = 50.0f;
  float barTopY = UI_REF_HEIGHT - 30.0f - 28.0f;

  float hpLeftX = hotbarLeft - barMargin - barWidth;
  float manaLeftX = hotbarRight + barMargin;

  // Buff Metrics
  float iconSize = 40.0f; // Slightly larger for better visibility
  float padding = 4.0f;
  float yOffset = 10.0f; // Space between bar and icons

  int maxPerRow = (int)std::floor((barWidth + padding) / (iconSize + padding));
  if (maxPerRow < 1)
    maxPerRow = 1;

  int currentBuffs = 0;
  int currentDebuffs = 0;

  for (int i = 0; i < (int)effects.effects.size(); ++i) {
    const auto &effect = effects.effects[i];
    bool isDebuff = effect.is_debuff;

    int count = isDebuff ? currentDebuffs++ : currentBuffs++;
    float startX = isDebuff ? manaLeftX : hpLeftX;

    int row = count / maxPerRow;
    int col = count % maxPerRow;

    float x = startX + col * (iconSize + padding);
    float y = barTopY - yOffset - (row + 1) * (iconSize + padding);

    const auto &visual = BuffRegistry::GetVisualData(effect.type);
    Texture2D icon = {0};
    if (visual.icon_asset) {
      icon = AssetLoadingSystem::GetTexture(visual.icon_asset->id);
    }
    const char *iconText = visual.icon_text.c_str();

    float ratio = 0.0f;
    if (effect.duration > 0) {
      ratio = std::clamp(effect.remaining / effect.duration, 0.0f, 1.0f);
    }

    UIRenderer::DrawBuffIcon(UISystem::GetFont(), x, y, iconSize, icon,
                             iconText, ratio, effect.stacks, isDebuff, 0.9f);

    if (CheckCollisionPointRec(UISystem::GetMousePositionLogic(),
                               {x, y, iconSize, iconSize})) {
      // U7 group 6-B: hover routes through the hosted tooltip controller
      // (was the legacy hovered-buff-index write).
      if (m_tooltip) {
        m_tooltip->SetHoveredBuff(i);
      }
      // U8 收尾: mouse-over-UI 门控经 host 实例成员（原 State.isMouseOverUI）。
      if (m_uiHost) {
        m_uiHost->SetMouseOverUI(true);
      }
    }
  }
}

} // namespace NoMoreDay::ui
