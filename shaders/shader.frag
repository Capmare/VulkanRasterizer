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

struct PointLight { vec4 Position; vec4 Color; };
struct DirectionalLight { vec4 Direction; vec4 Color; };

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
layout (set = 0, binding = 8) uniform textureCube irradianceMap;

const bool USE_DIRECT_RADIANCE = true;
const bool USE_IRRADIANCE = true;
const bool OUTPUT_SHADOWS_ONLY = false;

vec3 Uncharted2Tonemap(vec3 x){
    float A=0.15,B=0.50,C=0.10,D=0.20,E=0.02,F=0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
vec3 ToneMapUncharted2(vec3 color){
    float exposure = 2.0; color *= exposure;
    const float W = 11.2;
    vec3 mapped = Uncharted2Tonemap(color);
    vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
    return mapped * whiteScale;
}

float DistributionGGX(vec3 N, vec3 H, float roughness){
    float a=roughness*roughness, a2=a*a;
    float NdotH=max(dot(N,H),0.0), NdotH2=NdotH*NdotH;
    float num=a2, denom=(NdotH2*(a2-1.0)+1.0); denom=PI*denom*denom;
    return num/denom;
}
float GeomtrySchlickGGX_Direct(float NdotV, float roughness){
    float r=(roughness+1.0), k=(r*r)/8.0;
    return NdotV / (NdotV*(1.0-k)+k);
}
float GeomtrySchlickGGX_Indirect(float NdotV, float roughness){
    float k=(roughness*roughness)/2.0;
    return NdotV / (NdotV*(1.0-k)+k);
}
float GeomtrySmith(vec3 N, vec3 V, vec3 L, float roughness, bool indirectLighting){
    float NdotV=max(dot(N,V),0.0), NdotL=max(dot(N,L),0.0);
    float ggx2 = indirectLighting ? GeomtrySchlickGGX_Indirect(NdotV,roughness)
    : GeomtrySchlickGGX_Direct(NdotV,roughness);
    float ggx1 = indirectLighting ? GeomtrySchlickGGX_Indirect(NdotL,roughness)
    : GeomtrySchlickGGX_Direct(NdotL,roughness);
    return ggx1*ggx2;
}
vec3 fresnelSchlick(float cosTheta, vec3 F0){
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 reconstructWorldPos(float depth, mat4 invProj){

    vec4 ndc = vec4(inTexCoord * 2.0 - 1.0, depth, 1.0);
    vec4 view = invProj * ndc;
    view /= view.w;
    vec4 world = inverse(ubo.view) * view;
    return world.xyz;


}

float sampleShadowPCF_Tent(texture2D img, sampler cmp, vec3 uvz, vec2 texelSize, int radius){
    float sum=0.0, wsum=0.0;
    for (int y=-radius; y<=radius; ++y)
    for (int x=-radius; x<=radius; ++x){
        float wx=float(radius+1-abs(x)), wy=float(radius+1-abs(y));
        float w=wx*wy;
        vec2 off=vec2(x,y)*texelSize;
        float d=texture(sampler2D(img,cmp), uvz.xy+off).r;
        sum += w * (uvz.z <= d ? 1.0 : 0.0);
        wsum += w;
    }
    return sum/wsum;
}

void main(){
    float depth = texelFetch(sampler2D(Depth, texSampler), ivec2(gl_FragCoord.xy), 0).r;
    vec3 worldPos = reconstructWorldPos(depth, inverse(ubo.proj));

    if (depth >= 1.0){
        if (OUTPUT_SHADOWS_ONLY){
            outColor = vec4(0.0,0.0,0.0,1.0);
            return;
        }
        const vec3 sampleDirection = normalize(worldPos.xyz);
        outColor = vec4(texture(samplerCube(EnviromentMap,texSampler), sampleDirection).rgb, 1.0);
        return;
    }

    vec3 albedo    = texture(sampler2D(Diffuse,  texSampler), inTexCoord).rgb;
    float metallic = texture(sampler2D(Material, texSampler), inTexCoord).r;
    float roughness= texture(sampler2D(Material, texSampler), inTexCoord).g;
    metallic  = clamp(metallic,  0.0, 1.0);
    roughness = clamp(roughness, 0.04, 1.0);

    vec3 N = normalize(texture(sampler2D(Normal, texSampler), inTexCoord).xyz);
    vec3 V = normalize(ubo.cameraPos - worldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    vec3 kD_sum = vec3(0.0);
    float lightCount = 0.0;

    float minVis = 1.0;

    for (int i=0; i<int(MAX_POINT_LIGHTS); ++i){
        vec3 lightPos = pointLightBuffer.pointLights[i].Position.xyz;
        vec3 L = normalize(lightPos - worldPos);
        vec3 H = normalize(V + L);

        float distance = length(lightPos - worldPos);
        float attenuation = 1.0 / max(distance*distance, 0.0001);
        vec3 radiance = pointLightBuffer.pointLights[i].Color.xyz
        * pointLightBuffer.pointLights[i].Color.w * attenuation;

        vec3 F = fresnelSchlick(max(dot(H,V),0.0), F0);
        float NDF = DistributionGGX(N,H,roughness);
        float G   = GeomtrySmith(N,V,L,roughness,false);
        vec3 numerator = NDF*G*F;
        float denominator = max(4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0), 0.001);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0)-kS) * (1.0 - metallic);

        float NdotL = max(dot(N,L),0.0);
        vec3 energy = USE_DIRECT_RADIANCE ? radiance : vec3(1.0);

        Lo += (kD * albedo / PI + specular) * energy * NdotL;

        kD_sum += kD;
        lightCount += 1.0;
    }

    for (int i=0; i<int(MAX_DIRECTIONAL_LIGHTS); ++i){
        vec3 L = normalize(-dirLightBuffer.dirLights[i].Direction.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = dirLightBuffer.dirLights[i].Color.xyz
        * dirLightBuffer.dirLights[i].Color.w;

        vec3 F = fresnelSchlick(max(dot(H,V),0.0), F0);
        float NDF = DistributionGGX(N,H,roughness);
        float G   = GeomtrySmith(N,V,L,roughness,true);
        vec3 numerator = NDF*G*F;
        float denominator = max(4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0), 0.001);
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0)-kS) * (1.0 - metallic);

        float NdotL = max(dot(N,L),0.0);

        // Shadowing
        vec4 lightSpacePosition = shadowUbo.proj * shadowUbo.view * vec4(worldPos,1.0);
        lightSpacePosition /= lightSpacePosition.w;
        vec3 shadowMapUV = vec3(lightSpacePosition.xy * 0.5 + 0.5, lightSpacePosition.z);

        ivec2 sz = textureSize(sampler2D(Shadow[i], shadowSampler), 0);
        vec2 texelSize = 1.0 / vec2(sz);

        float vis = sampleShadowPCF_Tent(Shadow[i], shadowSampler,
                                         vec3(shadowMapUV.xy, shadowMapUV.z),
                                         texelSize, 20);

        minVis = min(minVis, vis);

        vec3 energy = USE_DIRECT_RADIANCE ? radiance : vec3(1.0);
        Lo += (kD * albedo / PI + specular) * energy * (NdotL * vis);

        kD_sum += kD;
        lightCount += 1.0;
    }

    if (OUTPUT_SHADOWS_ONLY){
        float shadowMask = 1.0 - minVis;
        outColor = vec4(vec3(shadowMask), 1.0);
        return;
    }

    vec3 ambient = vec3(0.0);
    if (USE_IRRADIANCE){
        vec3 irradiance = normalize(texture(samplerCube(irradianceMap, texSampler), N).rgb);
        vec3 diffuseIBL = irradiance * albedo;
        vec3 kD_avg = (lightCount > 0.0) ? (kD_sum / lightCount) : vec3(1.0 - metallic);
        const float exposureCompensation = 1.0;
        ambient = kD_avg * diffuseIBL * exposureCompensation;
    }

    vec3 color = ambient + Lo;
    color = ToneMapUncharted2(color);
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);

}
