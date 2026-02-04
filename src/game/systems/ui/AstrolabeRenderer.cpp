#include "game/systems/ui/AstrolabeRenderer.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/skill/TalentLayoutService.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include <algorithm>

namespace NoMoreDay {

Shader AstrolabeRenderer::s_shGalaxy = {0};
Shader AstrolabeRenderer::s_shNode = {0};
bool AstrolabeRenderer::s_initialized = false;

void AstrolabeRenderer::Init(Shader galaxyShader, Shader nodeShader) {
    s_shGalaxy = galaxyShader;
    s_shNode = nodeShader;
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
    if (!s_initialized) return;

    int locTime = GetShaderLocation(s_shNode, "uTime");
    int locStatus = GetShaderLocation(s_shNode, "uStatus");
    int locProgress = GetShaderLocation(s_shNode, "uProgress");
    int locBaseColor = GetShaderLocation(s_shNode, "uBaseColor");
    
    // Sort nodes to minimize state changes? Not strictly necessary for < 100 nodes.
    // But we should use BeginShaderMode once if possible.
    // However, SetShaderValue needs to be called per node.
    // In Raylib, if we are not using instancing, we have to:
    // BeginShaderMode -> SetUniforms -> Draw -> EndShaderMode (or Flush).
    // Actually, calling SetShaderValue affects the currently active shader?
    // Raylib docs: SetShaderValue sets uniform in shader program.
    // So yes, we can BeginShaderMode, then loop (SetUniform, Draw).
    // Note: DrawRectangle creates vertices. Raylib batches them. 
    // If we change uniforms between Draw calls, we break the batch.
    // Raylib will flush the batch when uniforms change IF we were using rlgl directly or if it detects it.
    // But standard `SetShaderValue` modifies the program directly. If we have queued vertices, they might be drawn with the NEW uniform value if the draw call hasn't happened yet.
    // Raylib's `DrawRectangle` batches into `rlgl`. 
    // To ensure uniforms apply to the specific Draw call, we must force a batch flush (rlDrawRenderBatchActive) before changing uniforms.
    
    BeginShaderMode(s_shNode);
    SetShaderValue(s_shNode, locTime, &view.time, SHADER_UNIFORM_FLOAT);
    
    for (const auto& [id, node] : graph.nodes) {
        auto status = AstrolabeSystem::NodeStatus::Locked;
        if (comp) {
            status = AstrolabeSystem::getNodeStatus(graph, *comp, id);
        }
        
        float r = getNodeRadius(node.type);
        if (id == hoveredNodeId) r *= 1.2f; // Slight zoom on hover
        
        int statusInt = (int)status;
        float progress = 0.0f;
        if (comp) {
             progress = (float)comp->getNodePoints(id) / node.maxPoints;
        }
        
        Vector4 baseColor = {0.8f, 0.8f, 0.8f, 1.0f}; // Default Grey
        
        // Force flush before changing uniforms for this specific node
        rlDrawRenderBatchActive();
        
        SetShaderValue(s_shNode, locStatus, &statusInt, SHADER_UNIFORM_INT);
        SetShaderValue(s_shNode, locProgress, &progress, SHADER_UNIFORM_FLOAT);
        SetShaderValue(s_shNode, locBaseColor, &baseColor, SHADER_UNIFORM_VEC4);
        
        // Draw Quad centered at node.x, node.y with radius r
        // DrawRectangle takes top-left.
        // Size is 2*r.
        DrawRectangle(node.x - r, node.y - r, r * 2, r * 2, WHITE);
    }
    
    EndShaderMode();
}

} // namespace NoMoreDay