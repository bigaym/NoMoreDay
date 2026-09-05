#include "game/application/states/GameplayStateInternal.hpp"

namespace NoMoreDay {

void GameplayState::EnsurePlayerHasDimensionalFragment(entt::registry& registry,
                                                       entt::entity player) {
  if (!registry.valid(player)) {
    return;
  }
  auto* inv = registry.try_get<InventoryComponent>(player);
  if (!inv) {
    return;
  }

  for (auto itemEntity : inv->items) {
    if (registry.valid(itemEntity) &&
        registry.all_of<NoMoreDay::MapFragmentComponent>(itemEntity)) {
      return;
    }
  }

  auto frag = registry.create();
  auto& item = registry.emplace<NoMoreDay::ItemComponent>(frag);
  item.name = "Empty Dimensional Fragment";
  item.rarity = NoMoreDay::Rarity::Common;
  item.type = NoMoreDay::ItemType::Material;

  registry.emplace<NoMoreDay::MapFragmentComponent>(frag);
  registry.emplace<NoMoreDay::MapFragmentTag>(frag);
  registry.emplace<PersistentTag>(frag);
  registry.emplace<LootTag>(frag);

  if (InventorySystem::pickUpItem(registry, player, frag)) {
    LOG_INFO("Granted default Dimensional Fragment to player.");
  } else {
    LOG_WARN("Inventory full! Could not grant default fragment.");
    registry.destroy(frag);
  }
}

void GameplayState::OpenDimensionalLevelSelect(entt::registry& registry,
                                               entt::entity player) {
  if (!registry.valid(player)) {
    auto players = registry.view<PlayerTag>();
    if (players.begin() != players.end()) {
      player = players.front();
    }
  }
  EnsurePlayerHasDimensionalFragment(registry, player);
  m_stateManager->PushState<DimensionalLevelSelectState>();
  LOG_INFO("Pushed DimensionalLevelSelectState");
}

void GameplayState::ClearActiveRiftForNewRun(entt::registry& registry) {
  if (!registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
    return;
  }
  auto& state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
  const int preservedBaseLevel = state.selectedBaseLevel;
  NoMoreDay::ClearRiftInstanceData(state);
  state.selectedBaseLevel = preservedBaseLevel;
}

bool GameplayState::HandleRiftDialogs(entt::registry& registry) {
  const Vector2 mouse = GetMousePosition();
  const bool mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

  if (m_showGateStartNewConfirmDialog) {
    Rectangle confirmRect{GetScreenWidth() * 0.5f - 180.0f,
                          GetScreenHeight() * 0.5f + 30.0f, 160.0f, 44.0f};
    Rectangle cancelRect{GetScreenWidth() * 0.5f + 20.0f,
                         GetScreenHeight() * 0.5f + 30.0f, 160.0f, 44.0f};

    if (mouseReleased && CheckCollisionPointRec(mouse, confirmRect)) {
      LOG_WARN("Player confirmed starting a new rift. Existing run will be destroyed.");
      ClearActiveRiftForNewRun(registry);
      OpenDimensionalLevelSelect(registry, m_gateDialogPlayer);
      m_showGateStartNewConfirmDialog = false;
      m_showGateResumeOrNewDialog = false;
      m_gateDialogPlayer = entt::null;
    } else if (mouseReleased &&
               (CheckCollisionPointRec(mouse, cancelRect) || IsKeyPressed(KEY_ESCAPE))) {
      LOG_INFO("Start New confirmation cancelled.");
      m_showGateStartNewConfirmDialog = false;
      m_showGateResumeOrNewDialog = true;
    }
    return true;
  }

  if (m_showGateResumeOrNewDialog) {
    Rectangle resumeRect{GetScreenWidth() * 0.5f - 180.0f,
                         GetScreenHeight() * 0.5f + 20.0f, 112.0f, 44.0f};
    Rectangle startNewRect{GetScreenWidth() * 0.5f - 54.0f,
                           GetScreenHeight() * 0.5f + 20.0f, 112.0f, 44.0f};
    Rectangle cancelRect{GetScreenWidth() * 0.5f + 72.0f,
                         GetScreenHeight() * 0.5f + 20.0f, 112.0f, 44.0f};

    if (mouseReleased && CheckCollisionPointRec(mouse, resumeRect)) {
      if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
        const auto& state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
        if (NoMoreDay::HasInProgressRift(state)) {
          LOG_INFO("Resuming active rift: biome={}, depth={}", static_cast<int>(state.biome),
                   state.currentDepth);
          if (m_context->sceneManager) {
            m_context->sceneManager->RequestTransition(state.biome, state.currentDepth, EntranceId::RiftResume);
          }
        } else {
          LOG_WARN("Resume clicked but no in-progress rift found. Falling back to new run.");
          OpenDimensionalLevelSelect(registry, m_gateDialogPlayer);
        }
      } else {
        OpenDimensionalLevelSelect(registry, m_gateDialogPlayer);
      }
      m_showGateResumeOrNewDialog = false;
      m_gateDialogPlayer = entt::null;
    } else if (mouseReleased && CheckCollisionPointRec(mouse, startNewRect)) {
      m_showGateResumeOrNewDialog = false;
      m_showGateStartNewConfirmDialog = true;
    } else if (mouseReleased &&
               (CheckCollisionPointRec(mouse, cancelRect) || IsKeyPressed(KEY_ESCAPE))) {
      LOG_INFO("Dimensional Gate action cancelled.");
      m_showGateResumeOrNewDialog = false;
      m_gateDialogPlayer = entt::null;
    }
    return true;
  }

  if (m_showRiftCompletedDialog) {
    Rectangle continueRect{GetScreenWidth() * 0.5f - 180.0f,
                           GetScreenHeight() * 0.5f + 20.0f, 170.0f, 44.0f};
    Rectangle townRect{GetScreenWidth() * 0.5f + 10.0f,
                       GetScreenHeight() * 0.5f + 20.0f, 170.0f, 44.0f};

    if (mouseReleased && CheckCollisionPointRec(mouse, continueRect)) {
      if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
        auto& state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
        const int preservedBaseLevel = state.selectedBaseLevel;
        NoMoreDay::ClearRiftInstanceData(state);
        state.selectedBaseLevel = preservedBaseLevel;
      }
      if (!registry.ctx().contains<RiftCompletionPromptState>()) {
        registry.ctx().emplace<RiftCompletionPromptState>();
      }
      registry.ctx().get<RiftCompletionPromptState>().isPending = false;
      m_showRiftCompletedDialog = false;
      LOG_INFO("Rift cycle completed. Entering MosaicEditorState for a new cycle.");
      m_stateManager->PushState<MosaicEditorState>();
    } else if (mouseReleased &&
               (CheckCollisionPointRec(mouse, townRect) || IsKeyPressed(KEY_ESCAPE))) {
      if (registry.ctx().contains<NoMoreDay::ActiveDimensionalState>()) {
        auto& state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
        const int preservedBaseLevel = state.selectedBaseLevel;
        NoMoreDay::ClearRiftInstanceData(state);
        state.selectedBaseLevel = preservedBaseLevel;
      }
      if (!registry.ctx().contains<RiftCompletionPromptState>()) {
        registry.ctx().emplace<RiftCompletionPromptState>();
      }
      registry.ctx().get<RiftCompletionPromptState>().isPending = false;
      m_showRiftCompletedDialog = false;
      if (m_context->sceneManager) {
        m_context->sceneManager->ClearOriginInfo();
        m_context->sceneManager->RequestTransition(BiomeID::Town, 1, EntranceId::RiftCompleteReturn);
      }
      LOG_INFO("Rift cycle completed and player chose return to town.");
    }
    return true;
  }

  return false;
}

void GameplayState::RenderRiftDialogs() {
  Font uiFont = UISystem::GetFont();

  auto drawSimpleButton = [&uiFont](const Rectangle& rect, const char* label, Color fill) {
    const Vector2 mouse = GetMousePosition();
    const bool hovered = CheckCollisionPointRec(mouse, rect);
    const Color tint = hovered ? ColorAlpha(fill, 0.85f) : fill;
    DrawRectangleRounded(rect, 0.2f, 4, tint);
    DrawRectangleRoundedLinesEx(rect, 0.2f, 4, 2.0f, RAYWHITE);
    const Vector2 textSize = MeasureTextEx(uiFont, label, 20.0f, 1.0f);
    DrawTextEx(uiFont, label,
               {rect.x + (rect.width - textSize.x) * 0.5f, rect.y + (rect.height - textSize.y) * 0.5f},
               20.0f, 1.0f, WHITE);
  };

  if (m_showGateResumeOrNewDialog) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 170});
    const Rectangle panel{GetScreenWidth() * 0.5f - 210.0f, GetScreenHeight() * 0.5f - 110.0f,
                          420.0f, 200.0f};
    DrawRectangleRounded(panel, 0.08f, 8, Color{28, 32, 44, 250});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, SKYBLUE);
    DrawTextEx(uiFont, "发现进行中的维度裂隙", {panel.x + 90.0f, panel.y + 30.0f},
               24.0f, 1.0f, WHITE);
    DrawTextEx(uiFont, "请选择继续当前裂隙或开始新裂隙", {panel.x + 70.0f, panel.y + 68.0f},
               20.0f, 1.0f, LIGHTGRAY);
    drawSimpleButton(Rectangle{panel.x + 30.0f, panel.y + 130.0f, 112.0f, 44.0f}, "继续", DARKGREEN);
    drawSimpleButton(Rectangle{panel.x + 154.0f, panel.y + 130.0f, 112.0f, 44.0f}, "新开", MAROON);
    drawSimpleButton(Rectangle{panel.x + 278.0f, panel.y + 130.0f, 112.0f, 44.0f}, "取消", DARKGRAY);
  }

  if (m_showGateStartNewConfirmDialog) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 180});
    const Rectangle panel{GetScreenWidth() * 0.5f - 230.0f, GetScreenHeight() * 0.5f - 110.0f,
                          460.0f, 210.0f};
    DrawRectangleRounded(panel, 0.08f, 8, Color{45, 28, 28, 250});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, ORANGE);
    DrawTextEx(uiFont, "确认开始新裂隙?", {panel.x + 130.0f, panel.y + 34.0f},
               28.0f, 1.0f, WHITE);
    DrawTextEx(uiFont, "当前裂隙进度将被销毁且无法恢复", {panel.x + 72.0f, panel.y + 78.0f},
               22.0f, 1.0f, GOLD);
    drawSimpleButton(Rectangle{panel.x + 50.0f, panel.y + 140.0f, 160.0f, 44.0f}, "确认新开", MAROON);
    drawSimpleButton(Rectangle{panel.x + 250.0f, panel.y + 140.0f, 160.0f, 44.0f}, "返回", DARKGRAY);
  }

  if (m_showRiftCompletedDialog) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 170});
    const Rectangle panel{GetScreenWidth() * 0.5f - 220.0f, GetScreenHeight() * 0.5f - 105.0f,
                          440.0f, 200.0f};
    DrawRectangleRounded(panel, 0.08f, 8, Color{25, 40, 35, 245});
    DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 2.0f, GREEN);
    DrawTextEx(uiFont, "本轮维度挑战已完成", {panel.x + 105.0f, panel.y + 34.0f},
               28.0f, 1.0f, WHITE);
    DrawTextEx(uiFont, "可继续重拼接下一轮，或返回城镇结算", {panel.x + 62.0f, panel.y + 74.0f},
               20.0f, 1.0f, LIGHTGRAY);
    drawSimpleButton(Rectangle{panel.x + 40.0f, panel.y + 130.0f, 170.0f, 44.0f}, "继续重拼接", DARKGREEN);
    drawSimpleButton(Rectangle{panel.x + 230.0f, panel.y + 130.0f, 170.0f, 44.0f}, "回城", DARKBLUE);
  }
}

void GameplayState::RenderMapAffixOverlay() {
  auto &registry = *m_context->registry;
  if (!registry.ctx().contains<NoMoreDay::ActiveDimensionalState>())
    return;

  const auto &state = registry.ctx().get<NoMoreDay::ActiveDimensionalState>();
  if (!state.isActive)
    return;

  // Draw Background
  float width = 350.0f;
  float height = 450.0f;
  float x = (GetScreenWidth() - width) / 2.0f;
  float y = (GetScreenHeight() - height) / 2.0f;

  DrawRectangleRounded(Rectangle{x, y, width, height}, 0.05f, 8,
                       ColorAlpha(BLACK, 0.85f));
  DrawRectangleRoundedLinesEx(Rectangle{x, y, width, height}, 0.05f, 8, 2.0f,
                              DARKGRAY);

  Font font = UISystem::GetFont();
  UIRenderer::DrawTextUI(font, "当前维度概览 (Map Modifiers)", x + 20, y + 20,
                         20, GOLD, 1.0f);

  float ly = y + 60;
  char buf[128];

  // 0. Depth Info
  utils::FormatToBuffer(buf, "当前深度 (Layer): {} / {}", state.currentDepth,
                        state.maxDepth);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 18, SKYBLUE, 1.0f);
  ly += 25;

  // 1. Difficulty
  utils::FormatToBuffer(buf, "难度系数 (DS): {}", state.difficultyScore);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 18, RED, 1.0f);
  ly += 25;

  // 1.1 Kill Counter
  utils::FormatToBuffer(buf, "击杀计数 (Kills): {}", state.killCounter);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 16, WHITE, 1.0f);
  ly += 25;

  // 2. Rewards
  utils::FormatToBuffer(buf, "物品寻宝率 (Rarity): +{:.0f}%",
                        state.calculatedRarity * 100.0f);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 16,
                         components::Colors::RARITY_LEGENDARY, 1.0f);
  ly += 20;

  utils::FormatToBuffer(buf, "物品数量 (Quantity): +{:.0f}%",
                        state.calculatedQuantity * 100.0f);
  UIRenderer::DrawTextUI(font, buf, x + 25, ly, 16,
                         components::Colors::RARITY_EPIC, 1.0f);
  ly += 25;

  DrawLine(x + 20, ly, x + width - 20, ly, GRAY);
  ly += 15;

  UIRenderer::DrawTextUI(font, "鎸戞垬璇嶇紑 (Active Challenges):", x + 25, ly,
                         16, LIGHTGRAY, 1.0f);
  ly += 25;

  // 3. Cached Aggregated Affixes
  for (const auto &agg : state.aggregatedAffixes) {
    std::string desc =
        MapAffixRegistry::FormatDescription(agg.type, agg.totalValue);

    Color color = WHITE;
    if (agg.category == MapAffixCategory::Debuff)
      color = RED;
    else if (agg.category == MapAffixCategory::Buff)
      color = GREEN;

    UIRenderer::DrawTextUI(font, desc.c_str(), x + 30, ly, 14, color, 1.0f);
    ly += 18;

    if (ly > y + height - 30) {
      UIRenderer::DrawTextUI(font, "...", x + 30, ly, 14, GRAY, 1.0f);
      break;
    }
  }
}

} // namespace NoMoreDay
