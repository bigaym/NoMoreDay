#pragma once
#include "raylib.h"
#include "game/data/TalentData.hpp"
#include <set>

namespace NoMoreDay {

struct AstrolabeView {
    Camera2D camera;
    float alpha = 1.0f;
    Vector2 resolution = {0, 0};
    float time = 0.0f;
};

class AstrolabeRenderer {
public:
    static void Draw(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes);
    
    // Shader management
    static void Init(Shader galaxyShader);
    static void Unload();

private:
    static void DrawBackground(const AstrolabeView& view);
    static void DrawConnections(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes);
    static void DrawStars(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes);
    
    static Shader s_shGalaxy;
    static bool s_initialized;
};

} // namespace NoMoreDay
