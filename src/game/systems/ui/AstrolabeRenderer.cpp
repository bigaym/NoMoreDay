#include "game/systems/ui/AstrolabeRenderer.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/data/TalentLayoutService.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>

namespace NoMoreDay {

Shader AstrolabeRenderer::s_shGalaxy = {0};
Shader AstrolabeRenderer::s_shNode = {0};
Texture2D AstrolabeRenderer::s_whitePixel = {0};
bool AstrolabeRenderer::s_initialized = false;
namespace {
int s_locGalaxyTime = -1;
int s_locGalaxyResolution = -1;
int s_locGalaxyOffset = -1;
int s_locGalaxyZoom = -1;
int s_locGalaxyCameraOffset = -1;
int s_locGalaxyCenter = -1;
int s_locGalaxyScale = -1;
int s_locGalaxyQualityTier = -1;
int s_prevGalaxyQualityTier = -1;
} // namespace

void AstrolabeRenderer::Init(Shader galaxyShader, Shader nodeShader) {
    s_shGalaxy = galaxyShader;
    s_shNode = nodeShader;
    
    // Create 1x1 white texture for UV-correct drawing
    Image img = GenImageColor(1, 1, WHITE);
    s_whitePixel = LoadTextureFromImage(img);
    UnloadImage(img);

    if (s_shGalaxy.id != 0) {
        s_locGalaxyTime = GetShaderLocation(s_shGalaxy, "uTime");
        s_locGalaxyResolution = GetShaderLocation(s_shGalaxy, "uResolution");
        s_locGalaxyOffset = GetShaderLocation(s_shGalaxy, "uOffset");
        s_locGalaxyZoom = GetShaderLocation(s_shGalaxy, "uZoom");
        s_locGalaxyCameraOffset = GetShaderLocation(s_shGalaxy, "uCameraOffset");
        s_locGalaxyCenter = GetShaderLocation(s_shGalaxy, "uGalaxyCenter");
        s_locGalaxyScale = GetShaderLocation(s_shGalaxy, "uGalaxyScale");
        s_locGalaxyQualityTier = GetShaderLocation(s_shGalaxy, "uQualityTier");
    }
    
    s_initialized = true;
}

void AstrolabeRenderer::Unload() {
    if (s_whitePixel.id != 0) {
        UnloadTexture(s_whitePixel);
        s_whitePixel = {0};
    }
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
    DrawConnections(graph, comp, view.alpha);
    DrawProfessionStars(graph, view, comp);
    DrawNodes(graph, view, comp, hoveredNodeId);
    
    EndMode2D();
}

void AstrolabeRenderer::DrawConnections(const TalentGraph& graph, const AstrolabeComponent* comp, float alpha) {
    if (alpha <= 0.0f) return;

    for (const auto& [id, node] : graph.nodes) {
        // Draw lines to prerequisites
        for (uint32_t parentId : node.prerequisites) {
            const AstrolabeTalentNode* parent = graph.findNode(parentId);
            if (parent) {
                Vector2 start = { parent->x, parent->y };
                Vector2 end = { node.x, node.y };
                
                bool unlocked = false;
                if (comp) {
                    auto status = AstrolabeSystem::getNodeStatus(graph, *comp, id);
                    unlocked = (status != AstrolabeSystem::NodeStatus::Locked && status != AstrolabeSystem::NodeStatus::Sealed);
                }
                
                Color color = unlocked ? GOLD : Fade(GOLD, 0.2f);
                DrawLineEx(start, end, 3.0f, Fade(color, alpha));
            }
        }
    }
}

void AstrolabeRenderer::DrawBackground(const AstrolabeView& view) {
    using namespace Constants::Astrolabe;
    if (s_initialized && s_shGalaxy.id > 0) {
        BeginShaderMode(s_shGalaxy);
        const int qualityTier = static_cast<int>(
            render::core::QualityTierManager::Get().GetTier());
        if (qualityTier != s_prevGalaxyQualityTier) {
            LOG_INFO("AstrolabeRenderer: Galaxy background quality tier={}",
                     qualityTier);
            s_prevGalaxyQualityTier = qualityTier;
        }

        if (s_locGalaxyTime >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyTime, &view.time,
                           SHADER_UNIFORM_FLOAT);
        }
        if (s_locGalaxyResolution >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyResolution, &view.resolution,
                           SHADER_UNIFORM_VEC2);
        }
        if (s_locGalaxyOffset >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyOffset, &view.camera.target,
                           SHADER_UNIFORM_VEC2);
        }
        if (s_locGalaxyZoom >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyZoom, &view.camera.zoom,
                           SHADER_UNIFORM_FLOAT);
        }
        if (s_locGalaxyCameraOffset >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyCameraOffset,
                           &view.camera.offset, SHADER_UNIFORM_VEC2);
        }
        
        Vector2 center = { GALAXY_CENTER_X, GALAXY_CENTER_Y };
        float scale = GALAXY_SCALE;
        if (s_locGalaxyCenter >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyCenter, &center,
                           SHADER_UNIFORM_VEC2);
        }
        if (s_locGalaxyScale >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyScale, &scale,
                           SHADER_UNIFORM_FLOAT);
        }
        if (s_locGalaxyQualityTier >= 0) {
            SetShaderValue(s_shGalaxy, s_locGalaxyQualityTier, &qualityTier,
                           SHADER_UNIFORM_INT);
        }

        
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
    if (!s_initialized || s_shNode.id == 0) return;

    // Begin Shader Mode for the entire batch

    BeginShaderMode(s_shNode);

    int locTime = GetShaderLocation(s_shNode, "uTime");
    int locBaseColor = GetShaderLocation(s_shNode, "uBaseColor");
    
    // Set Global Uniforms ONCE
    SetShaderValue(s_shNode, locTime, &view.time, SHADER_UNIFORM_FLOAT);
    Vector4 baseColorVec = {0.8f, 0.8f, 0.8f, 1.0f};
    SetShaderValue(s_shNode, locBaseColor, &baseColorVec, SHADER_UNIFORM_VEC4);

    
    for (const auto& [id, node] : graph.nodes) {
        auto status = AstrolabeSystem::NodeStatus::Locked;
        if (comp) {
            status = AstrolabeSystem::getNodeStatus(graph, *comp, id);
        }
        
        float r = getNodeRadius(node.type);
        if (id == hoveredNodeId) r *= 1.2f; 
        
        int statusInt = (int)status;
        float progress = 0.0f;
        if (comp) {
             progress = (float)comp->getNodePoints(id) / node.maxPoints;
        }
        
        int shapeType = 0; // Circle
        if (node.type == TalentNodeType::Major) shapeType = 1; // Hexagon
        else if (node.type == TalentNodeType::Core) shapeType = 2; // Octagon

        // Encode data into Vertex Color (Tint)
        // R = Status (0, 1, 2, 3, 4)
        // G = Shape (0, 1, 2)
        // B = Progress (0-255)
        // A = Opacity (0-255)
        
        Color encodedColor;
        encodedColor.r = (unsigned char)statusInt;
        encodedColor.g = (unsigned char)shapeType;
        encodedColor.b = (unsigned char)(progress * 255.0f);
        encodedColor.a = (unsigned char)(255.0f * view.alpha); // Fade support
        
        // Draw geometry into the batch
        Rectangle srcRect = { 0, 0, 1, 1 };
        Rectangle dstRect = { node.x - r, node.y - r, r * 2, r * 2 };
        DrawTexturePro(s_whitePixel, srcRect, dstRect, {0, 0}, 0.0f, encodedColor);
    }
    
    // End Shader Mode and Flush Batch
    EndShaderMode();
}

} // namespace NoMoreDay
