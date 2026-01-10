#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>
#include <atomic>
#include "raylib.h"
#include "../components/Common.hpp"

namespace NoMoreDay {

struct Popup {
    Vector2 position;
    Vector2 velocity;
    float amount;
    float timer;
    float lifeTime;
    Color color;
    bool isCrit;
    bool isStatus;
    char text[16];
    int textWidth = -1;
    bool active = false;
};

class DamagePopupManager {
public:
    static constexpr int MAX_POPUPS = 2048;

    static DamagePopupManager& Get() {
        static DamagePopupManager instance;
        return instance;
    }

    void Init() {
        popups.resize(MAX_POPUPS);
        nextIndex = 0;
    }

    void Emit(Vector2 pos, float amount, bool isCrit, Color color, bool isStatus = false, const std::string& statusText = "") {
        int index = nextIndex.fetch_add(1, std::memory_order_relaxed) % MAX_POPUPS;
        auto& p = popups[index];
        
        p.position = { pos.x + GetRandomValue(-15, 15), pos.y - 20.0f + GetRandomValue(-10, 5) };
        p.velocity = { (float)GetRandomValue(-40, 40), -150.0f };
        if (isStatus) p.velocity.y = -60.0f;
        
        p.amount = amount;
        p.timer = 0.0f;
        p.lifeTime = isStatus ? 1.2f : 1.0f;
        p.color = color;
        p.isCrit = isCrit;
        p.isStatus = isStatus;
        
        if (isStatus) {
            strncpy(p.text, statusText.c_str(), 15);
            p.text[15] = '\0';
        } else {
            snprintf(p.text, 16, "%.0f", amount);
        }
        p.textWidth = -1;
        p.active = true;
    }

    void Update(float dt) {
        for (auto& p : popups) {
            if (!p.active) continue;
            
            p.timer += dt;
            if (p.timer >= p.lifeTime) {
                p.active = false;
                continue;
            }

            p.position.x += p.velocity.x * dt;
            p.position.y += p.velocity.y * dt;
            p.velocity.y += 350.0f * dt;
        }
    }

    void Draw() {
        for (auto& p : popups) {
            if (!p.active) continue;

            float alpha = 1.0f - (p.timer / p.lifeTime);
            alpha = alpha * alpha; 
            Color col = ColorAlpha(p.color, alpha);
            float scale = p.isCrit ? (1.2f + 0.3f * sinf(p.timer * 20.0f)) : 1.0f;
            int fontSize = (int)(20 * scale);
            
            if (p.textWidth == -1) {
                p.textWidth = MeasureText(p.text, fontSize);
            }
            DrawText(p.text, (int)p.position.x - p.textWidth/2, (int)p.position.y, fontSize, col);
        }
    }

private:
    std::vector<Popup> popups;
    std::atomic<int> nextIndex{0};
    DamagePopupManager() { Init(); }
};

} // namespace NoMoreDay
