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


void main() {


    const float alphaThreshold = 0.99;
    vec4 albedoSample = texture(sampler2D(textures[nonuniformEXT(material.Diffuse)], texSampler), inTexCoord);
    if (albedoSample.a < alphaThreshold)
    discard;

    vec3 albedo = albedoSample.rgb;

    float metallic = texture(sampler2D(textures[nonuniformEXT(material.Metallic)], texSampler), inTexCoord).b;
    float roughness = texture(sampler2D(textures[nonuniformEXT(material.Roughness)], texSampler), inTexCoord).g;
    float ao = texture(sampler2D(textures[nonuniformEXT(material.AO)], texSampler), inTexCoord).r;

    // Sample tangent-space normal map
    vec3 normal = texture(sampler2D(textures[nonuniformEXT(material.Normal)], texSampler), inTexCoord).rgb;
    normal = normal * 2.0 - 1.0;

    // Build TBN matrix
    vec3 T = normalize(vec3(ubo.model * vec4(inTangent, 0.0)));
    vec3 N = normalize(vec3(ubo.model * vec4(inNormal, 0.0)));
    vec3 B = cross(N, T);
    T = normalize(T - dot(T, N) * N);

    mat3 TBN = mat3(T, B, N);

    // Transform sampled normal from tangent space to world space
    vec3 worldNormal = normalize(TBN * normal);



    outAlbedo   = vec4(albedo, 1.0);
    outNormal = vec4(worldNormal, 1.0);
    outMaterial = vec4(metallic, roughness, ao, 1.0);


}
