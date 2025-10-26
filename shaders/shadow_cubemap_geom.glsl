#version 410

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform mat4 shadowMatrices[6];

in vec3 worldPos[];

out vec4 fragPos;

void main()
{
    for (int face = 0; face < 6; ++face) {
        gl_Layer = face;
        for (int i = 0; i < 3; ++i) {
            fragPos = vec4(worldPos[i], 1.0);
            gl_Position = shadowMatrices[face] * fragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
