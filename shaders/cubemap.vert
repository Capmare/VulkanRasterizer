#version 450


layout (location = 0) out vec3 outLocalPos;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outTexCoord;
layout (location = 3) out vec3 outNormal;
layout (location = 4) out vec3 outTangent;
layout (location = 5) out vec3 outBitangent;


layout (push_constant) uniform Push {
    mat4 view;
    mat4 projection;
} pc;


const vec3 g_VertexPositions[36] = vec3[](
// +X
vec3(1, -1, -1), vec3(1, -1, 1), vec3(1, 1, 1),
vec3(1, -1, -1), vec3(1, 1, 1), vec3(1, 1, -1),

// -X
vec3(-1, -1, 1), vec3(-1, -1, -1), vec3(-1, 1, -1),
vec3(-1, -1, 1), vec3(-1, 1, -1), vec3(-1, 1, 1),

// +Y
vec3(-1, 1, -1), vec3(1, 1, -1), vec3(1, 1, 1),
vec3(-1, 1, -1), vec3(1, 1, 1), vec3(-1, 1, 1),

// -Y
vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, -1, -1),
vec3(-1, -1, 1), vec3(1, -1, -1), vec3(-1, -1, -1),

// +Z
vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1),
vec3(1, -1, 1), vec3(-1, 1, 1), vec3(1, 1, 1),

// -Z
vec3(-1, -1, -1), vec3(1, -1, -1), vec3(1, 1, -1),
vec3(-1, -1, -1), vec3(1, 1, -1), vec3(-1, 1, -1)
);


void main()
{

    vec3 position = g_VertexPositions[gl_VertexIndex];
    outLocalPos = position;
    gl_Position = pc.projection * pc.view * vec4(position, 1.0);


}