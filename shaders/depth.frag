#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;
layout(location = 4) out vec3 outFragTangent;
layout(location = 5) out vec3 outFragBitangent;


layout(set = 1, binding = 0) uniform sampler texSampler;

layout(push_constant) uniform constants {
    uint Diffuse;
    uint Normal;
    uint Metallic;
    uint Roughness;
    uint AO;
    uint Emmisive;
} material;

layout(constant_id = 0) const uint TEXTURE_COUNT = 1u;
layout(set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];


struct PointLight
{
    vec4 Position; // w is unused
    vec4 Color; // w is intensity
};

//layout(constant_id = 2) const uint MAX_LIGHTS = 1;
layout(set = 1, binding = 2) uniform LightBuffer {
    PointLight pointLights[1];
} lightBuffer;


void main() {
    const float alphaThreshold = 0.99f;
    if (texture(sampler2D(textures[nonuniformEXT(material.Diffuse)], texSampler), inTexCoord).a < alphaThreshold)
       discard;

}

