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
        BeginShaderMode(s_shGalaxy);
        // Set uniforms if needed
        // Draw quad covering view
        Vector2 tl = GetScreenToWorld2D({0, 0}, view.camera);
        Vector2 br = GetScreenToWorld2D(view.resolution, view.camera);
        DrawRectangle(tl.x, tl.y, br.x - tl.x, br.y - tl.y, BLACK);
        EndShaderMode();
    } else {
        ClearBackground(BLACK);
    }
}

void AstrolabeRenderer::DrawOrbits(const AstrolabeView& view) {
    using namespace Constants::Astrolabe;
    Color orbitColor = Fade(WHITE, 0.1f * view.alpha);
    
    // Draw Orbits
    DrawCircleLines(0, 0, ORBIT_R1, orbitColor);
    DrawCircleLines(0, 0, ORBIT_R2, orbitColor);
    DrawCircleLines(0, 0, ORBIT_R3, orbitColor);
    DrawCircleLines(0, 0, ORBIT_R4, orbitColor);
    
    // Draw Sector Dividers
    for(int i=0; i<6; ++i) {
        float angle = TalentLayoutService::getSectorCenterAngle((ProfessionID)i) + 30.0f; // Boundary (+30 from center)
        float rad = angle * DEG2RAD;
        Vector2 end = { cos(rad) * ORBIT_R4 * 1.2f, sin(rad) * ORBIT_R4 * 1.2f };
        DrawLineV({0,0}, end, orbitColor);
    }
}

void AstrolabeRenderer::DrawProfessionStars(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp) {
    using namespace Constants::Astrolabe;
    for (const auto& star : graph.professionStars) {
        Color color = GRAY;
        if (comp && comp->hasVow()) {
            if (comp->isMainProfession(star.profession)) color = GOLD;
            else color = DARKPURPLE;
        } else {
            color = Fade(GOLD, 0.5f); // Available to vow
        }
        
        DrawCircle(star.x, star.y, PROFESSION_STAR_RADIUS, Fade(color, view.alpha));
        
        // Simple label
        // DrawText(star.name_key.c_str(), star.x, star.y, 20, WHITE);
    }
}

void AstrolabeRenderer::DrawNodes(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp, uint32_t hoveredNodeId) {
    using namespace Constants::Astrolabe;
    for (const auto& [id, node] : graph.nodes) {
        auto status = AstrolabeSystem::NodeStatus::Locked;
        if (comp) {
            status = AstrolabeSystem::getNodeStatus(graph, *comp, id);
        }
        
        Color color = DARKGRAY;
        switch (status) {
            case AstrolabeSystem::NodeStatus::Locked: color = Fade(GRAY, 0.3f); break;
            case AstrolabeSystem::NodeStatus::Available: color = ORANGE; break;
            case AstrolabeSystem::NodeStatus::Activated: color = SKYBLUE; break;
            case AstrolabeSystem::NodeStatus::FullyActivated: color = GOLD; break;
            case AstrolabeSystem::NodeStatus::Sealed: color = DARKPURPLE; break;
        }
        
        float r = NODE_RADIUS_MINOR;
        if (node.type == TalentNodeType::Major) r = NODE_RADIUS_MAJOR;
        else if (node.type == TalentNodeType::Core) r = NODE_RADIUS_CORE;
        
        if (id == hoveredNodeId) r *= 1.2f;
        
        DrawCircle(node.x, node.y, r, Fade(color, view.alpha));
        
        // Progress ring
        if (status == AstrolabeSystem::NodeStatus::Activated && comp) {
            float progress = (float)comp->getNodePoints(id) / node.maxPoints;
            DrawRing({node.x, node.y}, r+2, r+4, 0, 360 * progress, 32, Fade(SKYBLUE, view.alpha));
        }
    }
}

} // namespace NoMoreDay