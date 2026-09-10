#include "game/application/input/InputSystem.hpp"
#include <cmath>
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/AstrolabeUIComponent.hpp"
#include "game/foundation/components/DeliveryArchetypes.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "raylib.h"
#include "game/systems/skill/SkillSystem.hpp"
#include "engine/render/CoordSystem.hpp"

namespace NoMoreDay {

namespace {

// 技能7 (心剑·无影) 的 id 与引导相关专精节点号（整数常量，避免热路径字符串比较）
constexpr uint32_t kMindBladeSkillId = 7u;
constexpr uint32_t kMindBladeStepWithShadowNode = 732u; // 步影随行：引导时可微步移动
constexpr uint32_t kMindBladeSpiritWalkNode = 734u;     // 神游脱战：位移键打断并瞬移

// 实体是否正在引导技能7
bool IsChannelingMindBlade(entt::registry &registry, entt::entity entity) {
    const auto *chan = registry.try_get<ChannelingComponent>(entity);
    return chan != nullptr && chan->skill_id == kMindBladeSkillId;
}

// 实体在技能7专精树上是否已投入指定节点
bool HasMindBladeNode(entt::registry &registry, entt::entity entity,
                      uint32_t node_id) {
    const auto *active = registry.try_get<ActiveSkillsComponent>(entity);
    if (!active)
        return false;
    for (const auto &spec : active->specialized_slots) {
        if (spec.skill_id == kMindBladeSkillId) {
            auto it = spec.allocated_points.find(node_id);
            return it != spec.allocated_points.end() && it->second > 0;
        }
    }
    return false;
}

} // namespace

void InputSystem::update(entt::registry &registry, const Camera2D &camera,
                         const NoMoreDay::ui::UiInputCapture &capture)
{
    auto view = registry.view<PlayerTag, InputComponent>();

    for (auto entity : view)
    {
        // Block all gameplay input while a modal, text field or keyboard-
        // capturing UI surface owns input (U5: driven by UiInputCapture instead
        // of reading legacy UI static state).
        if (capture.modal || capture.text || capture.keyboard)
        {
            // Block all input if UI is fully open or User is typing
            auto &input = view.get<InputComponent>(entity);
            input.moveX = 0;
            input.moveY = 0;
            input.attack = false;
            input.dash = false;
            s_hasMovementTarget = false; 
            continue;
        }

        auto &input = view.get<InputComponent>(entity);

        // 技能7引导态与专精节点判定（每帧一次，供按键分支与移动缩放复用）
        const bool channelingMindBlade = IsChannelingMindBlade(registry, entity);
        const bool hasSpiritWalkNode =
            channelingMindBlade && HasMindBladeNode(registry, entity, kMindBladeSpiritWalkNode);
        // 732 微步：只要该节点激活即生效，不再因 734 神游节点被一票否决。
        // 若同时激活 734，引导期间新按下左键会登记一次打断请求（见下方分支），
        // 持续按住（引导开始前已按下）则保持微步位移。
        const bool microStepAllowed =
            channelingMindBlade &&
            HasMindBladeNode(registry, entity, kMindBladeStepWithShadowNode);
        // 仅在持续按住位移键（左键）时保留移动目标，避免残留目标导致自动漂移
        const bool keepMovementWhileChanneling =
            microStepAllowed && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        // Rooted Check
        bool isRooted = false;
        if (auto* stats = registry.try_get<PlayerStats>(entity)) {
            isRooted = stats->isRooted;
        }

        // 重置
        input.moveX = 0.0f;
        input.moveY = 0.0f;
        input.attack = false; // 确保每帧重置攻击指令

        // If rooted, movement inputs are ignored (but skills might still work)
        if (!isRooted) {
             // 在鼠标左键按下或按住时更新移动目标
             // ... existing mouse movement logic ...
             // But simpler to just inject the check below before applying movement
        }

        // 动作
        // Only allow mouse actions if the pointer is not captured by UI
        if (!capture.pointer)
        {
            if (!isRooted) {
                // 在鼠标左键按下或按住时更新移动目标
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    s_movementTarget = NoMoreDay::render::coord::ScenePixelToWorld(
                        NoMoreDay::render::coord::Camera2DTransform::From(camera),
                        GetMousePosition());
                    s_hasMovementTarget = true;
                }
            } else {
                s_hasMovementTarget = false; // Cancel pending movement if rooted
            }

            // 普通攻击绑定到 'A' 键
            input.attack = IsKeyPressed(KEY_A);
            if (input.attack)
            {
                s_hasMovementTarget = false; // 攻击时取消移动
            }

            input.dash = IsKeyDown(KEY_LEFT_SHIFT);
            if (input.dash)
            {
                s_hasMovementTarget = false; // 冲刺时取消移动
            }
            Vector2 mouseWorld = NoMoreDay::render::coord::ScenePixelToWorld(
                NoMoreDay::render::coord::Camera2DTransform::From(camera),
                GetMousePosition());

            // 神游脱战 (734): 引导技能7期间首次按下位移键登记一次打断请求，
            // 由交付层执行扣蓝、传送与引导收尾（本层不处理）。仅在按下帧触发，
            // 避免按住时每帧重复写中断标志。
            if (hasSpiritWalkNode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (auto *beam = registry.try_get<BeamChannelComponent>(entity))
                {
                    beam->interrupt_requested = true;
                    beam->interrupt_target = mouseWorld;
                }
            }

            // 微步移动 (732) 生效时不清理移动目标；其余情况保持原有一致行为
            const bool cancelMovementOnSkillKey = !keepMovementWhileChanneling;

            // 技能 - 同样会取消移动
            if (IsKeyDown(KEY_Q))
            {
                SkillSystem::HandleSkillInput(registry, entity, 0, mouseWorld);
                if (cancelMovementOnSkillKey)
                    s_hasMovementTarget = false;
            }
            if (IsKeyDown(KEY_W))
            {
                SkillSystem::HandleSkillInput(registry, entity, 1, mouseWorld);
                if (cancelMovementOnSkillKey)
                    s_hasMovementTarget = false;
            }
            if (IsKeyDown(KEY_E))
            {
                SkillSystem::HandleSkillInput(registry, entity, 2, mouseWorld);
                if (cancelMovementOnSkillKey)
                    s_hasMovementTarget = false;
            }
            if (IsKeyDown(KEY_R))
            {
                SkillSystem::HandleSkillInput(registry, entity, 3, mouseWorld);
                if (cancelMovementOnSkillKey)
                    s_hasMovementTarget = false;
            }
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            {
                SkillSystem::HandleSkillInput(registry, entity, 4, mouseWorld);
                if (cancelMovementOnSkillKey)
                    s_hasMovementTarget = false;
            }

            // Real-time update for channeling skills: ensure the skill target follows mouse cursor
            if (auto* chan = registry.try_get<ChannelingComponent>(entity)) {
                chan->target_pos = mouseWorld;
                // 技能7现代交付层若未自行维护光束目标，则同步引导目标
                if (chan->skill_id == kMindBladeSkillId) {
                    if (auto *beam = registry.try_get<BeamChannelComponent>(entity);
                        beam != nullptr && beam->skill_id == kMindBladeSkillId) {
                        beam->target_pos = mouseWorld;
                    }
                }
            }
        }
        else
        {
            input.dash = IsKeyDown(KEY_LEFT_SHIFT); // Allow keyboard dash even if mouse over UI (standard ARPG behavior)
        }

        // 处理朝向目标的移动
        if (s_hasMovementTarget)
        {
            auto *playerPos = registry.try_get<Position>(entity);
            if (playerPos)
            {
                float dx = s_movementTarget.x - playerPos->x;
                float dy = s_movementTarget.y - playerPos->y;
                float distSq = dx * dx + dy * dy;

                // 到达后停止
                if (distSq < 10.0f * 10.0f)
                { // 到达半径
                    s_hasMovementTarget = false;
                }
                else
                {
                    float dist = std::sqrt(distSq);
                    input.moveX = dx / dist;
                    input.moveY = dy / dist;
                }
            }
        }

        // 归一化向量以保持对角线移动的一致性
        if (input.moveX != 0 || input.moveY != 0)
        {
            float length = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
            input.moveX /= length;
            input.moveY /= length;
        }

        // 微步移动 (732): 引导技能7期间在归一化之后缩放最终方向，移速固定为正常移速的 30%
        if (microStepAllowed && (input.moveX != 0.0f || input.moveY != 0.0f))
        {
            const float moveSpeedScale =
                data::SkillMechanicsRegistry::Get().GetFloat(
                    kMindBladeSkillId, kMindBladeStepWithShadowNode,
                    "move_speed_scale", 0.30f);
            input.moveX *= moveSpeedScale;
            input.moveY *= moveSpeedScale;
        }
    }
}

} // namespace NoMoreDay
