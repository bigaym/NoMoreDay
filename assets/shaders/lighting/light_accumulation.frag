#version 430 core

in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D uSceneTex;
uniform sampler2D uShadowMaskTex;
uniform vec3 uAmbientColor;
uniform float uAmbientIntensity;
uniform int uLightCount;
uniform int uShadowEnabled;
uniform int uClusteredLightingEnabled;
uniform int uClusterGridX;
uniform int uClusterGridY;
uniform int uClusterGridZ;
uniform float uClusterTileSizeWorld;
uniform float uLayerBandWorldUnits;
uniform vec2 uCameraOffset;
uniform vec2 uScreenSize;

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
    uint lightType;
};

layout(std430, binding = 9) readonly buffer LightBuffer {
    GPULight lights[];
};

struct ClusterHeaderData {
    uint offset;
    uint count;
    uint overflowCount;
    uint reserved;
};

struct ClusterLightIndexData {
    uint lightIndex;
};

layout(std430, binding = 1) readonly buffer ClusterHeaderBuffer {
    ClusterHeaderData clusterHeaders[];
};

layout(std430, binding = 2) readonly buffer ClusterIndexBuffer {
    ClusterLightIndexData clusterIndices[];
};

const int kRenderLayerMin = -32;
const int kRenderLayerSpan = 64;

float calcAttenuation(float dist, float radius) {
    float normalizedDist = dist / radius;
    if (normalizedDist >= 1.0) {
        return 0.0;
    }
    float d2 = normalizedDist * normalizedDist;
    float atten = 1.0 - d2;
    return atten * atten;
}

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

void accumulateSingleLight(vec2 worldPos, float shadowFactor, uint lightIndex,
                           inout vec3 totalLight) {
    if (lightIndex >= uint(uLightCount)) {
        return;
    }

    vec2 lightPos = vec2(lights[lightIndex].posX, lights[lightIndex].posY);
    float radius = lights[lightIndex].radius;
    float intensity = lights[lightIndex].intensity;
    vec3 lightColor = vec3(lights[lightIndex].colorR, lights[lightIndex].colorG,
                           lights[lightIndex].colorB);
    uint lightType = lights[lightIndex].lightType;

    float dist = distance(worldPos, lightPos);
    float atten = calcAttenuation(dist, radius);
    if (atten <= 0.0 || intensity <= 0.0) {
        return;
    }

    if (lightType == 2u) {
        totalLight += lightColor * intensity * atten;
        return;
    }

    float spotFactor = 1.0;
    if (lightType == 1u) {
        vec2 toPixel = worldPos - lightPos;
        spotFactor = calcSpotFactor(vec2(lights[lightIndex].dirX, lights[lightIndex].dirY),
                                    toPixel, lights[lightIndex].spotCosHalfAngle);
        if (spotFactor <= 0.0) {
            return;
        }
    }

    totalLight += lightColor * intensity * atten * spotFactor * shadowFactor;
}

void main() {
    vec4 sceneColor = texture(uSceneTex, vTexCoord);
    vec2 worldPos = vTexCoord * uScreenSize + uCameraOffset;

    vec3 totalLight = uAmbientColor * uAmbientIntensity;
    float shadowFactor = 1.0;
    if (uShadowEnabled != 0) {
        shadowFactor = texture(uShadowMaskTex, vTexCoord).r;
    }

    if (uClusteredLightingEnabled != 0) {
        int clusterId = computeClusterId(worldPos);
        if (clusterId >= 0) {
            ClusterHeaderData header = clusterHeaders[clusterId];
            for (uint i = 0u; i < header.count; ++i) {
                uint lightIndex = clusterIndices[header.offset + i].lightIndex;
                accumulateSingleLight(worldPos, shadowFactor, lightIndex, totalLight);
            }
        }
    } else {
        for (int i = 0; i < uLightCount; ++i) {
            accumulateSingleLight(worldPos, shadowFactor, uint(i), totalLight);
        }
    }

    fragColor = vec4(sceneColor.rgb * totalLight, sceneColor.a);
}
