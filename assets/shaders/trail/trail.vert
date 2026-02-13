#version 430

struct TrailPoint {
    float posX, posY;
    float dirX, dirY;
    float width;
    float lifetime;
    uint colorPacked;
    uint flags;
};

struct TrailHeader {
    int headIndex;
    int pointCount;
    int maxPoints;
    float maxLifetime;
    float widthStart;
    float widthEnd;
    uint colorStart;
    uint colorEnd;
};

layout(std430, binding = 10) readonly buffer TrailHeaders {
    TrailHeader headers[];
};

layout(std430, binding = 11) readonly buffer TrailPoints {
    TrailPoint points[];
};

uniform mat4 mvp;
uniform int trailIndex;
uniform int uMaxPointsPerTrail;

out float vProgress;
out vec4 vColor;
out vec2 vTexCoord;

void main() {
    TrailHeader h = headers[trailIndex];
    if (h.pointCount < 2 || h.maxPoints <= 1) {
        gl_Position = vec4(-9999.0, -9999.0, 0.0, 1.0);
        return;
    }

    int pointIdx = gl_VertexID / 2;
    int side = (gl_VertexID % 2 == 0) ? 1 : -1;

    int ringIdx = (h.headIndex - pointIdx + h.maxPoints) % h.maxPoints;
    int globalIdx = trailIndex * uMaxPointsPerTrail + ringIdx;

    TrailPoint pt = points[globalIdx];

    float progress = float(pointIdx) / float(h.pointCount - 1);
    vProgress = progress;

    float w = mix(h.widthStart, h.widthEnd, progress) * 0.5;

    vec2 normal = vec2(-pt.dirY, pt.dirX);
    vec2 worldPos = vec2(pt.posX, pt.posY) + normal * w * float(side);

    vec4 startCol = unpackUnorm4x8(h.colorStart);
    vec4 endCol = unpackUnorm4x8(h.colorEnd);
    vColor = mix(startCol, endCol, progress);

    vTexCoord = vec2(progress, float(side) * 0.5 + 0.5);

    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
