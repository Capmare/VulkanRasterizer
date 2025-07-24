#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

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

layout(set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;


// x fata spate
// y sus jos
// z stanga dreapta

const vec3 lightPos = vec3(5.f, -5.f, 0.0);
const vec3 lightColor = vec3(1.0, 0.95, 0.9);
const float lightIntensity = 10;

vec3 sampleNormal() {
    vec3 normalColor = texture(sampler2D(textures[nonuniformEXT(material.Normal)], texSampler), fragTexCoord).xyz;
    normalColor = normalColor * 2.0 - 1.0; // Map [0,1] → [-1,1]
    return normalize(normalColor);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {

    vec3 albedo = texture(sampler2D(textures[nonuniformEXT(material.Diffuse)], texSampler), fragTexCoord).rgb;
    float metallic = texture(sampler2D(textures[nonuniformEXT(material.Metallic)], texSampler), fragTexCoord).b;
    float roughness = texture(sampler2D(textures[nonuniformEXT(material.Roughness)], texSampler), fragTexCoord).g;

    vec3 N = sampleNormal();
    vec3 V = normalize(ubo.cameraPos - fragWorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0,albedo,metallic);

    vec3 Lo = vec3(0.0);

    vec3 L = normalize(lightPos-fragWorldPos);
    vec3 H = normalize(V+L);
    float distance = length(lightPos-fragWorldPos);
    float attenuation = 1.0 / (distance* distance);
    vec3 radiance = lightColor * attenuation * lightIntensity;

    float NDF = DistributionGGX(N,H,roughness);
    float G = GeometrySmith(N,V,L,roughness);
    vec3 F = fresnelSchlick(max(dot(H,V),0.0),F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N,V),0.0) * max(dot(N,L),0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    float NDotL = max(dot(N,L),0.0);
    Lo += (kD * albedo / 3.1415 + specular) * radiance * NDotL;

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color,vec3(1.0/2.2));



    outColor = vec4(color, 1.0);
    // for debugging purposes debugging

    // outColor = vec4(N * 0.5 + 0.5, 1.0); // normals , should look like a rainbow
    // outColor = vec4(vec3(NdotL), 1.0); // N*L, normals might be flipped if its nearly dark
    // outColor = vec4(radiance, 1.0);


}
