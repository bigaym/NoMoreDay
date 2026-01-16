import argparse
import os

TEMPLATE_HPP = """#pragma once
#include "pch.hpp"
#include "engine/render/GPUParticleSystem.hpp"

struct {name}Component {{
    // Add specific gameplay data here
    float duration;
    float time_elapsed;
}};

class {name}System : public GPUParticleSystem {{
public:
    explicit {name}System(entt::registry& registry);
    ~{name}System() override = default;

    void Update(float dt) override;
    void Render(const Camera2D& camera) override;

private:
    void InitializeParticles() override;
    
    // Shader & Buffer handles
    Shader compute_shader_;
    Shader render_shader_;
    unsigned int ssbo_[2]; // Double buffering
    unsigned int vao_;
}};
"""

TEMPLATE_CPP = """#include "{name}System.hpp"
#include "game/components/Common.hpp"

{name}System::{name}System(entt::registry& registry)
    : GPUParticleSystem(registry) {{
    InitializeParticles();
}}

void {name}System::InitializeParticles() {{
    // 1. Load Shaders
    compute_shader_ = LoadShader(0, "assets/shaders/vfx/{filename}.compute");
    render_shader_ = LoadShader("assets/shaders/vfx/{filename}.vert", "assets/shaders/vfx/{filename}.frag");

    // 2. Init SSBOs (Example size)
    const int max_particles = 10000;
    size_t data_size = max_particles * sizeof(ParticleData); // Ensure ParticleData is defined in a common header or here

    glGenBuffers(2, ssbo_);
    for(int i=0; i<2; i++) {{
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_[i]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, data_size, nullptr, GL_DYNAMIC_DRAW);
    }}
    
    // 3. Setup VAO for rendering (if needed)
    glGenVertexArrays(1, &vao_);
    // ... setup vertex attributes mapping to SSBO if using array buffer binding
}}

void {name}System::Update(float dt) {{
    // Dispatch Compute Shader
    glUseProgram(compute_shader_.id);
    
    // Bind Buffers (Ping-Pong)
    // ...
    
    glDispatchCompute(group_count_x, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}}

void {name}System::Render(const Camera2D& camera) {{
    glUseProgram(render_shader_.id);
    // ... Set uniforms
    
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, particle_count);
}}
"""

TEMPLATE_COMPUTE = """#version 430

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    vec2 position;
    vec2 velocity;
    vec4 color;
    float life;
    float scale;
};

layout(std430, binding = 0) readonly buffer InputBuffer {
    Particle particlesIn[];
};

layout(std430, binding = 1) writeonly buffer OutputBuffer {
    Particle particlesOut[];
};

uniform float dt;

void main() {
    uint id = gl_GlobalInvocationID.x;
    Particle p = particlesIn[id];

    if (p.life > 0.0) {
        // Physics update
        p.position += p.velocity * dt;
        p.life -= dt;
        p.color.a = smoothstep(0.0, 0.2, p.life); // Fade out
    }

    particlesOut[id] = p;
}
"""

def main():
    parser = argparse.ArgumentParser(description="Generate C++ VFX System boilerplate.")
    parser.add_argument('name', help="Name of the system (e.g., RendingWave)")
    parser.add_argument('--out_dir', default='src/game/systems/vfx', help="Output directory for C++ files")
    
    args = parser.parse_args()
    
    name = args.name
    filename = name  # Or snake_case if preferred
    
    # Ensure directories
    os.makedirs(args.out_dir, exist_ok=True)
    shader_dir = "assets/shaders/vfx"
    os.makedirs(shader_dir, exist_ok=True)
    
    # Write HPP
    with open(os.path.join(args.out_dir, f"{name}System.hpp"), 'w') as f:
        f.write(TEMPLATE_HPP.format(name=name))
        
    # Write CPP
    with open(os.path.join(args.out_dir, f"{name}System.cpp"), 'w') as f:
        f.write(TEMPLATE_CPP.format(name=name, filename=filename))

    # Write Compute Shader (if not exists)
    cs_path = os.path.join(shader_dir, f"{filename}.compute")
    if not os.path.exists(cs_path):
        with open(cs_path, 'w') as f:
            f.write(TEMPLATE_COMPUTE)
            
    print(f"✅ Generated VFX System for '{name}':")
    print(f"  - C++: {args.out_dir}/{name}System.hpp/cpp")
    print(f"  - Shader: {cs_path}")

if __name__ == "__main__":
    main()
