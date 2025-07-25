#version 450

layout(location = 0) in vec3 vertexPos;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec2 vertexTexCoord;
layout(location = 3) in vec3 vertexNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
        mat4 model;
        mat4 view;
        mat4 proj;
        vec3 cameraPos;
} ubo;

void main() {
        vec4 worldPos = ubo.model * vec4(vertexPos, 1.0);
        gl_Position = ubo.proj * ubo.view * worldPos;

        fragColor = vertexColor;
        fragTexCoord = vertexTexCoord;
        fragWorldPos = worldPos.xyz;
        fragNormal = normalize(mat3(ubo.model) * vertexNormal);
}


