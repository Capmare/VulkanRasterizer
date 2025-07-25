#version 450
#extension GL_EXT_nonuniform_qualifier: require


//#define CAMERA_PRESET_SUNNY16
#define CAMERA_PRESET_INDOOR


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler texSampler;
layout(constant_id = 0) const uint TEXTURE_COUNT = 1u;


layout(set = 1, binding = 1) uniform texture2D textures[TEXTURE_COUNT];

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} ubo;

layout(set = 0, binding = 1) uniform texture2D Diffuse;
layout(set = 0, binding = 2) uniform texture2D Normal;
layout(set = 0, binding = 3) uniform texture2D Material;


// Point light
const vec3 pointLightPos = vec3(5.f, -5.f, 0.0);
const vec3 pointLightColor = vec3(1.0, 0.3, 0.3);
const float pointLightIntensity = 10.0;

// Directional light
const vec3 dirLightDirection = normalize(vec3(-1.0, -0.2, -0.1));
const vec3 dirLightColor     = vec3(1.0, 0.96, 0.9);
const float dirLightIntensity = 2.5;

vec3 sampleNormal() {
    vec3 normalColor = texture(sampler2D(Normal, texSampler), fragTexCoord).xyz;
    normalColor.g = 1.0 - normalColor.g; // Flip Y channel
    normalColor = normalColor * 2.0 - 1.0;
    return normalize(normalColor);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.1415 * denom * denom;

    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

#ifdef CAMERA_PRESET_SUNNY16
    const float aperture     = 5.0f;
    const float ISO          = 100.0f;
    const float shutterSpeed = 1.0f / 200.0f;
#endif

#ifdef CAMERA_PRESET_INDOOR
    const float aperture     = 1.4f;
    const float ISO          = 1600.0f;
    const float shutterSpeed = 1.0f / 60.0f;
#endif

vec3 Uncharted2ToneMappingCurve(in vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 Uncharted2ToneMapping(in vec3 color) {
    const float whitePoint = 11.2;
    vec3 mapped = Uncharted2ToneMappingCurve(color);
    float exposureNormalization = 1.0 / Uncharted2ToneMappingCurve(vec3(whitePoint)).r;

    return clamp(mapped * exposureNormalization, 0.0, 1.0);
}


void main() {
    vec3 albedo = texture(sampler2D(Diffuse, texSampler), fragTexCoord).rgb;
    float metallic = texture(sampler2D(Material, texSampler), fragTexCoord).r;
    float roughness = texture(sampler2D(Material, texSampler), fragTexCoord).g;

    vec3 N = sampleNormal();

    vec3 V = normalize(ubo.cameraPos - fragWorldPos);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Point Light
    vec3 Lp = normalize(pointLightPos - fragWorldPos);
    vec3 Hp = normalize(V + Lp);
    float distance = length(pointLightPos - fragWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance_p = pointLightColor * attenuation * pointLightIntensity;

    float NDF_p = DistributionGGX(N, Hp, roughness);
    float G_p = GeometrySmith(N, V, Lp, roughness);
    vec3 F_p = fresnelSchlick(max(dot(Hp, V), 0.0), F0);


    vec3 kS_p = F_p;
    vec3 kD_p = vec3(1.0) - kS_p;
    kD_p *= 1.0 - metallic;


    vec3 numerator_p = NDF_p * G_p * F_p;
    float denominator_p = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Lp), 0.0) + 0.0001;
    vec3 specular_p = numerator_p / denominator_p;

    float NDotLp = max(dot(N, Lp), 0.0);
    Lo += (kD_p * albedo / 3.1415 + specular_p) * radiance_p * NDotLp;

    // Directional Light
    vec3 Ld = normalize(-dirLightDirection);
    vec3 Hd = normalize(V + Ld);
    vec3 radiance_d = dirLightColor * dirLightIntensity;

    float NDF_d = DistributionGGX(N, Hd, roughness);
    float G_d = GeometrySmith(N, V, Ld, roughness);
    vec3 F_d = fresnelSchlick(max(dot(Hd, V), 0.0), F0);

    vec3 kS_d = F_d;
    vec3 kD_d = vec3(1.0) - kS_d;
    kD_d *= 1.0 - metallic;

    vec3 numerator_d = NDF_d * G_d * F_d;
    float denominator_d = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Ld), 0.0) + 0.0001;
    vec3 specular_d = numerator_d / denominator_d;

    float NDotLd = max(dot(N, Ld), 0.0);
    Lo += (kD_d * albedo / 3.1415 + specular_d) * radiance_d * NDotLd;

    vec3 ambient = vec3(0.03) * albedo;

    // Combine all lighting
    vec3 color = ambient + Lo;

    // Physical Camera Exposure
    float Ev100 = log2((aperture * aperture) / shutterSpeed * 100.0 / ISO);
    float exposure = 1.0 / (1.2 * pow(2.0, Ev100));
    color *= exposure;

    color = Uncharted2ToneMapping(color);
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(N, 1.0);

    // Debug outputs
    // outColor = vec4(N * 0.5 + 0.5, 1.0);        // Normal visualization
    // outColor = vec4(vec3(NDotLp), 1.0);         // Dot(N, Lp) visualization
    // outColor = vec4(radiance_d, 1.0);           // Directional light intensity
}
