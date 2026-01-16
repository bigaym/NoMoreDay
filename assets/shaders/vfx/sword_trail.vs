#version 430

// Vertex attributes: standard raylib layout
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoord;
layout(location = 2) in vec4 vertexColor;

out vec2 fragTexCoord;
out vec4 fragColor;

uniform mat4 mvp;
uniform float time;
uniform float scrollSpeed;

void main()
{
    fragTexCoord = vertexTexCoord;
    
    // UV Scrolling: Animate the trail texture along its length
    // This makes the "slash" feel like energy moving through the air
    fragTexCoord.x += time * scrollSpeed;
    
    fragColor = vertexColor;
    
    // Final position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
