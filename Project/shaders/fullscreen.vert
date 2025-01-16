#version 450

layout (location = 0) out FragData {
    vec2 uv;
} fragData;

void main() {
    fragData.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragData.uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
