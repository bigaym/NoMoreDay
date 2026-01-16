# GLSL Shader Code Library

常用 VFX 算法片段库，适用于 OpenGL 4.3。可以直接复制到 Shader 文件中。

## 1. UV 操作 (UV Manipulation)

### 极坐标转换 (Polar Coordinates)
用于制作圆形扩散、旋涡、冲击波效果。
```glsl
vec2 CartesianToPolar(vec2 uv, vec2 center) {
    vec2 dir = uv - center;
    float radius = length(dir) * 2.0;
    float angle = atan(dir.y, dir.x) / 6.28318530718; // Normalize to [0, 1]
    return vec2(radius, angle);
}
// Usage: vec2 polarUV = CartesianToPolar(TexCoord, vec2(0.5));
```

### UV 滚动 (Scrolling)
用于模拟流动（水、火、烟雾）。
```glsl
vec2 ScrollUV(vec2 uv, vec2 speed, float time) {
    return uv + speed * time;
}
```

### 旋转 (Rotation)
```glsl
vec2 RotateUV(vec2 uv, vec2 pivot, float rotation) {
    float sina = sin(rotation);
    float cosa = cos(rotation);
    uv -= pivot;
    return vec2(
        uv.x * cosa - uv.y * sina,
        uv.x * sina + uv.y * cosa
    ) + pivot;
}
```

## 2. 遮罩与形状 (Masks & Shapes)

### 软圆形 (Soft Circle)
```glsl
float Circle(vec2 uv, float radius, float softness) {
    float dist = length(uv - 0.5);
    return 1.0 - smoothstep(radius - softness, radius, dist);
}
```

### 菲涅尔效应 (Fresnel)
用于 3D 模型边缘发光（需法线）。
```glsl
float Fresnel(vec3 normal, vec3 viewDir, float power) {
    return pow(1.0 - max(dot(normal, viewDir), 0.0), power);
}
```

### 深度溶解 (Soft Particles / Depth Fade)
防止粒子与几何体穿插时的硬边（需线性深度缓冲区）。
```glsl
float DepthFade(float sceneDepth, float fragmentDepth, float softness) {
    return clamp((sceneDepth - fragmentDepth) / softness, 0.0, 1.0);
}
```

## 3. 噪声与随机 (Noise)

### 伪随机 (Pseudo Random)
```glsl
float Random(vec2 uv) {
    return fract(sin(dot(uv.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}
```

## 4. 风格化滤镜 (Stylization)

### 水墨边缘检测 (Sobel Edge)
```glsl
float Sobel(sampler2D tex, vec2 uv, vec2 texSize) {
    vec2 d = 1.0 / texSize;
    float s1 = texture(tex, uv + vec2(-d.x, -d.y)).r;
    float s2 = texture(tex, uv + vec2(0,    -d.y)).r;
    float s3 = texture(tex, uv + vec2(d.x,  -d.y)).r;
    float s4 = texture(tex, uv + vec2(-d.x,  0)).r;
    float s6 = texture(tex, uv + vec2(d.x,   0)).r;
    float s7 = texture(tex, uv + vec2(-d.x,  d.y)).r;
    float s8 = texture(tex, uv + vec2(0,     d.y)).r;
    float s9 = texture(tex, uv + vec2(d.x,   d.y)).r;
    
    float x = s1 + 2.0*s6 + s9 - (s3 + 2.0*s4 + s7);
    float y = s1 + 2.0*s2 + s3 - (s7 + 2.0*s8 + s9);
    
    return sqrt(x*x + y*y);
}
```
