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

void AstrolabeRenderer::Draw(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes) {
    DrawBackground(view);
    
    BeginMode2D(view.camera);
    DrawConnections(map, view, activatedNodes);
    DrawStars(map, view, activatedNodes);
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

void AstrolabeRenderer::DrawStars(const AstrolabeMap& map, const AstrolabeView& view, const std::set<uint32_t>& activatedNodes) {
    // 1. [底衬层]: 绘制半透明黑色背景 (Occlusion)
    //    用于遮挡背景银河，凸显前景星星
    for (const auto& [id, star] : map.stars) {
        Vector2 pos = {star.x, star.y};
        float typeScale = (star.type == StarNodeType::Keystone) ? 2.5f : (star.type == StarNodeType::Major ? 1.6f : 1.2f);
        
        Color coreBlack = {0, 0, 0, (unsigned char)(240 * view.alpha)};
        Color edgeBlack = {0, 0, 0, 0};
        DrawCircleGradient((int)pos.x, (int)pos.y, 18.0f * typeScale, coreBlack, edgeBlack);
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

        float typeScale = (star.type == StarNodeType::Keystone) ? 1.8f : (star.type == StarNodeType::Major ? 1.4f : 1.0f);
        
        if (isActive) {
            // === [已激活]: 纯净恒星 (Pure Stellar Core) ===
            // 去除了十字星芒，专注于极致的球体光感
            
            bool isKeystone = (star.type == StarNodeType::Keystone);
            float pulse = sin(view.time * 2.0f) * 0.05f + 1.0f; // 呼吸幅度减小，更稳重

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
            
            // Adjust alpha by view
            cCore.a = (unsigned char)(cCore.a * view.alpha);
            cInner.a = (unsigned char)(cInner.a * view.alpha);
            cOuter.a = (unsigned char)(cOuter.a * view.alpha);

            // 1. Core (Solid & Sharp)
            DrawCircleV(pos, 4.5f * typeScale, cCore);
            
            // 2. Photosphere (Inner intense glow) - 范围适中 -> 缩小25% (12 -> 9)
            DrawCircleGradient((int)pos.x, (int)pos.y, 9.0f * typeScale * pulse, cInner, Fade(BLACK, 0));

            // 3. Corona (Soft ambient glow) - 范围受控 -> 缩小25% (25 -> 19)
            DrawCircleGradient((int)pos.x, (int)pos.y, 19.0f * typeScale, cOuter, Fade(BLACK, 0));

        } else if (isAvailable) {
            // === [可解锁]: 奇点信标 (Singularity Beacon) ===
            // 颜色改为 [琥珀金/亮橙色]，与蓝色背景形成互补色对比，极度显眼
            // 特效范围大幅缩小，不再是巨大的涟漪，而是紧贴的能量充能
            
            float pulse = sin(view.time * 6.0f) * 0.1f + 1.0f; // 快节奏呼吸，提示"点击我"

            // Colors: Amber / Gold High Contrast
            Color cSignal = { 255, 200, 50, (unsigned char)(220 * view.alpha) };
            Color cGlow = { 255, 150, 0, (unsigned char)(150 * view.alpha) };
            
            // 1. Core Ring (Hollow)
            DrawRing(pos, 3.0f * typeScale, 5.0f * typeScale, 0, 360, 24, cSignal);
            
            // 2. Focused Glow (Small radius) -> 缩小25% (12 -> 9)
            DrawCircleGradient((int)pos.x, (int)pos.y, 9.0f * typeScale * pulse, cGlow, Fade(BLACK, 0));
            
            // 3. Sharp Import Ring (Tighter shrinking effect)
            // 范围从 25 缩小到 14，更紧凑
            float shrink = fmod(view.time * 10.0f, 14.0f);
            float r = 14.0f - shrink;
            
            if (r > 5.5f * typeScale) { // 不要缩得太里面
                 Color cRing = { 255, 220, 100, (unsigned char)((r/14.0f) * 200 * view.alpha) };
                 // 完整的细锐圆环，不再是断裂的
                 DrawRing(pos, r * typeScale, (r + 1.0f) * typeScale, 0, 360, 32, cRing);
            }

        } else {
            // === [未解锁]: 沉睡星核 (Sleeping Star Core) ===
            // 风格调整：不再使用诡异的红色，改为"冷钢/深空"风格，预示着等待被点燃
            
            // 核心: 深邃的蓝灰，像熄灭的恒星
            Color cDormant = { 20, 30, 40, (unsigned char)(200 * view.alpha) };
            // 边缘: 银灰色的金属光泽，清晰可见但不突兀
            Color cRim = { 120, 140, 160, (unsigned char)(120 * view.alpha) };
            // 种子: 中心微弱的白点，暗示潜能
            Color cSeed = { 200, 220, 255, (unsigned char)(60 * view.alpha) };

            // 1. Solid Cold Core
            DrawCircleV(pos, 4.0f * typeScale, cDormant);
            
            // 2. Metallic Rim
            DrawRing(pos, 5.5f * typeScale, 6.2f * typeScale, 0, 360, 32, cRim);
            
            // 3. Potential Seed
            DrawCircleV(pos, 1.0f * typeScale, cSeed);
        }
    }
    EndBlendMode();
}

} // namespace NoMoreDay
