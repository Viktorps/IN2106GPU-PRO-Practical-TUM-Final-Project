#version 450

layout (location = 0) in FragData {
    vec2 uv;
} fragData;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 toWorld;
    mat4 projection;
    vec3 cameraPosition;
} camera;

struct Light {
    vec4 color_intensity; // (R, G, B, Intensity)
    vec3 position;        // Light position (e.g., the sun in world space)
    vec3 direction;       // Directional light direction
};

layout(set = 0, binding = 1) uniform LightData {
    Light at[128]; // Array of lights, we use only at[0] for sunlight
} lights;

layout(set = 1, binding = 0) uniform sampler2D albedo;
layout(set = 1, binding = 1) uniform sampler2D normal;
layout(set = 1, binding = 2) uniform sampler2D position;


layout(location = 0) out vec4 color;

void main() {
    vec4 objectColor = texture(albedo, fragData.uv);
    vec3 skyColor = vec3(0.36, 0.45, 0.57); // Fallback color for missing textures
    if (objectColor.a < 0.1) { 
        color = vec4(skyColor, 1.0);
        return;
    }

    vec3 N = normalize(texture(normal, fragData.uv).xyz); // Normal from G-buffer
    vec3 P = texture(position, fragData.uv).xyz;          // World position from G-buffer

    // Calculate light direction
    Light sunlight = lights.at[0];
    vec3 L = normalize(sunlight.position - P); // Vector from fragment to light (point light behavior)

    // Calculate view direction
    vec3 V = normalize(camera.cameraPosition - P);

    // Diffuse lighting
    float NdL = max(dot(N, L), 0.0); // Diffuse term
    vec3 diffuse = NdL * sunlight.color_intensity.rgb * sunlight.color_intensity.w * objectColor.rgb;

    // Ambient lighting (constant ambient color for now)
    vec3 ambient = vec3(0.1) * objectColor.rgb;

    // Combine lighting
    vec3 finalColor = ambient + diffuse;
    color = vec4(finalColor, objectColor.a);
}
