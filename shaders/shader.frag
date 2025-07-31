#version 450
#extension GL_EXT_nonuniform_qualifier: require

//#define CAMERA_PRESET_SUNNY16
#define CAMERA_PRESET_INDOOR

const float PI = 3.14159265359;

layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler texSampler;
layout(constant_id = 0) const uint TEXTURE_COUNT = 1u;
layout(set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];

struct PointLight
{
    vec4 Position; // w is unused
    vec4 Color; // w is intensity
};

layout(constant_id = 2) const uint MAX_LIGHTS = 1;
layout(set = 1, binding = 2, std430) readonly buffer LightBuffer {
    PointLight pointLights[MAX_LIGHTS];
} lightBuffer;


layout(std140, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;

} ubo;

layout(set = 0, binding = 1) uniform texture2D Diffuse;
layout(set = 0, binding = 2) uniform texture2D Normal;
layout(set = 0, binding = 3) uniform texture2D Material;
layout(set = 0, binding = 4) uniform texture2D Depth;



vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 ToneMapUncharted2(vec3 color) {
    float exposure = 2;
    color *= exposure;

    const float W = 11.2;
    vec3 mapped = Uncharted2Tonemap(color);
    vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
    return mapped * whiteScale;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Reconstruct position from depth in view space
vec3 reconstructWorldPos(float depth, mat4 invProj) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(inTexCoord * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = invProj * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = inverse(ubo.view) * viewSpacePosition;

    return worldSpacePosition.xyz;
}

void main() {
    float depth = texelFetch(sampler2D(Depth, texSampler), ivec2(gl_FragCoord.xy), 0).r;

    // Reconstruct view-space position
    vec3 worldPos = reconstructWorldPos(depth, inverse(ubo.proj));

    vec3 albedo = texture(sampler2D(Diffuse, texSampler), inTexCoord).rgb;
    float metallic = texture(sampler2D(Material, texSampler), inTexCoord).r;
    float roughness = texture(sampler2D(Material, texSampler), inTexCoord).g;
    vec3 N = texture(sampler2D(Normal, texSampler), inTexCoord).rgb;

    vec3 Lo = vec3(0,0,0);

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        vec3 V = normalize(ubo.cameraPos - worldPos);
        vec3 lightPos = lightBuffer.pointLights[i].Position.xyz;
        vec3 L = normalize(lightPos - worldPos);
        vec3 H = normalize(V + L);

        float distance = length(lightPos - worldPos);
        float attenuation = 1.0 / max((distance * distance), 0.0001f);
        vec3 radiance = lightBuffer.pointLights[i].Color.xyz * lightBuffer.pointLights[i].Color.w * attenuation;

        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        denominator = max(denominator, 0.001);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // Gamma correction
    color = ToneMapUncharted2(color);
    color = pow(color, vec3(1.0 / 2.2));

    // Debug light vector
    outColor = vec4(color, 1.0);

    //outColor = vec4(normalize(L) * 0.5 + 0.5, 1.0); // visualize L direction

    //outColor = vec4(color, 1.0);
}