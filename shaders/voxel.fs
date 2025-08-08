#version 330 core

// Input from vertex shader
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in float TextureId;
in float DebugFlag;

// Uniforms
uniform sampler2D texture_atlas;
uniform float time;
uniform int renderPass; // 0 for opaque pass, 1 for transparent pass

// Output
out vec4 FragColor;

// Lighting parameters
const vec3 lightPos = vec3(100.0, 200.0, 100.0);
const vec3 lightColor = vec3(1.0, 1.0, 0.9);
const vec3 ambientColor = vec3(0.3, 0.3, 0.4);

void main()
{
    int textureIndex = int(TextureId);
    
    // Determine if the current fragment belongs to a transparent block (water)
    bool is_transparent = (textureIndex >= 10 && textureIndex <= 41);

    // Early discard for better performance
    if (renderPass == 0 && is_transparent) { // Opaque pass: discard transparent fragments
        discard;
    }
    if (renderPass == 1 && !is_transparent) { // Transparent pass: discard opaque fragments
        discard;
    }

    // Calculate texture coordinates for atlas
    const float tileSize = 1.0 / 9.0;
    const float tileHeightSize = 1.0 / 5.0;
    
    // Handle water animation more efficiently
    if (textureIndex >= 10 && textureIndex <= 41) {
        // Use faster animation calculation
        int animFrame = int(time * 2.0) & 31; // Bitwise AND for modulo with power of 2
        textureIndex = 10 + animFrame;
    }
    
    // Calculate tile position in atlas
    int tilesPerRow = 9;
    int tileX = textureIndex % tilesPerRow;
    int tileY = textureIndex / tilesPerRow;
    vec2 atlasCoord = vec2(tileX * tileSize, tileY * tileHeightSize) + TexCoord * vec2(tileSize, tileHeightSize);
    
    vec4 texColor = texture(texture_atlas, atlasCoord);
    
    // Early alpha test for better performance
    if (texColor.a < 0.1) {
        discard;
    }
    
    // Apply transparency for water
    if (is_transparent) {
        texColor.a = 0.75;
    }
    
    // Optimized lighting calculation
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    
    // Combine lighting more efficiently
    vec3 result = texColor.rgb * (ambientColor + diff * lightColor);
    
    FragColor = vec4(result, texColor.a);
}