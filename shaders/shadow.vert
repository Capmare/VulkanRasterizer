#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;



layout(std140, binding = 5) uniform shadowUBO {
        mat4 view;
        mat4 proj;
        mat4 vp;
        vec3 center;
        float _pad0;
        vec2 NearFarPlanes;
        vec2 _pad1;
} ubo;

void main() {
        gl_Position = ubo.vp * vec4(inWorldPos, 1.0);
}
