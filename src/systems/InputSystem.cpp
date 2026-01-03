#include "InputSystem.hpp"
#include "../components/Common.hpp"
#include "../components/AstrolabeUIComponent.hpp"
#include "raylib.h"
#include "UISystem.hpp" // For UI state check
#include "UIAstrolabe.hpp"
#include "SkillSystem.hpp"

using namespace NoMoreDay;

// --- Click-to-move state ---
// 这是一个适用于单人游戏的简单解决方案。
// 更好的方法是在玩家实体上使用一个 MovementTargetComponent。
static bool s_hasMovementTarget = false;
static Vector2 s_movementTarget = {0.0f, 0.0f};

void InputSystem::update(entt::registry &registry, const Camera2D &camera)
{
    auto view = registry.view<PlayerTag, InputComponent>();

    for (auto entity : view)
    {
        // Check if Astrolabe is open
        if (UIAstrolabe::IsVisible(registry, entity))
        {
            // Block all input if Astrolabe UI is fully open
            // We might allow closing via ESC/N (handled in UISystem::Update)
            // But movement/attack should be blocked.
            auto &input = view.get<InputComponent>(entity);
            input.moveX = 0;
            input.moveY = 0;
            input.attack = false;
            input.dash = false;
            s_hasMovementTarget = false; // 同时取消移动
            continue;
        }

        auto &input = view.get<InputComponent>(entity);

        // 重置
        input.moveX = 0.0f;
        input.moveY = 0.0f;
        input.attack = false; // 确保每帧重置攻击指令

        // 动作
        // Only allow mouse actions if not hovering over UI
        if (!UISystem::State.isMouseOverUI)
        {

            // 在鼠标左键按下或按住时更新移动目标
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                s_movementTarget = GetScreenToWorld2D(GetMousePosition(), camera);
                s_hasMovementTarget = true;
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
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);

            // 技能 - 同样会取消移动
            if (IsKeyPressed(KEY_Q))
            {
                SkillSystem::TryCast(registry, entity, 0, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsKeyPressed(KEY_W))
            {
                SkillSystem::TryCast(registry, entity, 1, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsKeyPressed(KEY_E))
            {
                SkillSystem::TryCast(registry, entity, 2, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsKeyPressed(KEY_R))
            {
                SkillSystem::TryCast(registry, entity, 3, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            {
                SkillSystem::TryCast(registry, entity, 4, mouseWorld);
                s_hasMovementTarget = false;
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
    }
}
