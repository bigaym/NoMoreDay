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

// Instance renderer for the astrolabe panel.
//
// U7 cleanup: converted from a static class to an instance type so the UI
// system keeps no static mutable rendering state (design invariant 4). All
// shader/texture/cache state is an instance member; resources are released by
// Unload() and by the destructor (idempotent, and skipped once the GL context
// is gone). The class is non-copyable; AstrolabeController owns one instance.
class AstrolabeRenderer {
public:
    AstrolabeRenderer() = default;
    ~AstrolabeRenderer();

    AstrolabeRenderer(const AstrolabeRenderer&) = delete;
    AstrolabeRenderer& operator=(const AstrolabeRenderer&) = delete;

    void Draw(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId = 0);
    
    // Shader management
    void Init(Shader galaxyShader, Shader nodeShader);
    void Unload();
    
    float getNodeRadius(TalentNodeType type);

    void DrawConnections(const TalentGraph& graph, const AstrolabeComponent* comp, float alpha);

private:
    void DrawBackground(const AstrolabeView& view);
    void DrawOrbits(const AstrolabeView& view);
    void DrawNodes(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId);
    void DrawProfessionStars(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp);
    
    Shader m_shGalaxy = {0};
    Shader m_shNode = {0};
    Texture2D m_whitePixel = {0}; // 1x1 white texture for UV-correct drawing
    RenderTexture2D m_galaxyCache = {0}; // Half-resolution galaxy background cache
    Vector2 m_galaxyCacheRes = {0, 0};
    bool m_galaxyCacheValid = false;
    bool m_initialized = false;

    // Cached galaxy shader uniform locations
    int m_locGalaxyTime = -1;
    int m_locGalaxyResolution = -1;
    int m_locGalaxyOffset = -1;
    int m_locGalaxyZoom = -1;
    int m_locGalaxyCameraOffset = -1;
    int m_locGalaxyCenter = -1;
    int m_locGalaxyScale = -1;
    int m_locGalaxyQualityTier = -1;
    int m_locGalaxyRenderScale = -1;
    int m_prevGalaxyQualityTier = -1;
};

} // namespace NoMoreDay
