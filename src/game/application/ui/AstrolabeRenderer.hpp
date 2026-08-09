#pragma once
#include "raylib.h"
#include "game/foundation/data/TalentData.hpp"
#include "game/foundation/components/Progression.hpp"

namespace NoMoreDay {

struct AstrolabeView {
    Camera2D camera;
    float alpha = 1.0f;
    Vector2 resolution = {0, 0};
    float time = 0.0f;
};

class AstrolabeRenderer {
public:
    static void Draw(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId = 0);
    
    // Shader management
    static void Init(Shader galaxyShader, Shader nodeShader);
    static void Unload();
    
    static float getNodeRadius(TalentNodeType type);

    static void DrawConnections(const TalentGraph& graph, const AstrolabeComponent* comp, float alpha);

private:
    static void DrawBackground(const AstrolabeView& view);
    static void DrawOrbits(const AstrolabeView& view);
    static void DrawNodes(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId);
    static void DrawProfessionStars(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp);
    
    static Shader s_shGalaxy;
    static Shader s_shNode;
    static Texture2D s_whitePixel; // 1x1 white texture for UV-correct drawing
    static bool s_initialized;
};

} // namespace NoMoreDay