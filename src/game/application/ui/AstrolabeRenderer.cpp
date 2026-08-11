#include "game/application/ui/AstrolabeRenderer.hpp"
#include "engine/render/core/QualityTierManager.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/foundation/data/TalentLayoutService.hpp"
#include "game/foundation/data/AstrolabeConstants.hpp"
#include "core/logging/Logger.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>

namespace NoMoreDay {

AstrolabeRenderer::~AstrolabeRenderer() {
    Unload();
}

void AstrolabeRenderer::Init(Shader galaxyShader, Shader nodeShader) {
    m_shGalaxy = galaxyShader;
    m_shNode = nodeShader;
    
    // Create 1x1 white texture for UV-correct drawing
    Image img = GenImageColor(1, 1, WHITE);
    m_whitePixel = LoadTextureFromImage(img);
    UnloadImage(img);

    if (m_shGalaxy.id != 0) {
        m_locGalaxyTime = GetShaderLocation(m_shGalaxy, "uTime");
        m_locGalaxyResolution = GetShaderLocation(m_shGalaxy, "uResolution");
        m_locGalaxyOffset = GetShaderLocation(m_shGalaxy, "uOffset");
        m_locGalaxyZoom = GetShaderLocation(m_shGalaxy, "uZoom");
        m_locGalaxyCameraOffset = GetShaderLocation(m_shGalaxy, "uCameraOffset");
        m_locGalaxyCenter = GetShaderLocation(m_shGalaxy, "uGalaxyCenter");
        m_locGalaxyScale = GetShaderLocation(m_shGalaxy, "uGalaxyScale");
        m_locGalaxyQualityTier = GetShaderLocation(m_shGalaxy, "uQualityTier");
        m_locGalaxyRenderScale = GetShaderLocation(m_shGalaxy, "uRenderScale");
    }
    
    m_initialized = true;
}

void AstrolabeRenderer::Unload() {
    if (m_whitePixel.id != 0) {
        UnloadTexture(m_whitePixel);
        m_whitePixel = {0};
    }
    if (m_galaxyCache.texture.id != 0) {
        UnloadRenderTexture(m_galaxyCache);
        m_galaxyCache = {0};
    }
    m_galaxyCacheRes = {0, 0};
    m_galaxyCacheValid = false;
    m_initialized = false;
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
    if (m_initialized && m_shGalaxy.id > 0) {
        // Half-resolution cache size derived from the viewport resolution
        int cacheW = (int)(view.resolution.x * 0.5f);
        int cacheH = (int)(view.resolution.y * 0.5f);
        if (cacheW < 1) cacheW = 1;
        if (cacheH < 1) cacheH = 1;
        Vector2 cacheRes = { (float)cacheW, (float)cacheH };

        // (Re)create the cache when the viewport resolution changes
        if (!m_galaxyCacheValid || m_galaxyCache.texture.width != cacheW ||
            m_galaxyCache.texture.height != cacheH) {
            if (m_galaxyCache.texture.id != 0) {
                UnloadRenderTexture(m_galaxyCache);
                m_galaxyCache = {0};
            }
            m_galaxyCache = LoadRenderTexture(cacheW, cacheH);
            m_galaxyCacheRes = cacheRes;
            m_galaxyCacheValid = (m_galaxyCache.texture.id != 0);
        }

        if (!m_galaxyCacheValid) {
            ClearBackground(BLACK);
            return;
        }

        // Pass 1: render the galaxy into the half-resolution cache. The shader
        // is driven purely by gl_FragCoord + uniforms, so a full-viewport
        // rectangle covers the FBO without needing the camera transform.
        BeginTextureMode(m_galaxyCache);
        BeginShaderMode(m_shGalaxy);
        const int qualityTier = static_cast<int>(
            render::core::QualityTierManager::Get().GetTier());
        if (qualityTier != m_prevGalaxyQualityTier) {
            LOG_INFO("AstrolabeRenderer: Galaxy background quality tier={}",
                     qualityTier);
            m_prevGalaxyQualityTier = qualityTier;
        }

        if (m_locGalaxyTime >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyTime, &view.time,
                           SHADER_UNIFORM_FLOAT);
        }
        if (m_locGalaxyResolution >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyResolution, &view.resolution,
                           SHADER_UNIFORM_VEC2);
        }
        if (m_locGalaxyOffset >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyOffset, &view.camera.target,
                           SHADER_UNIFORM_VEC2);
        }
        if (m_locGalaxyZoom >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyZoom, &view.camera.zoom,
                           SHADER_UNIFORM_FLOAT);
        }
        if (m_locGalaxyCameraOffset >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyCameraOffset,
                           &view.camera.offset, SHADER_UNIFORM_VEC2);
        }
        
        Vector2 center = { GALAXY_CENTER_X, GALAXY_CENTER_Y };
        float scale = GALAXY_SCALE;
        if (m_locGalaxyCenter >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyCenter, &center,
                           SHADER_UNIFORM_VEC2);
        }
        if (m_locGalaxyScale >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyScale, &scale,
                           SHADER_UNIFORM_FLOAT);
        }
        if (m_locGalaxyQualityTier >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyQualityTier, &qualityTier,
                           SHADER_UNIFORM_INT);
        }
        // Map cache texels back to full-resolution screen coordinates
        Vector2 renderScale = { view.resolution.x / cacheRes.x,
                                view.resolution.y / cacheRes.y };
        if (m_locGalaxyRenderScale >= 0) {
            SetShaderValue(m_shGalaxy, m_locGalaxyRenderScale, &renderScale,
                           SHADER_UNIFORM_VEC2);
        }

        DrawRectangle(0, 0, cacheW, cacheH, BLACK);
        
        EndShaderMode();
        EndTextureMode();

        // Pass 2: blit the cache up to the screen. EndTextureMode reset the
        // modelview matrix, so re-apply the camera before drawing.
        BeginMode2D(view.camera);
        Vector2 tl = GetScreenToWorld2D({0, 0}, view.camera);
        Vector2 br = GetScreenToWorld2D(view.resolution, view.camera);
        Rectangle src = { 0, 0, cacheRes.x, -cacheRes.y };
        Rectangle dst = { tl.x - 100, tl.y - 100, (br.x - tl.x) + 200,
                          (br.y - tl.y) + 200 };
        DrawTexturePro(m_galaxyCache.texture, src, dst, {0, 0}, 0.0f, WHITE);
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
    if (!m_initialized || m_shNode.id == 0) return;

    // Begin Shader Mode for the entire batch

    BeginShaderMode(m_shNode);

    int locTime = GetShaderLocation(m_shNode, "uTime");
    int locBaseColor = GetShaderLocation(m_shNode, "uBaseColor");
    
    // Set Global Uniforms ONCE
    SetShaderValue(m_shNode, locTime, &view.time, SHADER_UNIFORM_FLOAT);
    Vector4 baseColorVec = {0.8f, 0.8f, 0.8f, 1.0f};
    SetShaderValue(m_shNode, locBaseColor, &baseColorVec, SHADER_UNIFORM_VEC4);

    
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
        DrawTexturePro(m_whitePixel, srcRect, dstRect, {0, 0}, 0.0f, encodedColor);
    }
    
    // End Shader Mode and Flush Batch
    EndShaderMode();
}

} // namespace NoMoreDay
