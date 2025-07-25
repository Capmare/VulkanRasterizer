#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler texSampler;
layout(location = 1) in vec2 fragTexCoord;

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

void main() {
    const float alphaThreshold = 0.99f;
    if (texture(sampler2D(textures[nonuniformEXT(material.Diffuse)], texSampler), fragTexCoord).a < alphaThreshold)
       discard;

}

