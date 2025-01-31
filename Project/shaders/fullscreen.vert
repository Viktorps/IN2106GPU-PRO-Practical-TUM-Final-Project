#version 450

layout(location = 0) out FragData {
    vec2 uv;              // UV coordinates
    vec4 lightSpacePos;   // Light-space position for shadow mapping
} fragData;


void main() {
    fragData.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2); // Generate UVs
    gl_Position = vec4(fragData.uv * 2.0 - 1.0, 0.0, 1.0);            // Fullscreen quad
}
