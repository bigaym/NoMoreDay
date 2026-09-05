#version 430 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

struct TextQuadDrawData {
    float screenPosX;
    float screenPosY;
    float sizeX;
    float sizeY;
    float uvMinX;
    float uvMinY;
    float uvMaxX;
    float uvMaxY;
    uint colorPacked;
    float opacity;
};

// 与 RenderConstants.hpp 的 Binding::SSBO_TEXT_QUAD (=16) 保持一致，
// 与 CPU 标签管线的 SSBO_GLYPH_INSTANCE (=8) 相互独立。
layout(std430, binding = 16) readonly buffer TextQuadBuffer {
    TextQuadDrawData quads[];
};

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec4 fragColor;

void main() {
    TextQuadDrawData q = quads[gl_InstanceID];
    vec2 worldPos = vec2(q.screenPosX, q.screenPosY) + aPos * vec2(q.sizeX, q.sizeY);
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);

    vec2 uvMin = vec2(q.uvMinX, q.uvMinY);
    vec2 uvMax = vec2(q.uvMaxX, q.uvMaxY);
    fragTexCoord = mix(uvMin, uvMax, aTexCoord);

    vec4 color = vec4(
        float(q.colorPacked & 0xFFu) / 255.0,
        float((q.colorPacked >> 8u) & 0xFFu) / 255.0,
        float((q.colorPacked >> 16u) & 0xFFu) / 255.0,
        float((q.colorPacked >> 24u) & 0xFFu) / 255.0
    );
    color.a *= q.opacity;
    fragColor = color;
}
