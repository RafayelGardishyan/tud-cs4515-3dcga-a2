#version 410

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform float envBrightness;

void main()
{
    vec3 color = texture(skybox, TexCoords).rgb;

    // Apply brightness
    color *= envBrightness;

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));

    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}