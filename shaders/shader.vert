#version 450

layout(location = 0) in vec2 vertexPos;
layout(location = 1) in vec3 vertexColor;

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
        mat4 model;
        mat4 view;
        mat4 proj;
} ubo;

void main() {
        vec4 pos = vec4(vertexPos, 0.0, 1.0);
        gl_Position = ubo.proj * ubo.view * ubo.model * pos;
        fragColor = vertexColor;
}
