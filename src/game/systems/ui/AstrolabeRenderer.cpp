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

void AstrolabeRenderer::Draw(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes, uint32_t hoveredNodeId) {
    DrawBackground(view);
    
    BeginMode2D(view.camera);
    DrawConnections(map, view, activatedNodes);
    DrawStars(map, view, activatedNodes, hoveredNodeId);
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

void AstrolabeRenderer::DrawConnections(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes) {
    // 1. [对比度层]: 先绘制黑色的底衬线，确保在亮背景上也能看清
    //    不需要 Additive 混合，使用默认混合 (Alpha Blend)   
    for (const auto& [id, star] : map.stars) {
        Vector2 start = {star.x, star.y};
        bool isStartActive = activatedNodes.contains(id);
        for (uint32_t preId : star.prerequisites) {
            auto it = map.stars.find(preId);
            if (it != map.stars.end()) {
                Vector2 end = {it->second.x, it->second.y};
                bool isEndActive = activatedNodes.contains(preId);
                
                // 仅为已激活或可激活的连线绘制黑色底，未解锁的灰色线本身就暗，不需要
                if (isEndActive) {
                    float width = (isStartActive && isEndActive) ? 5.0f : 3.0f;
                    DrawLineEx(start, end, width, Color{0, 0, 0, (unsigned char)(150 * view.alpha)});
                }
            }
        }
    }

    // 2. [光效层]: 启用混合模式绘制发光部分
    BeginBlendMode(BLEND_ADDITIVE);

    for (const auto& [id, star] : map.stars) {
        Vector2 start = {star.x, star.y};
        bool isStartActive = activatedNodes.contains(id);

        for (uint32_t preId : star.prerequisites) {
            auto it = map.stars.find(preId);
            if (it != map.stars.end()) {
                Vector2 end = {it->second.x, it->second.y};
                bool isEndActive = activatedNodes.contains(preId);

                if (isStartActive && isEndActive) {
                    // [已连通]: 星光能量流 (Starlight Stream)
                    // 核心: 纯白亮线
                    DrawLineEx(start, end, 2.0f, Color{255, 255, 255, (unsigned char)(200 * view.alpha)});
                    // 外辉: 极光蓝
                    DrawLineEx(start, end, 5.0f, Color{0, 200, 255, (unsigned char)(80 * view.alpha)});
                    // 远辉: 深蓝
                    DrawLineEx(start, end, 8.0f, Color{0, 50, 200, (unsigned char)(40 * view.alpha)});
                } else if (isEndActive) { 
                    // [潜在路径]: 星尘轨迹 (Stardust Path)
                    // 虚幻的青色流光
                    float flow = sin(view.time * 2.5f - (start.x + start.y) * 0.02f) * 0.4f + 0.6f;
                    Color flowColor = { 100, 255, 200, (unsigned char)(120 * flow * view.alpha) };
                    DrawLineEx(start, end, 1.5f, flowColor);
                } else {
                    // [未解锁]: 暗物质连线 (Dark Matter Link)
                    // 极细的深灰线
                    Color lockedColor = { 60, 70, 80, (unsigned char)(40 * view.alpha) };
                    DrawLineEx(start, end, 1.0f, lockedColor);
                }
            }
        }
    }
    EndBlendMode();
}

void AstrolabeRenderer::DrawStars(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes, uint32_t hoveredNodeId) {
    // 1. [底衬层]: 绘制半透明黑色背景 (Occlusion)
    //    用于遮挡背景银河，凸显前景星星
    for (const auto& [id, star] : map.stars) {
        Vector2 pos = {star.x, star.y};
        bool isHovered = (id == hoveredNodeId);
        float hoverScale = isHovered ? 1.2f : 1.0f;
        float typeScale = (star.type == StarNodeType::Keystone) ? 2.5f : (star.type == StarNodeType::Major ? 1.6f : 1.2f);
        typeScale *= hoverScale;

        // 调整：不再使用巨大的实心多边形，改回柔和的圆形渐变，且大幅缩小范围
        // 从 18.0f 缩小到 8.0f，仅略大于节点本体，避免"黑洞"违和感
        float occlusionRadius = 8.0f * typeScale;
        
        Color coreBlack = {0, 0, 0, (unsigned char)(150 * view.alpha)}; // 大幅降低不透明度 200 -> 150
        Color edgeBlack = {0, 0, 0, 0};
        
        DrawCircleGradient((int)pos.x, (int)pos.y, occlusionRadius, coreBlack, edgeBlack);
    }

    // 2. [天体层]: 使用叠加混合模式绘制
    BeginBlendMode(BLEND_ADDITIVE);

    for (const auto& [id, star] : map.stars) {
        Vector2 pos = {star.x, star.y};
        bool isActive = activatedNodes.contains(id);
        
        // 可用性检查
        bool isAvailable = !isActive;
        if (isAvailable) {
            for (uint32_t preId : star.prerequisites) {
                if (!activatedNodes.contains(preId)) {
                    isAvailable = false;
                    break;
                }
            }
            if (star.prerequisites.empty() && !isActive) isAvailable = true; 
        }

        bool isHovered = (id == hoveredNodeId);
        float hoverScale = isHovered ? 1.15f : 1.0f;
        float typeScale = (star.type == StarNodeType::Keystone) ? 1.8f : (star.type == StarNodeType::Major ? 1.4f : 1.0f);
        typeScale *= hoverScale;
        
        if (isActive) {
            // === [已激活]: 纯净恒星 (Pure Stellar Core) ===
            bool isKeystone = (star.type == StarNodeType::Keystone);
            float pulse = sin(view.time * 2.0f) * 0.05f + 1.0f; 

            Color cCore, cInner, cOuter;
            
            if (isKeystone) {
                // Keystone: 炽热金芯
                cCore = {255, 255, 240, 255};
                cInner = {255, 200, 100, 200};
                cOuter = {255, 100, 50, 100};
            } else {
                // Normal: 极寒蓝芯
                cCore = {240, 255, 255, 255};
                cInner = {100, 220, 255, 200};
                cOuter = {20, 100, 255, 100};
            }
            
            cCore.a = (unsigned char)(cCore.a * view.alpha);
            cInner.a = (unsigned char)(cInner.a * view.alpha);
            cOuter.a = (unsigned char)(cOuter.a * view.alpha);

            float rCore = 4.5f * typeScale;
            float rInner = 9.0f * typeScale * pulse;
            float rOuter = 19.0f * typeScale;

            if (star.type == StarNodeType::Keystone) {
                DrawPoly(pos, 8, rCore, 22.5f + view.time * 10, cCore);
                DrawPoly(pos, 8, rInner, 22.5f - view.time * 5, Fade(cInner, 0.5f));
                // Outer glow is spherical
                DrawCircleGradient((int)pos.x, (int)pos.y, rOuter, cOuter, Fade(BLACK, 0));
            } else if (star.type == StarNodeType::Major) {
                DrawPoly(pos, 6, rCore, 0.0f + view.time * 15, cCore);
                DrawPoly(pos, 6, rInner, 0.0f - view.time * 8, Fade(cInner, 0.5f));
                DrawCircleGradient((int)pos.x, (int)pos.y, rOuter, cOuter, Fade(BLACK, 0));
            } else {
                DrawCircleV(pos, rCore, cCore);
                DrawCircleGradient((int)pos.x, (int)pos.y, rInner, cInner, Fade(BLACK, 0));
                DrawCircleGradient((int)pos.x, (int)pos.y, rOuter, cOuter, Fade(BLACK, 0));
            }

        } else if (isAvailable) {
            // === [可解锁]: 奇点信标 (Singularity Beacon) ===
            float pulse = sin(view.time * 4.0f) * 0.1f + 1.0f;

            Color cSignal = { 255, 200, 50, (unsigned char)(220 * view.alpha) };
            Color cGlow = { 255, 150, 0, (unsigned char)(150 * view.alpha) };
            
            float rBase = 3.0f * typeScale;
            float rGlow = 9.0f * typeScale * pulse;

            // Shape Rendering
            if (star.type == StarNodeType::Keystone) {
                DrawPolyLinesEx(pos, 8, rBase * 1.5f, 22.5f, 2.0f, cSignal);
                DrawPolyLinesEx(pos, 8, rBase * 2.0f * pulse, 22.5f, 1.0f, Fade(cSignal, 0.5f));
            } else if (star.type == StarNodeType::Major) {
                DrawPolyLinesEx(pos, 6, rBase * 1.5f, 0.0f, 2.0f, cSignal);
                DrawPolyLinesEx(pos, 6, rBase * 2.0f * pulse, 0.0f, 1.0f, Fade(cSignal, 0.5f));
            } else {
                DrawRing(pos, rBase, rBase + 2.0f, 0, 360, 24, cSignal);
            }
            
            // Central Glow
            DrawCircleGradient((int)pos.x, (int)pos.y, rGlow, cGlow, Fade(BLACK, 0));
            
            // External Halo (Breathing)
            float shrink = fmod(view.time * 10.0f, 14.0f);
            float r = 14.0f - shrink; 
            if (r > 6.0f) {
                 Color cRing = { 255, 220, 100, (unsigned char)((r/14.0f) * 200 * view.alpha) };
                 DrawRing(pos, r * typeScale, (r + 1.0f) * typeScale, 0, 360, 32, cRing);
            }

        } else {
            // === [未解锁]: 沉睡星核 (Sleeping Star Core) ===
            Color cDormant = { 20, 30, 40, (unsigned char)(200 * view.alpha) };
            Color cRim = { 120, 140, 160, (unsigned char)(120 * view.alpha) };
            Color cSeed = { 200, 220, 255, (unsigned char)(60 * view.alpha) };

            float rBase = 4.0f * typeScale;
            float rRim = 5.5f * typeScale;

            if (star.type == StarNodeType::Keystone) {
                DrawPoly(pos, 8, rBase, 22.5f, cDormant);
                DrawPolyLinesEx(pos, 8, rRim, 22.5f, 1.5f, cRim);
                DrawCircleV(pos, 1.0f * typeScale, cSeed);
            } else if (star.type == StarNodeType::Major) {
                 DrawPoly(pos, 6, rBase, 0.0f, cDormant);
                 DrawPolyLinesEx(pos, 6, rRim, 0.0f, 1.5f, cRim);
                 DrawCircleV(pos, 1.0f * typeScale, cSeed);
            } else {
                DrawCircleV(pos, rBase, cDormant);
                DrawRing(pos, rRim, rRim + 1.0f, 0, 360, 32, cRim);
                DrawCircleV(pos, 1.0f * typeScale, cSeed);
            }
        }
    }
    EndBlendMode();
}

} // namespace NoMoreDay
