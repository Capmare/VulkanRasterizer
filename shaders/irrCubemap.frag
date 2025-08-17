#version 450

layout(location = 0) in  vec3 fragLocalPosition;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler     hdriSampler;
layout(set = 0, binding = 1) uniform textureCube hdriImage;

const float PI = 3.14159265;

void TangentToWorld(in vec3 normal, out vec3 tangent, out vec3 bitangent)
{
    vec3 up = (abs(normal.y) < 0.999)
    ? vec3(0.0, 1.0, 0.0)
    : vec3(1.0, 0.0, 0.0);

    tangent   = normalize(cross(up, normal));

    bitangent = normalize(cross(normal, tangent));
}

void main()
{
    vec3 normal = normalize(fragLocalPosition);
    vec3 tangent, bitangent;
    TangentToWorld(normal, tangent, bitangent);

    vec3  irradiance  = vec3(0.0);
    int   nrSamples   = 0;
    const float sampleDelta = 0.025;

    for (float phi = 0.0;   phi   < 2.0 * PI;  phi   += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);

            // tangent-space sample direction
            vec3 tangentSample = vec3(
            sinTheta * cos(phi),
            sinTheta * sin(phi),
            cosTheta
            );

            // to world space
            vec3 sampleVec =
            tangentSample.x * tangent +
            tangentSample.y * bitangent +
            tangentSample.z * normal;

            irradiance += texture(samplerCube(hdriImage, hdriSampler), sampleVec).rgb  * cosTheta * sinTheta;
            ++nrSamples;
        }
    }

    irradiance = PI * irradiance * (1.f / float(nrSamples));
    outColor = vec4(irradiance, 1.0);


}
