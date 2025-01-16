#version 450

layout (location = 0) in FragData {
    vec2 uv;
} fragData;

layout(set = 0, binding = 0) uniform sampler2D hdr;

layout(location = 0) out vec4 color;

void main() {
    vec3 col = texture(hdr, fragData.uv).rgb;

    // Tone mapping using extended Reinhard method
    const float luminance = dot(vec3(1.64, 1.27, 0.99), vec3(0.2126, 0.7152, 0.0722));
    const float maxWhite = luminance * 128 * 2; // luminance * number * intensity  
    col = (col * (vec3(1) + (col / vec3(maxWhite * maxWhite)))) / (col + vec3(1));

    // Add crosshair
    vec2 center = vec2(0.5, 0.5); // Center of the screen
    vec2 crosshairSize = vec2(0.005, 0.002); // Size of the crosshair lines (horizontal and vertical)

    // Check if the fragment is within the horizontal or vertical line of the crosshair
    if ((abs(fragData.uv.x - center.x) < crosshairSize.x && abs(fragData.uv.y - center.y) < crosshairSize.y) || 
        (abs(fragData.uv.y - center.y) < crosshairSize.x && abs(fragData.uv.x - center.x) < crosshairSize.y)) {
        col = vec3(1.0, 1.0, 1.0); // Color of the crosshair (white)
    }

    color = vec4(col, 1.0);
}


