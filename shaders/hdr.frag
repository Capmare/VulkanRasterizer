#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout (location = 2) in vec2 inTexCoord;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler texSampler;
layout (set = 0, binding = 1) uniform texture2D HDRColor;

const float PI = 3.14159265359;

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
    float exposure = 2.0;           // tweak as you like
    color *= exposure;

    const float W = 11.2;           // white point
    vec3 mapped = Uncharted2Tonemap(color);
    vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
    return mapped * whiteScale;
}

void main() {
    vec3 hdr = texture(sampler2D(HDRColor, texSampler), inTexCoord).rgb;

    // tonemap + gamma
    vec3 color = ToneMapUncharted2(hdr);
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
