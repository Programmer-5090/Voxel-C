#version 330 core

// Vertex attributes
layout (location = 0) in vec3 aPos;        // Position
layout (location = 1) in vec3 aNormal;     // Normal
layout (location = 2) in vec2 aTexCoord;   // Texture coordinates
layout (location = 3) in float aTextureId; // Texture ID
layout (location = 4) in float aDebugFlag; // Debug flag

// Uniforms
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Output to fragment shader
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out float TextureId;
out float DebugFlag;

void main()
{
    // Transform position to world space
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // Transform normal to world space (assuming uniform scaling)
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // Pass through texture coordinates and ID
    TexCoord = aTexCoord;
    TextureId = aTextureId;
    DebugFlag = aDebugFlag; // FIX: Actually pass through the debug flag
    
    // Final position
    gl_Position = projection * view * vec4(FragPos, 1.0);
}