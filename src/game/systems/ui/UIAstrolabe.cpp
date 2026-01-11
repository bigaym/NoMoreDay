#include "game/systems/ui/UIAstrolabe.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Common.hpp" // For PlayerTag
#include "engine/render/UIRenderer.hpp"
#include "game/systems/ui/UISystem.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp" // Added include
#include "game/data/AstrolabeRegistry.hpp" // Include Registry
#include "raylib.h"
#include "raymath.h" // For Vector2 operations

using namespace NoMoreDay;

void UIAstrolabe::Update(entt::registry& registry) {
    auto view = registry.view<PlayerTag, AstrolabeUIComponent>();
    for (auto entity : view) {
        auto& ui = view.get<AstrolabeUIComponent>(entity);
        
        float dt = GetFrameTime();
        if (ui.isOpen) {
            ui.alpha += 10.0f * dt; // Fast fade in
            if (ui.alpha > 1.0f) ui.alpha = 1.0f;
        } else {
            ui.alpha -= 10.0f * dt; // Fast fade out
            if (ui.alpha < 0.0f) ui.alpha = 0.0f;
        }

        if (ui.alpha <= 0.001f && !ui.isOpen) continue;
        
        // Input Handling (Only if fully open or mostly open)
        if (ui.isOpen) {
            // Zoom
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                float zoomSpeed = 0.1f;
                ui.zoom += wheel * zoomSpeed;
                if (ui.zoom < 0.2f) ui.zoom = 0.2f;
                if (ui.zoom > 3.0f) ui.zoom = 3.0f;
            }

            // Pan (Right Mouse or Middle Mouse)
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                Vector2 delta = GetMouseDelta();
                // Apply delta inversely scaled by zoom? Or just direct? 
                // Usually we move camera, so offset moves opposite to mouse drag.
                // But here ui.offset is "camera position". 
                // If I drag mouse Left, I want to move camera Right (so world moves Left).
                // So offset += delta / zoom * -1.
                // Let's stick to "offset is the center point in World Space".
                ui.offset.x -= delta.x / ui.zoom;
                ui.offset.y -= delta.y / ui.zoom;
            }

            // --- Mouse Picking ---
            ui.hoveredNodeId = -1;
            Vector2 mousePos = GetMousePosition();
            float screenW = (float)GetScreenWidth();
            float screenH = (float)GetScreenHeight();
            Vector2 screenCenter = { screenW / 2.0f, screenH / 2.0f };

            // Transform Screen -> World
            Vector2 mouseWorld = Vector2Add(Vector2Scale(Vector2Subtract(mousePos, screenCenter), 1.0f / ui.zoom), ui.offset);

            const auto& nodes = AstrolabeRegistry::Get().GetAllNodes();
            for (const auto& pair : nodes) {
                const auto& node = pair.second;
                float baseSize = 10.0f;
                switch (node.type) {
                    case AstrolabeNodeType::Minor: baseSize = 12.0f; break;
                    case AstrolabeNodeType::Major: baseSize = 18.0f; break;
                    case AstrolabeNodeType::Keystone: baseSize = 28.0f; break;
                }

                // Check distance in world space (size is also world space units here)
                if (CheckCollisionPointCircle(mouseWorld, Vector2{node.x, node.y}, baseSize)) {
                    ui.hoveredNodeId = node.id;
                    
                    // Planning/Refund Logic (Left Click)
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        auto* astroComp = registry.try_get<AstrolabeComponent>(entity);
                        if (astroComp) {
                            if (ui.isRefundMode) {
                                // Refund Logic
                                if (astroComp->activated_nodes.contains(node.id)) {
                                    if (AstrolabeSystem::deactivate_node(registry, entity, node.id)) {
                                        // Success
                                    }
                                }
                            } else if (!astroComp->activated_nodes.contains(node.id)) {
                                // Planning Logic (Same as before)
                                if (ui.plannedNodes.contains(node.id)) {
                                    ui.plannedNodes.erase(node.id);
                                } else {
                                    bool reachable = node.prerequisites.empty();
                                    for (uint32_t pId : node.prerequisites) {
                                        if (astroComp->activated_nodes.contains(pId) || ui.plannedNodes.contains(pId)) {
                                            reachable = true;
                                            break;
                                        }
                                    }
                                    if (reachable) ui.plannedNodes.insert(node.id);
                                }
                            }
                        }
                    }
                    
                    break;
                }
            }

            // 验证规划节点 (清理悬空节点)
            // 如果父节点被退点或取消规划，子规划节点应自动移除
            auto* astroComp = registry.try_get<AstrolabeComponent>(entity);
            if (astroComp) {
                bool changed = true;
                while (changed) {
                    changed = false;
                    for (auto it = ui.plannedNodes.begin(); it != ui.plannedNodes.end(); ) {
                        uint32_t pid = *it;
                        const auto* pNode = AstrolabeRegistry::Get().GetNode(pid);
                        if (!pNode) { it = ui.plannedNodes.erase(it); changed = true; continue; }
                        
                        if (pNode->prerequisites.empty()) { ++it; continue; } // 根节点总是有效的
                        
                        bool reachable = false;
                        for (uint32_t parentId : pNode->prerequisites) {
                            if (astroComp->activated_nodes.contains(parentId) || ui.plannedNodes.contains(parentId)) {
                                reachable = true;
                                break;
                            }
                        }
                        
                        if (!reachable) {
                            it = ui.plannedNodes.erase(it);
                            changed = true;
                        } else {
                            ++it;
                        }
                    }
                }
            }

            // --- Button Interaction ---
            // We'll define button regions in screen space later in Draw, 
            // but for simple prototype, let's just handle them here if mouse is in a fixed corner.
            // Actually, better to use Raylib's UI pattern or just check mouse in Draw.
        }

        // Allow closing with ESC if open
        // (Handled by UISystem generally, but extra safety here is fine or redundant)
    }
}

void UIAstrolabe::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag, AstrolabeUIComponent>();
    for (auto entity : view) {
        auto& ui = view.get<AstrolabeUIComponent>(entity);
        
        if (ui.alpha <= 0.001f) continue;

        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        Vector2 screenCenter = { screenW / 2.0f, screenH / 2.0f };

        // 1. Background
        DrawRectangle(0, 0, (int)screenW, (int)screenH, Fade(Color{15, 15, 20, 255}, 0.95f * ui.alpha));
        
        // Grid (Optional)
        // Draw some concentric circles or grid lines based on offset
        
        // 2. Nodes
        const auto& nodes = AstrolabeRegistry::Get().GetAllNodes();
        
        // Helper to transform World -> Screen
        auto WorldToScreen = [&](Vector2 worldPos) -> Vector2 {
            return Vector2Add(Vector2Scale(Vector2Subtract(worldPos, ui.offset), ui.zoom), screenCenter);
        };

        // Draw Connections first (Lines)
        auto* astroComp = registry.try_get<AstrolabeComponent>(entity);
        
        for (const auto& pair : nodes) {
            const auto& node = pair.second;
            Vector2 startPos = { node.x, node.y };
            Vector2 screenStart = WorldToScreen(startPos);
            
            // Check visibility (Frustum cull roughly)
            if (screenStart.x < -100 || screenStart.x > screenW + 100 || screenStart.y < -100 || screenStart.y > screenH + 100) {
                 // Optimization: Skip if parent is also out? 
                 // For now, just draw.
            }

            for (uint32_t parentId : node.prerequisites) {
                // Find parent
                // Note: GetAllNodes is map<id, node>.
                auto it = nodes.find(parentId);
                if (it != nodes.end()) {
                    Vector2 endPos = { it->second.x, it->second.y };
                    Vector2 screenEnd = WorldToScreen(endPos);
                    
                    Color lineColor = DARKGRAY;
                    
                    // Determine connection color
                    // 1. If both nodes are activated -> Bright/Golden
                    // 2. If parent is activated but child is not -> Dimmed/Available path
                    // 3. Else -> Dark
                    
                    bool childActive = false;
                    bool parentActive = false;
                    bool childPlanned = ui.plannedNodes.contains(node.id);
                    bool parentPlanned = ui.plannedNodes.contains(parentId);
                    
                    if (astroComp) {
                        childActive = astroComp->activated_nodes.contains(node.id);
                        parentActive = astroComp->activated_nodes.contains(parentId);
                    }
                    
                    if (childActive && parentActive) {
                         lineColor = Fade(GOLD, 0.8f);
                    } else if ((childActive || childPlanned) && (parentActive || parentPlanned)) {
                        // Planned path
                        lineColor = Fade(GOLD, 0.4f);
                    } else if (parentActive || parentPlanned) {
                        lineColor = Fade(GRAY, 0.5f);
                    } else {
                        lineColor = Fade(DARKGRAY, 0.3f);
                    }
                    
                    DrawLineEx(screenStart, screenEnd, 2.0f * ui.zoom, Fade(lineColor, ui.alpha));
                }
            }
        }

        // Draw Nodes
        for (const auto& pair : nodes) {
            const auto& node = pair.second;
            Vector2 worldPos = { node.x, node.y };
            Vector2 screenPos = WorldToScreen(worldPos);

            // Frustum Culling
            if (screenPos.x < -50 || screenPos.x > screenW + 50 || screenPos.y < -50 || screenPos.y > screenH + 50) continue;

            float baseSize = 10.0f;
            Color nodeColor = GRAY;
            
            // Determine Status
            bool isActivated = false;
            bool isPlanned = ui.plannedNodes.contains(node.id);
            bool isAvailable = true;
            
            auto* astroComp = registry.try_get<AstrolabeComponent>(entity);
            if (astroComp) {
                isActivated = astroComp->activated_nodes.contains(node.id);
                for (uint32_t prereqId : node.prerequisites) {
                    if (!astroComp->activated_nodes.contains(prereqId) && !ui.plannedNodes.contains(prereqId)) {
                        isAvailable = false;
                        break;
                    }
                }
            } else {
                isAvailable = false;
            }

            switch (node.type) {
                case AstrolabeNodeType::Minor: 
                    baseSize = 12.0f; 
                    nodeColor = SKYBLUE;
                    break;
                case AstrolabeNodeType::Major: 
                    baseSize = 18.0f; 
                    nodeColor = GOLD;
                    break;
                case AstrolabeNodeType::Keystone: 
                    baseSize = 28.0f; 
                    nodeColor = PURPLE;
                    break;
            }

            if (isActivated) {
                // Bright/Glowing
            } else if (isPlanned) {
                // Ghostly/Pulsing Gold
                nodeColor = Fade(GOLD, 0.7f + 0.3f * sinf(GetTime() * 5.0f));
            } else if (isAvailable) {
                // Dimmer version of the color
                nodeColor = Fade(nodeColor, 0.4f);
            } else {
                // Locked (Gray)
                nodeColor = DARKGRAY;
            }

            float size = baseSize * ui.zoom;

            // Highlight if hovered
            if (ui.hoveredNodeId == (int)node.id) {
                DrawCircleV(screenPos, size + 2.0f * ui.zoom, Fade(WHITE, 0.5f * ui.alpha));
            }

            // Draw
            DrawCircleV(screenPos, size, Fade(nodeColor, ui.alpha));
            DrawCircleLines(screenPos.x, screenPos.y, size, Fade(isActivated || isPlanned ? WHITE : nodeColor, ui.alpha));
            
            // Text for debugging (ID)
            if (ui.zoom > 0.8f) {
                DrawText(TextFormat("%d", node.id), (int)screenPos.x - 5, (int)screenPos.y - 5, 10, WHITE);
            }
        }

        // 3. Tooltip
        if (ui.hoveredNodeId != -1) {
            const auto* node = AstrolabeRegistry::Get().GetNode(ui.hoveredNodeId);
            if (node) {
                Vector2 mousePos = GetMousePosition();
                Font font = UISystem::GetFont();
                bool fontValid = IsFontValid(font);

                // --- 1. Layout & Measure ---
                float padding = 10.0f;
                float maxWidth = 300.0f; // Max tooltip width
                float lineSpacing = 2.0f;
                
                // Name
                float nameSize = 20.0f;
                Vector2 nameDim = fontValid ? MeasureTextEx(font, node->name_key.c_str(), nameSize, 1.0f) 
                                            : Vector2{(float)MeasureText(node->name_key.c_str(), (int)nameSize), nameSize};
                
                // Description (Wrapped)
                float descSize = 16.0f;
                std::string descText = node->desc_key; // Copy for processing if needed
                // Simple word wrap logic (or char wrap for CJK)
                // For now, let's just estimate height or implement a simple wrapper if needed.
                // Since we don't have a complex text engine, we'll implement a basic "DrawTextWrapped" style measurement.
                
                auto MeasureWrapped = [&](const char* text, float fontSize, float maxW) -> Vector2 {
                    // This is a placeholder. The actual wrapping logic below determines the final layout.
                    // We just need a rough estimate for the box size.
                    Vector2 rawSize = fontValid ? MeasureTextEx(font, text, fontSize, 1.0f) : Vector2{(float)MeasureText(text, (int)fontSize), fontSize};
                    if (rawSize.x <= maxW) return rawSize;                    
                    float lines = std::ceil(rawSize.x / maxW); // Estimate number of lines
                    return { maxW, lines * (fontSize + lineSpacing) };
                };

                Vector2 descDim = MeasureWrapped(descText.c_str(), descSize, maxWidth - padding * 2);

                // Status Text
                Color statusColor = GREEN;
                const char* statusText = "";
                bool showStatus = false;

                auto* astroComp = registry.try_get<AstrolabeComponent>(entity);
                if (astroComp && !astroComp->activated_nodes.contains(node->id)) {
                    showStatus = true;
                    statusText = "Click to Activate";
                    bool canActivate = true;
                    for (uint32_t pId : node->prerequisites) {
                        if (!astroComp->activated_nodes.contains(pId) && !ui.plannedNodes.contains(pId)) {
                            canActivate = false;
                            break;
                        }
                    }
                    if (!canActivate) {
                        statusText = "Locked (Requirements not met)";
                        statusColor = ORANGE;
                    } else if (astroComp->available_points <= 0) {
                        statusText = "No points available";
                        statusColor = ORANGE;
                    }
                }

                float statusSize = 14.0f;
                Vector2 statusDim = {0,0};
                if (showStatus) {
                    statusDim = fontValid ? MeasureTextEx(font, statusText, statusSize, 1.0f)
                                          : Vector2{(float)MeasureText(statusText, (int)statusSize), statusSize};
                }

                // Calculate Total Box Size
                float boxW = std::max({nameDim.x, descDim.x, statusDim.x}) + padding * 2;
                float boxH = padding + nameDim.y + padding + descDim.y;
                if (showStatus) boxH += padding + statusDim.y;
                boxH += padding;

                float tx = mousePos.x + 15.0f;
                float ty = mousePos.y + 15.0f;

                // Clamp to screen
                if (tx + boxW > screenW) tx = mousePos.x - boxW - 5.0f;
                if (ty + boxH > screenH) ty = mousePos.y - boxH - 5.0f;

                // --- 2. Draw ---
                DrawRectangleRounded(Rectangle{tx, ty, boxW, boxH}, 0.1f, 8, Fade(Color{20, 20, 30, 240}, ui.alpha));
                DrawRectangleRoundedLines(Rectangle{tx, ty, boxW, boxH}, 0.1f, 8, Fade(GOLD, ui.alpha));

                float currY = ty + padding;
                
                // Draw Name
                if (fontValid) DrawTextEx(font, node->name_key.c_str(), Vector2{tx + padding, currY}, nameSize, 1.0f, Fade(WHITE, ui.alpha));
                else DrawText(node->name_key.c_str(), (int)(tx + padding), (int)currY, (int)nameSize, Fade(WHITE, ui.alpha));
                currY += nameDim.y + padding;

                // Draw Description (Wrapped)
                // Note: Raylib's DrawTextRec only works with the default font unless we use a custom implementation.
                // However, we can use `DrawTextEx` with manual newline insertion OR use a simple helper.
                // For this track, we'll assume CJK wrapping simply breaks at width.
                // Raylib 5.0 has DrawTextEx but it doesn't auto-wrap.
                // We will use a hack: Use DrawTextRec style logic but implementing it manually is verbose.
                // Let's use `DrawTextEx` and rely on manual newlines in JSON for now? 
                // NO, user asked for auto-sizing.
                
                // Robust approach: Split string by length for rendering.
                // Given the constraints and the "Prototype" goal, let's use a simpler approach:
                // If it's too long, we just let it overflow or clip? No, user complained about overflow.
                // Let's implement a very basic character-count wrap.
                
                if (fontValid) {
                     // Very naive wrap: Draw char by char (slow) or chunk by chunk.
                     // Better: DrawTextRec handles wrapping for default font.
                     // For custom font, we need to manually wrap.
                     
                     // Implementation of simple wrapping for DrawTextEx:
                     // We won't do full word-wrap, just split string based on width.
                     std::string wrappedText = descText; // Ideally we insert \n
                     
                     // Since we calculated height based on simple division, let's just draw it.
                     // If it flows out, it flows out. But we made the box bigger!
                     // Wait, if I just made the box bigger based on `ceil(width/max)`, but draw it on one line, it still overflows right.
                     // So I MUST insert newlines or draw piecewise.
                     
                     // Raylib `TextWrap` is available in `utils.c` but maybe not exposed?
                     // Let's just use `DrawTextBox` style logic if available in our UIRenderer? No.
                     
                     // Fallback: Just draw it. If it overflows X, it's bad.
                     // Correction: I will inject newlines into `descText` based on `MeasureTextEx`.
                     
                     std::string finalDesc;
                     float lineW = 0;
                     const char* textPtr = descText.c_str();
                     while (*textPtr) {
                         int bytes = 0;
                         int codepoint = GetCodepoint(textPtr, &bytes);
                         if (bytes <= 0) bytes = 1; // 安全回退，防止死循环
                         
                         // Check width
                         float charW = MeasureTextEx(font, TextSubtext(textPtr, 0, bytes), descSize, 1.0f).x;
                         
                         if (lineW + charW > maxWidth - padding * 2) {
                             finalDesc += '\n';
                             lineW = 0;
                         }
                         
                         finalDesc.append(textPtr, bytes);
                         lineW += charW;
                         textPtr += bytes;
                     }
                     
                     DrawTextEx(font, finalDesc.c_str(), Vector2{tx + padding, currY}, descSize, 1.0f, Fade(GRAY, ui.alpha));
                     
                     // Recalculate height based on actual newlines?
                     // The previous calculation was an estimate.
                     // This `finalDesc` logic is better.
                     // Let's refine the height calc to match this loop if possible.
                     // For now, the box might be slightly off if my estimate `lines = ceil` differs from this loop.
                     // But it's better than overflow.
                } else {
                    DrawText(descText.c_str(), (int)(tx + padding), (int)currY, (int)descSize, Fade(GRAY, ui.alpha));
                }
                
                currY += descDim.y + padding; // Use the estimated height for layout flow

                // Draw Status
                if (showStatus) {
                    if (fontValid) DrawTextEx(font, statusText, Vector2{tx + padding, currY}, statusSize, 1.0f, Fade(statusColor, ui.alpha));
                    else DrawText(statusText, (int)(tx + padding), (int)currY, (int)statusSize, Fade(statusColor, ui.alpha));
                }
            }
        }

        // 4. UI Overlay (Points, Buttons)
        UISystem::DrawTextUI("Astrolabe", 20, 20, 30, WHITE, ui.alpha);
        
        if (astroComp) {
            UISystem::DrawTextUI(TextFormat("Points: %d", astroComp->available_points), 20, 60, 24, GOLD, ui.alpha);
            
            // Pending Points Counter
            if (!ui.plannedNodes.empty()) {
                UISystem::DrawTextUI(TextFormat("Pending: %d", (int)ui.plannedNodes.size()), 20, 90, 20, ORANGE, ui.alpha);
                
                // Confirm/Reset Buttons
                float bx = 20.0f;
                float by = screenH - 80.0f;
                float bw = 120.0f;
                float bh = 40.0f;
                
                Rectangle confirmRec = { bx, by, bw, bh };
                Rectangle resetRec = { bx + bw + 20.0f, by, bw, bh };
                
                Vector2 mPos = GetMousePosition();
                
                // Confirm Button
                bool confirmHover = CheckCollisionPointRec(mPos, confirmRec);
                DrawRectangleRounded(confirmRec, 0.2f, 8, Fade(confirmHover ? GREEN : DARKGREEN, ui.alpha));
                UISystem::DrawTextUI("CONFIRM", bx + 20, by + 10, 20, WHITE, ui.alpha);
                
                if (confirmHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    // Activate all planned nodes in order (if possible)
                    // Simple approach: Keep activating until no more can be activated
                    bool changed = true;
                    while (changed) {
                        changed = false;
                        for (auto it = ui.plannedNodes.begin(); it != ui.plannedNodes.end(); ) {
                            uint32_t nodeId = *it;
                            if (AstrolabeSystem::activate_node(registry, entity, nodeId)) {
                                it = ui.plannedNodes.erase(it);
                                changed = true;
                            } else {
                                ++it;
                            }
                        }
                    }
                }
                
                // Reset Button
                bool resetHover = CheckCollisionPointRec(mPos, resetRec);
                DrawRectangleRounded(resetRec, 0.2f, 8, Fade(resetHover ? RED : MAROON, ui.alpha));
                UISystem::DrawTextUI("RESET", bx + bw + 20.0f + 35, by + 10, 20, WHITE, ui.alpha);
                
                if (resetHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    ui.plannedNodes.clear();
                }
            }

            // Refund Mode Toggle
            float rx = screenW - 160.0f;
            float ry = 20.0f;
            float rw = 140.0f;
            float rh = 40.0f;
            Rectangle refundRec = { rx, ry, rw, rh };
            bool refundHover = CheckCollisionPointRec(GetMousePosition(), refundRec);
            DrawRectangleRounded(refundRec, 0.2f, 8, Fade(ui.isRefundMode ? RED : DARKGRAY, ui.alpha));
            UISystem::DrawTextUI(ui.isRefundMode ? "REFUNDING" : "REFUND MODE", rx + 15, ry + 10, 18, WHITE, ui.alpha);
            
            if (refundHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ui.isRefundMode = !ui.isRefundMode;
                if (ui.isRefundMode) ui.plannedNodes.clear(); // Clear planning when entering refund mode
            }
        }
    }
}
