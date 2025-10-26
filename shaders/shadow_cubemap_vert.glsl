#version 410

layout(location = 0) in vec3 position;

uniform mat4 modelMatrix;

out vec3 worldPos;

void main()
{
    worldPos = (modelMatrix * vec4(position, 1.0)).xyz;
    gl_Position = vec4(worldPos, 1.0);
}
