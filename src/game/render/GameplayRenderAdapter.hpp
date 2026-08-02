#pragma once
#include "game/SharedContext.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "engine/render/GameplayRenderHooks.hpp"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

class FogOfWarSystem;

namespace NoMoreDay {

/**
 * @brief Game 层 gameplay 绘制适配器。
 *
 * 承接 RenderSystem 中全部 Game 专属绘制：stash/sprite/BloodSea/Molten trail、
 * AttackEffect/VisualEffect switch、Projectile->GPUSkillEffect 提交、
 * ResistOverlay 绘制、DamagePopup CPU 弹字、loot label 收集/排序/重叠化解，
 * 以及 label/glyph/beam instance buffer 填充。Engine 仅通过
 * render::GameplayRenderHooks 回调驱动；shader/SSBO/instanced draw 基础设施
 * 保留在 Engine。
 */
class GameplayRenderAdapter : public render::GameplayRenderHooks {
public:
  // Shared Visibility Cache (moved from RenderSystem::VisibleItemCache).
  struct VisibleItemCache {
    struct ItemData {
      entt::entity entity;
      Rectangle worldRect; // World Space Bounds for Label
    };
    static std::vector<ItemData> visibleItems;
    static void Clear() { visibleItems.clear(); }
  };

  // Loot Label Spatial Optimization grid (moved from RenderSystem).
  static std::unique_ptr<systems::SIMDSpatialGrid> s_itemGrid;
  static bool s_itemGridDirty;

  void SetContext(const NoMoreDay::SharedContext *context) {
    m_context = context;
  }
  void Init();
  void Shutdown();

  // render::GameplayRenderHooks
  void onFrameData(render::GameplayRenderFrame &frame) override;
  void onScene(render::GameplayRenderFrame &frame) override;
  void onVFX(render::GameplayRenderFrame &frame) override;
  void onUIWorld(render::GameplayRenderFrame &frame) override;
  void onOccluders(render::GameplayRenderFrame &frame) override;
  void onLights(render::GameplayRenderFrame &frame) override;
  void onHeightField(render::GameplayRenderFrame &frame) override;
  void onLoot(render::GameplayRenderFrame &frame) override;
  void onEmissive(render::GameplayRenderFrame &frame) override;

private:
  void BuildFrameData(render::GameplayRenderFrame &frame);
  void ExecuteScenePass(render::GameplayRenderFrame &frame);
  void ExecuteVFXPass(render::GameplayRenderFrame &frame);
  void ExecuteUIWorldPass(render::GameplayRenderFrame &frame);

  const NoMoreDay::SharedContext *m_context = nullptr;
  float m_cameraZoom = 1.5f;
  float m_fontScale = 1.0f;
  Font m_font = {};
  Vector2 m_playerPos = {0.0f, 0.0f};
  bool m_hasPlayer = false;
  bool m_limitEnemyVision = false;
  const FogOfWarSystem *m_fogSystem = nullptr;
};

} // namespace NoMoreDay
