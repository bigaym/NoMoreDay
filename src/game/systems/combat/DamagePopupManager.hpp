#pragma once
#include <vector>
#include <string>
#include <atomic>
#include "raylib.h"

// Include OUTSIDE of namespace to avoid nesting issues
#include "engine/render/PopupRenderer.hpp"

namespace NoMoreDay {

class DamagePopupManager {
public:
    static DamagePopupManager& Get() {
        static DamagePopupManager instance;
        return instance;
    }

    void Init() {}

    void Emit(Vector2 pos, float amount, bool isCrit, Color color, bool isStatus = false, const std::string& statusText = "") {
        if (isStatus) {
            render::PopupRenderer::Get().EmitStatus(pos, statusText.c_str(), color);
        } else {
            render::PopupRenderer::Get().Emit(pos, (int)amount, isCrit, color);
        }
    }

    void Update(float dt) {
        // Now handled by PopupRenderer
    }

    void Draw(const Font& font) {
        // Now handled by PopupRenderer
    }

private:
    DamagePopupManager() {}
};

} // namespace NoMoreDay