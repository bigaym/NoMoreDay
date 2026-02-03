#include "game/systems/ui/AstrolabeRenderer.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/skill/TalentLayoutService.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay {

Shader AstrolabeRenderer::s_shGalaxy = {0};
bool AstrolabeRenderer::s_initialized = false;

void AstrolabeRenderer::Init(Shader galaxyShader) {
    s_shGalaxy = galaxyShader;
    s_initialized = true;
}

void AstrolabeRenderer::Unload() {
    s_initialized = false;
}

float AstrolabeRenderer::getNodeRadius(TalentNodeType type) {
    using namespace Constants::Astrolabe;
    switch (type) {
        case TalentNodeType::Minor: return NODE_RADIUS_MINOR;
        case TalentNodeType::Major: return NODE_RADIUS_MAJOR;
        case TalentNodeType::Core:  return NODE_RADIUS_CORE;
        default: return NODE_RADIUS_MINOR;
    }
}

void AstrolabeRenderer::Draw(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId) {
    BeginMode2D(view.camera);
    
    DrawBackground(view);
    DrawOrbits(view);
    DrawProfessionStars(graph, view, comp);
    DrawNodes(graph, view, comp, hoveredNodeId);
    
    EndMode2D();
}

void AstrolabeRenderer::DrawBackground(const AstrolabeView& view) {
    using namespace Constants::Astrolabe;
    if (s_initialized) {
        // Set Uniforms
        int locTime = GetShaderLocation(s_shGalaxy, "uTime");
        int locRes = GetShaderLocation(s_shGalaxy, "uResolution");
        int locOffset = GetShaderLocation(s_shGalaxy, "uOffset");
        int locZoom = GetShaderLocation(s_shGalaxy, "uZoom");
        int locCenter = GetShaderLocation(s_shGalaxy, "uGalaxyCenter");
        int locScale = GetShaderLocation(s_shGalaxy, "uGalaxyScale");

        SetShaderValue(s_shGalaxy, locTime, &view.time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(s_shGalaxy, locRes, &view.resolution, SHADER_UNIFORM_VEC2);
        SetShaderValue(s_shGalaxy, locOffset, &view.camera.target, SHADER_UNIFORM_VEC2);
        SetShaderValue(s_shGalaxy, locZoom, &view.camera.zoom, SHADER_UNIFORM_FLOAT);
        
        Vector2 center = { GALAXY_CENTER_X, GALAXY_CENTER_Y };
        float scale = GALAXY_SCALE;
        SetShaderValue(s_shGalaxy, locCenter, &center, SHADER_UNIFORM_VEC2);
        SetShaderValue(s_shGalaxy, locScale, &scale, SHADER_UNIFORM_FLOAT);

        BeginShaderMode(s_shGalaxy);
        
        Vector2 tl = GetScreenToWorld2D({0, 0}, view.camera);
        Vector2 br = GetScreenToWorld2D(view.resolution, view.camera);
        // Expand slightly to cover rotation/zoom edges if needed, but World to Screen handles view
        // The shader uses fragCoord + camera uniforms to reconstruct world pos, so drawing a fullscreen quad is safest
        // But DrawRectangle takes World coordinates in Mode2D
        
        // Actually, for a fullscreen shader effect that depends on screen coords -> world coords,
        // we essentially want to cover the visible world area.
        DrawRectangle(tl.x - 100, tl.y - 100, (br.x - tl.x) + 200, (br.y - tl.y) + 200, BLACK);
        
        EndShaderMode();
    } else {
        ClearBackground(BLACK);
    }
}

void AstrolabeRenderer::DrawOrbits(const AstrolabeView& view) {
    using namespace Constants::Astrolabe;
    Color orbitColor = Fade(WHITE, 0.15f * view.alpha);
    
    // Draw Orbits
    DrawCircleLinesV({0, 0}, ORBIT_R1, orbitColor);
    DrawCircleLinesV({0, 0}, ORBIT_R2, orbitColor);
    DrawCircleLinesV({0, 0}, ORBIT_R3, orbitColor);
    DrawCircleLinesV({0, 0}, ORBIT_R4, orbitColor);
    
    // Draw Sector Dividers
    Color sectorColor = Fade(GOLD, 0.2f * view.alpha);
    for(int i=0; i<PROFESSION_COUNT; ++i) {
        float angle = TalentLayoutService::getSectorCenterAngle((ProfessionID)i) + 30.0f; // Boundary (+30 from center)
        float rad = angle * DEG2RAD;
        Vector2 end = { cos(rad) * ORBIT_R4 * 1.3f, sin(rad) * ORBIT_R4 * 1.3f };
        DrawLineEx({0,0}, end, 2.0f, sectorColor);
    }
}

void AstrolabeRenderer::DrawProfessionStars(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp) {
    using namespace Constants::Astrolabe;
    for (const auto& star : graph.professionStars) {
        Color color = GRAY;
        bool pulse = false;
        bool ray = false;
        
        if (comp && comp->hasVow()) {
            if (comp->isMainProfession(star.profession)) {
                color = GOLD;
                ray = true;
            } else {
                color = Fade(DARKPURPLE, 0.7f); // Sealed
            }
        } else {
            color = Fade(GOLD, 0.5f); // Available to vow
            pulse = true;
        }
        
        float r = PROFESSION_STAR_RADIUS;
        if (pulse) {
            float p = 1.0f + 0.1f * sinf(view.time * 2.0f);
            DrawCircle(star.x, star.y, r * p, Fade(color, 0.3f * view.alpha));
        }
        
        if (ray) {
             DrawCircleGradient(star.x, star.y, r * 1.5f, Fade(GOLD, 0.5f * view.alpha), Fade(GOLD, 0.0f));
        }
        
        DrawCircle(star.x, star.y, r, Fade(color, view.alpha));
        
        if (comp && comp->hasVow() && !comp->isMainProfession(star.profession)) {
             DrawRing({star.x, star.y}, r, r + 2, 0, 360, 0, Fade(PURPLE, 0.8f * view.alpha));
        }
    }
}

void AstrolabeRenderer::DrawNodes(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId) {
    using namespace Constants::Astrolabe;
    for (const auto& [id, node] : graph.nodes) {
        auto status = AstrolabeSystem::NodeStatus::Locked;
        if (comp) {
            status = AstrolabeSystem::getNodeStatus(graph, *comp, id);
        }
        
        float r = getNodeRadius(node.type);
        if (id == hoveredNodeId) r *= 1.2f;
        
        switch (status) {
            case AstrolabeSystem::NodeStatus::Locked:
                DrawCircle(node.x, node.y, r, Fade(GRAY, 0.25f * view.alpha));
                break;
                
            case AstrolabeSystem::NodeStatus::Available: {
                // Amber pulse
                float pulse = 0.6f + 0.4f * sinf(view.time * 3.0f);
                DrawCircle(node.x, node.y, r * 1.3f, Fade(GOLD, 0.2f * pulse * view.alpha));
                DrawCircle(node.x, node.y, r, Fade(ORANGE, view.alpha));
                break;
            }
                
            case AstrolabeSystem::NodeStatus::Activated:
                DrawCircle(node.x, node.y, r, Fade(SKYBLUE, view.alpha));
                // Progress ring
                if (comp) {
                    float progress = (float)comp->getNodePoints(id) / node.maxPoints;
                    DrawRing({node.x, node.y}, r+2, r+5, 0, 360 * progress, 32, Fade(SKYBLUE, view.alpha));
                }
                break;
                
            case AstrolabeSystem::NodeStatus::FullyActivated:
                DrawCircle(node.x, node.y, r * 1.1f, Fade(GOLD, 0.5f * view.alpha)); // Outer glow
                DrawCircle(node.x, node.y, r, Fade(GOLD, view.alpha));
                break;
                
            case AstrolabeSystem::NodeStatus::Sealed:
                DrawCircle(node.x, node.y, r, Fade(DARKPURPLE, 0.6f * view.alpha));
                break;
        }
    }
}

} // namespace NoMoreDay