#version 430 core

in vec2 vTexCoord;
in vec2 vLocalPos;
flat in uint vTextureIndex;
flat in uint vFlags;
flat in float vGlow;

out vec4 fragColor;

void main() {
    // Check flags (GPU_ENTITY_FLAG_NO_RENDER = 2)
    if ((vFlags & 2u) != 0u) discard;

    // Current fallback: Circle SDF (matches legacy GPUEntitySystem)
    float distSq = dot(vLocalPos, vLocalPos);
    if (distSq > 1.0) discard;
    
    float delta = fwidth(distSq);
    float alpha = 1.0 - smoothstep(1.0 - delta, 1.0, distSq);
    
    // Default color (Red for enemies)
    // In future, vTextureIndex can sample a TextureArray
    vec3 color = vec3(1.0, 0.3, 0.3);
    
    // Simple debug visualization for other types if needed (e.g. textureIndex > 0)
    if (vTextureIndex == 1) color = vec3(0.3, 1.0, 0.3);

    // Apply Glow (Visual Stats)
    if (vGlow > 0.0) {
        color += vec3(vGlow * 0.8, vGlow * 0.8, vGlow * 1.0); // Blue-ish tint for barrier/glow
    }
    
    fragColor = vec4(color, alpha);
}
