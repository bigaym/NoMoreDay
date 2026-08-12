#pragma once
#include "game/application/ui/WorldUiFrame.hpp"
#include "game/foundation/SharedContext.hpp"
#include "engine/render/SIMDSpatialGrid.hpp"
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
  void SetContext(const NoMoreDay::SharedContext *context) {
    m_context = context;
  }
  void Init();
  void Shutdown();

  // U8 收尾：全局 UI 字体由组合根（Game）在 UI host 初始化后注入，替代
  // 原先对 UiShared::GlobalFont() 的逐帧读取（UiShared 静态槽已删除）。
  void SetFont(Font font) { m_font = font; }

  // Binds the frame-scoped world UI bridge (U8, plan §11): the write side of
  // the static UiShared slots (VisibleItemCache / HoveredItem) is replaced by
  // this frame object. Bound by the composition root (Game) in the integration
  // phase; when null the UIWorldPass skips all static-slot writes.
  void BindWorldUiFrame(NoMoreDay::ui::WorldUiFrame *frame) {
    m_worldFrame = frame;
  }

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
  // Frame-scoped world UI bridge (U8): written by ExecuteUIWorldPass, read by
  // GameUiHost. Null until bound by the composition root.
  NoMoreDay::ui::WorldUiFrame *m_worldFrame = nullptr;
  // Monotonic frame token source for WorldUiFrame::BeginFrame (U8, plan §11).
  uint64_t m_frameCounter = 0;
};

} // namespace NoMoreDay
