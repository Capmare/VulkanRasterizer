#version 450

layout(location = 0) in vec3 inPosition;


layout(std140, binding = 5) uniform shadowUBO {
        mat4 lightViewProj;
} ubo;

void main() {
        gl_Position = ubo.lightViewProj * vec4(inPosition, 1.0);
}
