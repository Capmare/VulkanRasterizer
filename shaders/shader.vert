
#version 450

layout(location = 1) out vec2 fragTexCoord;



void main()
{
        fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
        gl_Position = vec4(fragTexCoord * 2.f - 1.f, 0.0f, 1.0f);
}