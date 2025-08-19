#version 450
#extension GL_EXT_nonuniform_qualifier : require


layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

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

struct PointLight { vec4 Position; vec4 Color; };
layout(set = 1, binding = 2) uniform LightBuffer {
    PointLight pointLights[1];
} lightBuffer;

void main() {
    // Alpha cutout
    const float alphaThreshold = 0.99;
    vec4 albedoSample = texture(sampler2D(textures[nonuniformEXT(material.Diffuse)], texSampler), inTexCoord);
    if (albedoSample.a < alphaThreshold) discard;

    vec3 albedo    = albedoSample.rgb;
    float metallic = texture(sampler2D(textures[nonuniformEXT(material.Metallic)],  texSampler), inTexCoord).b;
    float roughness= texture(sampler2D(textures[nonuniformEXT(material.Roughness)], texSampler), inTexCoord).g;
    float ao       = texture(sampler2D(textures[nonuniformEXT(material.AO)],        texSampler), inTexCoord).r;

    vec3 n_ts = texture(sampler2D(textures[nonuniformEXT(material.Normal)], texSampler), inTexCoord).rgb * 2.0 - 1.0;


    vec3 Tw = normalize(inTangent);
    vec3 Bw = normalize(inBitangent);
    vec3 Nw = normalize(inNormal);
    mat3 TBN = mat3(Tw, Bw, Nw);

    vec3 worldNormal = normalize(TBN * n_ts);
    vec3 packedNormal = worldNormal * 0.5 + 0.5;

    outAlbedo   = vec4(albedo, 1.0);
    outNormal   = vec4(packedNormal, 1.0);
    outMaterial = vec4(metallic, roughness, ao, 1.0);
}
