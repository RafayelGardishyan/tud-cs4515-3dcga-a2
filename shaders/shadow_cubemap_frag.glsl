#version 410

in vec4 fragPos;

uniform vec3 lightPosition;
uniform float farPlane;

void main()
{
    float lightDistance = length(fragPos.xyz - lightPosition);
    lightDistance = lightDistance / farPlane;
    gl_FragDepth = lightDistance;
}
