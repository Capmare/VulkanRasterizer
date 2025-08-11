#version 450

layout(location = 0) in  vec3 fragLocalPosition;
layout(location = 0) out vec4 outColor;

// Separate sampler + image (matches set=0, bindings 0 and 1)
layout(set = 0, binding = 0) uniform sampler   hdriSampler;
layout(set = 0, binding = 1) uniform texture2D hdriImage;

// Constants for spherical mapping
const vec2 gInvAtan = vec2(0.1591, 0.3183); // 1/(2π), 1/π

vec2 SampleSphericalMap(vec3 v)
{
    // v should be normalized
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= gInvAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    // Use cube local position as direction; adjust axes to your convention
    vec3 dir = normalize(fragLocalPosition);

    // If your IBL data needs the swap/flip noted in the screenshot:
    dir = vec3(dir.z, dir.y, -dir.x);

    vec2 uv = SampleSphericalMap(dir);
    vec3 hdr = texture(sampler2D(hdriImage, hdriSampler), uv).rgb;

    outColor = vec4(hdr, 1.0);
}
