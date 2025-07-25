#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outMaterial;

layout(set = 1, binding = 0) uniform sampler texSampler;
layout(constant_id = 0) const uint TEXTURE_COUNT = 1u;

layout(push_constant) uniform constants {
    uint Diffuse;
    uint Normal;
    uint Metallic;
    uint Roughness;
    uint AO;
    uint Emmisive;
} material;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;


layout(set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];


void main() {
    const float alphaThreshold = 0.99;
    vec4 albedoSample = texture(sampler2D(textures[nonuniformEXT(material.Diffuse)], texSampler), fragTexCoord);
    if (albedoSample.a < alphaThreshold)
    discard;

    vec3 albedo = albedoSample.rgb;

    float metallic = texture(sampler2D(textures[nonuniformEXT(material.Metallic)], texSampler), fragTexCoord).b;
    float roughness = texture(sampler2D(textures[nonuniformEXT(material.Roughness)], texSampler), fragTexCoord).g;
    float ao = texture(sampler2D(textures[nonuniformEXT(material.AO)], texSampler), fragTexCoord).r;

    vec3 normal = texture(sampler2D(textures[nonuniformEXT(material.Normal)], texSampler), fragTexCoord).rgb;
    normal = normal * 2.0 - vec3(1.0, 1.0, 1.0);

    outAlbedo   = vec4(albedo, 1.0);
    outNormal   = vec4(normal * 0.5 + 0.5, 1.0);
    outMaterial = vec4(metallic, roughness, ao, 1.0);
}
