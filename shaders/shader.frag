#version 450
#extension GL_EXT_nonuniform_qualifier: require


//#define CAMERA_PRESET_SUNNY16
#define CAMERA_PRESET_INDOOR

const float PI = 3.14159265359;


layout (location = 2) in vec2 inTexCoord;
layout (location = 0) out vec4 outColor;


layout (set = 1, binding = 0) uniform sampler texSampler;
layout (set = 1, binding = 4) uniform sampler shadowSampler;
layout (constant_id = 0) const uint TEXTURE_COUNT = 1u;
layout (set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];

struct PointLight {
    vec4 Position;
    vec4 Color;
};

struct DirectionalLight {
    vec4 Direction;
    vec4 Color;
};

layout (constant_id = 2) const uint MAX_POINT_LIGHTS = 1u;
layout (constant_id = 3) const uint MAX_DIRECTIONAL_LIGHTS = 1u;

layout (set = 1, binding = 2, std430) readonly buffer PointLightBuffer {
    PointLight pointLights[MAX_POINT_LIGHTS];
} pointLightBuffer;

layout (set = 1, binding = 3, std430) readonly buffer DirLightBuffer {
    DirectionalLight dirLights[MAX_DIRECTIONAL_LIGHTS];
} dirLightBuffer;

layout (std140, binding = 0) uniform UBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;

layout (set = 0, binding = 1) uniform texture2D Diffuse;
layout (set = 0, binding = 2) uniform texture2D Normal;
layout (set = 0, binding = 3) uniform texture2D Material;
layout (set = 0, binding = 4) uniform texture2D Depth;
layout (std140, binding = 5) uniform shadowUBO {
    mat4 view;
    mat4 proj;
} shadowUbo;

layout (set = 0, binding = 6) uniform texture2D Shadow[MAX_DIRECTIONAL_LIGHTS];
layout (set = 0, binding = 7) uniform textureCube EnviromentMap;

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


float sampleShadowPCF(texture2D shadowImg, sampler cmp, vec3 uvz, vec2 texelSize)
{
    float s = 0.0;
    for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y) {
        vec2 off = vec2(x, y) * texelSize;
        s += texture( sampler2DShadow(shadowImg, cmp),
                      vec3(uvz.xy + off, uvz.z) );
    }
    return s / 9.0;
}

void main() {
    float depth = texelFetch(sampler2D(Depth, texSampler), ivec2(gl_FragCoord.xy), 0).r;

    // Reconstruct view-space position
    vec3 worldPos;
    worldPos = reconstructWorldPos(depth, inverse(ubo.proj));
    if (depth >= 1.f)
    {
        const vec3 sampleDirection = normalize(worldPos.xyz);
        outColor = vec4(texture(samplerCube(EnviromentMap,texSampler), sampleDirection).rgb,1.f);
        return;

    }

    vec3 albedo = texture(sampler2D(Diffuse, texSampler), inTexCoord).rgb;
    float metallic = texture(sampler2D(Material, texSampler), inTexCoord).r;
    float roughness = texture(sampler2D(Material, texSampler), inTexCoord).g;
    roughness = clamp(roughness, 0.04, 1.0);

    vec3 N = normalize(texture(sampler2D(Normal, texSampler), inTexCoord).rgb);

    vec3 Lo = vec3(0,0,0);
    vec3 V = normalize(ubo.cameraPos - worldPos);

    for (int i = 0; i < MAX_POINT_LIGHTS; ++i) {
        vec3 lightPos = pointLightBuffer.pointLights[i].Position.xyz;
        vec3 L = normalize(lightPos - worldPos);
        vec3 H = normalize(V + L);

        float distance = length(lightPos - worldPos);
        float attenuation = 1.0 / max((distance * distance), 0.0001f);
        vec3 radiance = pointLightBuffer.pointLights[i].Color.xyz * pointLightBuffer.pointLights[i].Color.w * attenuation;

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

    float shadowDepth;
    for (int i = 0; i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        vec3 L = normalize(-dirLightBuffer.dirLights[i].Direction.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = dirLightBuffer.dirLights[i].Color.xyz * dirLightBuffer.dirLights[i].Color.w;

        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0), 0.001);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);


        vec4 lightSpacePosition = shadowUbo.proj * shadowUbo.view * vec4(worldPos,1.f);
        lightSpacePosition /= lightSpacePosition.w;
        vec3 shadowMapUV = vec3(lightSpacePosition.xy * 0.5f + 0.5f, lightSpacePosition.z);
        //float bias = max(0.0005 * (1.0 - dot(N, L)), 0.00005);
        ivec2 sz = textureSize(sampler2D(Shadow[i], shadowSampler), 0);
        vec2 texelSize = 1.0 / vec2(sz);
        float shadowDepth = sampleShadowPCF(Shadow[i],shadowSampler, vec3(shadowMapUV.xy, shadowMapUV.z), texelSize);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadowDepth;
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