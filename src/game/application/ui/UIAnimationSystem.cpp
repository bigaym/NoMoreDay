#include "game/application/ui/UIAnimationSystem.hpp"
#include "game/foundation/components/UIAnimationComponent.hpp"
#include <cmath>
#include <algorithm>

namespace NoMoreDay {

    float UIAnimationSystem::Ease(float t, float b, float c, float d, int type) {
        // t: current time, b: start value, c: change in value (end - start), d: duration
        float nt = std::clamp(t / d, 0.0f, 1.0f);
        
        switch (static_cast<EasingType>(type)) {
            case EasingType::EaseIn:
                return c * nt * nt + b;
            case EasingType::EaseOut:
                return -c * nt * (nt - 2.0f) + b;
            case EasingType::EaseInOut:
                nt *= 2.0f;
                if (nt < 1.0f) return c / 2.0f * nt * nt + b;
                nt -= 1.0f;
                return -c / 2.0f * (nt * (nt - 2.0f) - 1.0f) + b;
            case EasingType::Linear:
            default:
                return c * nt + b;
        }
    }

    void UIAnimationSystem::Update(entt::registry& registry, float dt) {
        auto view = registry.view<UIAnimationComponent>();
        for (auto entity : view) {
            auto& anim = view.get<UIAnimationComponent>(entity);
            if (!anim.active) continue;

            anim.timer += dt;
            if (anim.timer >= anim.duration) {
                anim.timer = anim.duration;
                anim.currentValue = anim.targetValue;
                if (anim.autoDisable) anim.active = false;
            } else {
                anim.currentValue = Ease(anim.timer, anim.startValue, anim.targetValue - anim.startValue, anim.duration, static_cast<int>(anim.easing));
            }
        }
    }

}
