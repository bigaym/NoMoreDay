#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Custom Uniforms
uniform float uTime;
uniform vec4 uColor;       // Tint Color (r, g, b, a)
uniform float uSwirlStrength; // e.g. 5.0
uniform float uCoreSize;      // e.g. 0.15

// Output fragment color
out vec4 finalColor;

#define PI 3.14159265359

void main()
{
    // 1. Center UVs: [0,1] -> [-1, 1]
    vec2 uv = fragTexCoord * 2.0 - 1.0;
    
    // 2. Polar Coordinates
    // Scale X to account for Aspect Ratio 3:5 (0.6) if we want circular vortex?
    // Actually, we want the vortex to FIT the 3:5 oval.
    // So if we render on a 3:5 quad, UVs are already stretched.
    // We just treat UV space as -1..1 and the shader output will be stretched by quad.
    
    float r = length(uv);
    float angle = atan(uv.y, uv.x);
    
    // 3. Vortex Mechanics
    // Swirl increases near center
    float twist = uSwirlStrength / (r + 0.1);
    float activeAngle = angle + twist - uTime * 2.0;
    
    // 4. Sample Noise Texture (Polar Mapping)
    // x = Angle (0..1), y = Radius (Flowing Inwards)
    vec2 polarUV;
    polarUV.x = activeAngle / (2.0 * PI); 
    polarUV.y = r - uTime * 0.5; // Flow inwards
    
    vec4 noise = texture(texture0, polarUV);
    
    // 5. Masking
    // Soft outer edge
    float outerMask = smoothstep(1.0, 0.6, r);
    // Soft core (bright center)
    float coreMask = smoothstep(0.0, uCoreSize, r);
    
    // 6. Composition
    // Boost brightness at center ring
    float brightness = 1.0 + (1.0 - smoothstep(uCoreSize, 0.6, r)) * 2.0;
    
    // Use Noise Intensity (R channel) instead of RGB to allow pure tinting
    float noiseVal = noise.r;
    
    vec3 baseColor = uColor.rgb * noiseVal * brightness;
    float alpha = uColor.a * outerMask * noise.a; // * coreMask? No, core should be bright?
    
    // Core glow (solid color in very center)
    float centerGlow = 1.0 - smoothstep(0.0, uCoreSize, r);
    baseColor += uColor.rgb * centerGlow * 2.0;
    alpha += centerGlow * uColor.a;
    
    finalColor = vec4(baseColor, alpha * fragColor.a);
}