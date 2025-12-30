#pragma once

namespace NoMoreDay {

    enum class UIAnimationType {
        Fade,
        Scale,
        Move,
        Color
    };

    enum class EasingType {
        Linear,
        EaseIn,
        EaseOut,
        EaseInOut
    };

    struct UIAnimationComponent {
        float timer = 0.0f;
        float duration = 0.2f;
        
        float startValue = 0.0f;
        float targetValue = 0.0f;
        float currentValue = 0.0f;

        UIAnimationType type = UIAnimationType::Fade;
        EasingType easing = EasingType::Linear;
        
        bool active = false;
        bool isReverse = false;
        bool autoDisable = true;
    };

}
