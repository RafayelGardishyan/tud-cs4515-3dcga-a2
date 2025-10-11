#version 410

layout(std140) uniform Material
{
    vec3 baseColor;
    float metallic;
    float roughness;
    float transmission;
    vec3 emissive;
    float _padding;
    ivec4 textureFlags;
};

uniform vec3 cameraPosition;
uniform samplerCube environmentMap;
uniform float envBrightness;

in vec3 fragPosition;
in vec3 fragNormal;

layout(location = 0) out vec4 fragColor;

const float PI = 3.1415926;

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(cameraPosition - fragPosition);
    vec3 R = reflect(-V, N);

    // Sample cubemap
    vec3 specColor = texture(environmentMap, R).rgb;
    vec3 diffuseEnv = texture(environmentMap, N).rgb;

    // Fresnel for metallic surfaces
    float NdotV = max(dot(N, V), 0.0);
    float ior = 1.5;
    float f0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    vec3 F0 = mix(vec3(f0), baseColor, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0); // Schlick approximation

    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 envDiffuse = diffuseEnv * baseColor * kD * envBrightness;

    vec3 envSpecular = specColor * F * envBrightness;

    vec3 color = envDiffuse + envSpecular;

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    fragColor = vec4(color, 1.0);
}
