#include "engine/input/InputSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "raylib.h"
#include "game/systems/ui/UISystem.hpp" // For UI state check
#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay {

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
        // Check if Astrolabe or Skill Tree is open
        if (UIAstrolabe::IsVisible(registry, entity) || UISystem::IsSkillTreeVisible(registry, entity))
        {
            // Block all input if UI is fully open
            auto &input = view.get<InputComponent>(entity);
            input.moveX = 0;
            input.moveY = 0;
            input.attack = false;
            input.dash = false;
            s_hasMovementTarget = false; 
            continue;
        }

        auto &input = view.get<InputComponent>(entity);
        
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
        // Only allow mouse actions if not hovering over UI
        if (!UISystem::State.isMouseOverUI)
        {
            if (!isRooted) {
                // 在鼠标左键按下或按住时更新移动目标
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    s_movementTarget = GetScreenToWorld2D(GetMousePosition(), camera);
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
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);

            // 技能 - 同样会取消移动
            if (IsKeyDown(KEY_Q))
            {
                SkillSystem::HandleSkillInput(registry, entity, 0, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsKeyDown(KEY_W))
            {
                SkillSystem::HandleSkillInput(registry, entity, 1, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsKeyDown(KEY_E))
            {
                SkillSystem::HandleSkillInput(registry, entity, 2, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsKeyDown(KEY_R))
            {
                SkillSystem::HandleSkillInput(registry, entity, 3, mouseWorld);
                s_hasMovementTarget = false;
            }
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            {
                SkillSystem::HandleSkillInput(registry, entity, 4, mouseWorld);
                s_hasMovementTarget = false;
            }

            // Real-time update for channeling skills: ensure the skill target follows mouse cursor
            if (auto* chan = registry.try_get<ChannelingComponent>(entity)) {
                chan->target_pos = mouseWorld;
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

} // namespace NoMoreDay
