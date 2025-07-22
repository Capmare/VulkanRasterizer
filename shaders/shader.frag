#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler texSampler;
layout(constant_id = 0) const uint TEXTURE_COUNT = 1u;

layout (push_constant) uniform constants
{
    uint Diffuse;
} textureIndices;

layout (set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];



void main() {
    vec4 texColor = texture(sampler2D(textures[nonuniformEXT(textureIndices.Diffuse)], texSampler), fragTexCoord);
    outColor = texColor;
}