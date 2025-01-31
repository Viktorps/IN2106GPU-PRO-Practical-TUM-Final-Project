#version 460
#extension GL_EXT_nonuniform_qualifier: enable

layout (location = 0) in FragData {
    vec3 worldPosition;
    vec2 texCoords;
    vec3 normal;
    vec3 tangent; // Unused
    uint textureID;
} fragData;



layout(set = 1, binding = 2) uniform sampler2D colorTex[];

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 toWorld;
    mat4 projection;
    vec3 cameraPosition; // Pass this as an additional uniform
};

layout(location = 0) out vec4 albedo;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec3 position;

void main() {
    // Texture lookup and other fragment processing
    if (fragData.textureID == 9) {
        albedo = vec4(texture(colorTex[fragData.textureID], fragData.texCoords).rgb, 0.1);
    } else {
        albedo = vec4(texture(colorTex[fragData.textureID], fragData.texCoords).rgb, 1.0);
    }

    normal = normalize(fragData.normal); // TODO: Normal mapping
    position = fragData.worldPosition;
    
    
}
