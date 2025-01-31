#version 450

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 toWorld;
    mat4 projection;
    vec3 cameraPosition;
} camera;

layout(set = 0, binding = 1) uniform LightData {
    vec4 color_intensity;  // (R, G, B, Intensity)
    vec3 direction;        // Directional light direction
    mat4 lightSpaceMatrix; // Light space transformation matrix
} sunlight;

layout(set = 1, binding = 0) uniform sampler2D albedo;
layout(set = 1, binding = 1) uniform sampler2D normal;
layout(set = 1, binding = 2) uniform sampler2D position;

layout(location = 0) in FragData {
    vec2 uv;              // UV coordinates
    vec4 lightSpacePos;   // Light-space position
} fragData;

layout(location = 0) out vec4 color;

void main() {
    vec4 objectColor = texture(albedo, fragData.uv);

    // Step 1: Constant sky color
    vec3 skyColor = vec3(0.53, 0.81, 0.92); // Bright sky blue

    if (objectColor.a < 0.1) { 
        color = vec4(skyColor, 1.0); // Use sky color for transparent areas
        return;
    }

    // Step 2: Lighting computations
    vec3 N = normalize(texture(normal, fragData.uv).xyz); // Normal from G-buffer
    vec3 P = texture(position, fragData.uv).xyz;          // World position from G-buffer
    vec3 L = normalize(sunlight.direction);              // Light direction
    vec3 V = normalize(camera.cameraPosition - P);       // View direction

    // Diffuse lighting
    float NdL = max(dot(N, L), 0.0);
    vec3 diffuse = NdL * sunlight.color_intensity.rgb * sunlight.color_intensity.w * objectColor.rgb;

    // Ambient lighting
    vec3 ambient = vec3(0.1) * objectColor.rgb;

  
    // Combine lighting with shadow factor
    vec3 finalColor = ambient + diffuse;
    color = vec4(finalColor, objectColor.a);
}
