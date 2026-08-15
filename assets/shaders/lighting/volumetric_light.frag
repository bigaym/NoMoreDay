#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;
uniform int uSampleCount;
uniform float uScattering;
uniform float uDecay;
uniform float uExposure;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;
uniform int uClusterGridX;
uniform int uClusterGridY;
uniform int uClusterGridZ;
uniform float uClusterTileSizeWorld;
uniform float uLayerBandWorldUnits;

struct GPULight {
    float posX;
    float posY;
    float radius;
    float intensity;
    float colorR;
    float colorG;
    float colorB;
    float colorA;
    float dirX;
    float dirY;
    float spotCosHalfAngle;
    float spotOuterCos;
    uint lightType;
    uint shadowMapIndex;
    uint priority;
    uint flags;
};

layout(std430, binding = 9) readonly buffer LightBuffer {
    GPULight lights[];
};

struct ClusterHeaderData {
    uint offset;
    uint pointCount;
    uint spotCount;
    uint areaCount;
};

layout(std430, binding = 1) readonly buffer ClusterHeaderBuffer {
    ClusterHeaderData clusterHeaders[];
};

struct ClusterLightIndexData {
    uint lightIndex;
};

layout(std430, binding = 2) readonly buffer ClusterIndexBuffer {
    ClusterLightIndexData clusterIndices[];
};

const int kRenderLayerMin = -32;
const int kRenderLayerSpan = 64;

float calcSpotFactor(vec2 lightDir, vec2 toPixelDir, float spotCosHalfAngle) {
    if (spotCosHalfAngle <= -0.9999) {
        return 1.0;
    }
    float cone = dot(normalize(lightDir), normalize(toPixelDir));
    if (cone <= spotCosHalfAngle) {
        return 0.0;
    }
    float denom = max(1e-4, 1.0 - spotCosHalfAngle);
    float t = clamp((cone - spotCosHalfAngle) / denom, 0.0, 1.0);
    return t * t;
}

int mapRenderLayerToSlice(int layer, int slices) {
    if (slices <= 0) {
        return 0;
    }
    int normalized = clamp(layer - kRenderLayerMin, 0, kRenderLayerSpan - 1);
    return (normalized * slices) / kRenderLayerSpan;
}

int computeClusterId(vec2 worldPos) {
    if (uClusterGridX <= 0 || uClusterGridY <= 0 || uClusterGridZ <= 0 ||
        uClusterTileSizeWorld <= 0.0) {
        return -1;
    }

    vec2 local = (worldPos - uCameraOffset) / uClusterTileSizeWorld;
    int tileX = clamp(int(floor(local.x)), 0, uClusterGridX - 1);
    int tileY = clamp(int(floor(local.y)), 0, uClusterGridY - 1);

    float layerBand = max(uLayerBandWorldUnits, 1e-4);
    int renderLayer = int(floor(worldPos.y / layerBand));
    int slice = clamp(mapRenderLayerToSlice(renderLayer, uClusterGridZ), 0, uClusterGridZ - 1);
    return tileX + tileY * uClusterGridX + slice * uClusterGridX * uClusterGridY;
}

void main() {
    vec3 scene = texture(uSceneTex, vTexCoord).rgb;
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    vec3 scatterSum = vec3(0.0);
    int sampleCount = max(uSampleCount, 1);

    // Clustered lighting is the single production path (Phase E): the pass
    // fail-closes unless cluster data is available, so iterate the current
    // cluster's light indices instead of the full light list.
    int clusterId = computeClusterId(worldPos);
    if (clusterId >= 0) {
        ClusterHeaderData header = clusterHeaders[clusterId];
        uint totalCount = header.pointCount + header.spotCount + header.areaCount;
        for (uint c = 0u; c < totalCount; ++c) {
            uint lightIndex = clusterIndices[header.offset + c].lightIndex;
            vec2 lightPos = vec2(lights[lightIndex].posX, lights[lightIndex].posY);
            float radius = max(lights[lightIndex].radius, 1e-4);
            float intensity = max(lights[lightIndex].intensity, 0.0);
            vec3 lightColor = vec3(lights[lightIndex].colorR, lights[lightIndex].colorG,
                                   lights[lightIndex].colorB);
            uint lightType = lights[lightIndex].lightType;

            vec2 toLight = lightPos - worldPos;
            float distToLight = length(toLight);
            if (distToLight >= radius || intensity <= 0.0) {
                continue;
            }

            float normalizedDist = distToLight / radius;
            float boundaryFade = smoothstep(1.0, 0.6, normalizedDist);

            vec2 stepDir = toLight / float(sampleCount);
            vec2 samplePos = worldPos;
            float weight = 1.0;
            vec3 lightScatter = vec3(0.0);
            for (int s = 0; s < sampleCount; ++s) {
                float dist = length(samplePos - lightPos);
                float radial = 1.0 - clamp(dist / radius, 0.0, 1.0);
                radial = radial * radial;
                float spotFactor = 1.0;
                if (lightType == 1u) {
                    vec2 toSample = samplePos - lightPos;
                    spotFactor = calcSpotFactor(vec2(lights[lightIndex].dirX, lights[lightIndex].dirY),
                                                toSample, lights[lightIndex].spotCosHalfAngle);
                    if (spotFactor <= 0.0) {
                        samplePos += stepDir;
                        weight *= uDecay;
                        continue;
                    }
                }
                lightScatter += lightColor * (intensity * radial * weight * spotFactor);
                weight *= uDecay;
                samplePos += stepDir;
            }

            scatterSum += (lightScatter / float(sampleCount)) * boundaryFade;
        }
    }

    vec3 result = scene + scatterSum * uScattering * uExposure;
    fragColor = vec4(result, 1.0);
}
