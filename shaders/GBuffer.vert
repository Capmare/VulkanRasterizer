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


layout(set = 0, binding = 0) uniform UniformBufferObject {
        mat4 model;
        mat4 view;
        mat4 proj;
        vec3 cameraPos;
} ubo;

void main() {
        vec4 worldPos = ubo.model * vec4(inWorldPos, 1.0);
        gl_Position = ubo.proj * ubo.view * worldPos;

        outColor = inColor;
        outTexCoord = inTexCoord;
        outWorldPos = worldPos.xyz;
        mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
        outNormal = normalize(normalMatrix * inNormal);
        outTangent = normalize(normalMatrix * inTangent);
        outBitangent = normalize(normalMatrix * inBitangent);

}


