#include "game/systems/ui/AstrolabeRenderer.hpp"
#include "game/components/Common.hpp"
#include "rlgl.h"
#include "raymath.h"
#include "core/logging/Logger.hpp"

namespace NoMoreDay {

Shader AstrolabeRenderer::s_shGalaxy;
bool AstrolabeRenderer::s_initialized = false;

void AstrolabeRenderer::Init(Shader galaxyShader) {
    s_shGalaxy = galaxyShader;
    s_initialized = true;
    LOG_INFO("AstrolabeRenderer: Initialized. Shader ID: {}", galaxyShader.id);
}

void AstrolabeRenderer::Unload() {
    s_initialized = false;
}

void AstrolabeRenderer::Draw(const AstrolabeMap& map, const AstrolabeView& view) {
    DrawBackground(view);
    
    BeginMode2D(view.camera);
    DrawConnections(map, view);
    DrawStars(map, view);
    EndMode2D();
}

void AstrolabeRenderer::DrawBackground(const AstrolabeView& view) {
    if (!s_initialized || s_shGalaxy.id <= 0) {
        DrawRectangle(0, 0, (int)view.resolution.x, (int)view.resolution.y, BLACK);
        return;
    }

    // Standard Uniforms
    int timeLoc = GetShaderLocation(s_shGalaxy, "uTime");
    int offsetLoc = GetShaderLocation(s_shGalaxy, "uOffset");
    int resLoc = GetShaderLocation(s_shGalaxy, "uResolution");
    int zoomLoc = GetShaderLocation(s_shGalaxy, "uZoom");

    // Galaxy-specific Uniforms
    int galaxyCenterLoc = GetShaderLocation(s_shGalaxy, "uGalaxyCenter");
    int galaxyScaleLoc = GetShaderLocation(s_shGalaxy, "uGalaxyScale");

    float zoom = view.camera.zoom;
    SetShaderValue(s_shGalaxy, timeLoc, &view.time, SHADER_UNIFORM_FLOAT);
    Vector2 offset = { view.camera.target.x, view.camera.target.y };
    SetShaderValue(s_shGalaxy, offsetLoc, &offset, SHADER_UNIFORM_VEC2);
    SetShaderValue(s_shGalaxy, resLoc, &view.resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(s_shGalaxy, zoomLoc, &zoom, SHADER_UNIFORM_FLOAT);

    // Galaxy center is FIXED at the talent tree visual center (in world coordinates)
    using namespace Constants::Astrolabe;
    Vector2 galaxyCenter = { GALAXY_CENTER_X, GALAXY_CENTER_Y };
    float galaxyScale = GALAXY_SCALE;
    SetShaderValue(s_shGalaxy, galaxyCenterLoc, &galaxyCenter, SHADER_UNIFORM_VEC2);
    SetShaderValue(s_shGalaxy, galaxyScaleLoc, &galaxyScale, SHADER_UNIFORM_FLOAT);

    // Ensure Render State
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    BeginShaderMode(s_shGalaxy);
    DrawRectangle(0, 0, (int)view.resolution.x, (int)view.resolution.y, WHITE);
    EndShaderMode();
}

void AstrolabeRenderer::DrawConnections(const AstrolabeMap& map, const AstrolabeView& view) {
    for (const auto& [id, star] : map.stars) {
        Vector2 start = {star.x, star.y};
        for (uint32_t preId : star.prerequisites) {
            auto it = map.stars.find(preId);
            if (it != map.stars.end()) {
                Vector2 end = {it->second.x, it->second.y};
                Color glowColor = { 100, 150, 255, (unsigned char)(120 * view.alpha) };
                Color coreColor = { 200, 230, 255, (unsigned char)(200 * view.alpha) };
                DrawLineEx(start, end, 6.0f, glowColor);
                DrawLineEx(start, end, 2.5f, coreColor);
            }
        }
    }
}

void AstrolabeRenderer::DrawStars(const AstrolabeMap& map, const AstrolabeView& view) {
    // 1. Large Outer Glow
    for (const auto& [id, star] : map.stars) {
        Vector2 pos = {star.x, star.y};
        float radius = (star.type == StarNodeType::Keystone) ? 100.0f : (star.type == StarNodeType::Major ? 60.0f : 40.0f);
        Color glow = { 100, 160, 255, (unsigned char)(80 * view.alpha) };
        DrawCircleGradient((int)pos.x, (int)pos.y, radius, glow, Fade(glow, 0.0f));
    }

    // 2. Bright Core
    for (const auto& [id, star] : map.stars) {
        Vector2 pos = {star.x, star.y};
        float radius = (star.type == StarNodeType::Keystone) ? 25.0f : (star.type == StarNodeType::Major ? 15.0f : 8.0f);
        Color color = (star.type == StarNodeType::Keystone) ? Color{255, 255, 200, (unsigned char)(255 * view.alpha)} : Color{220, 245, 255, (unsigned char)(255 * view.alpha)};
        DrawCircleV(pos, radius, color);
        DrawCircleV(pos, radius * 0.5f, WHITE);
    }
}

} // namespace NoMoreDay
