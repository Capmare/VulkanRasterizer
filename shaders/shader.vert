#version 450

layout(location = 0) in vec3 vertexPos;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec2 vertexTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
        mat4 model;
        mat4 view;
        mat4 proj;
} ubo;

void main() {
        gl_Position = ubo.proj * ubo.view * ubo.model * vec4(vertexPos, 1.0);
        fragColor = vertexColor;
        fragTexCoord = vertexTexCoord;
}
