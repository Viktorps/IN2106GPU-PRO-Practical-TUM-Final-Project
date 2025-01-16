#version 460


// Position , uv, normal and tangent same as the tga::vertex struct
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 tangent;

// Use this set here to for view and projection
layout(set = 0, binding = 0) uniform CameraData{
    mat4 view;
    mat4 toWorld;
    mat4 projection;
}camera;

layout(set = 1, binding = 0) readonly buffer ModelData{
    mat4 at[];
}modelMatrices;

layout(set = 1, binding = 1) readonly buffer MaterialID{
    uint at[];
}materialIDs;



layout (location = 0) out FragData{
    vec3 worldPosition;
    vec2 texCoords;
    vec3 normal;
    vec3 tangent; // Unused
    flat uint textureID;
} fragData;

void main(){
    mat4 modelMatrix = modelMatrices.at[gl_InstanceIndex];
    vec4 wPos = modelMatrix * vec4(position,1);
    fragData.worldPosition = wPos.xyz;

    fragData.texCoords = uv;
    fragData.normal = mat3(modelMatrix) * normal;
    fragData.tangent = mat3(modelMatrix) * tangent;
    fragData.textureID = materialIDs.at[gl_InstanceIndex];
    gl_Position = camera.projection * camera.view * wPos;
    

}