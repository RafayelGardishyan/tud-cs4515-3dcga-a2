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

uniform bool useMaterial;
uniform bool hasTexCoords;
uniform sampler2D colorMap;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

const float PI = 3.1415926;

const float environmentIntensity = .3;

vec3 textureSample(samplerCube map, vec3 dir, float roughness, float spreadFactor)
{
    vec3 accumulatedColor = vec3(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < 16; ++i)
    {
        // Sample texture around the direction, with a spread based on roughness
        float radius = max(exp(roughness * spreadFactor), 1.0);
        float theta = acos(1.0 - 2.0 * (float(i) + 0.5) / 16.0);
        float phi = PI * (1.0 + sqrt(5.0)) * float(i);
        vec3 sampleDir = normalize(dir + radius * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)));

        float weight = max(dot(dir, sampleDir), 0.0);
        accumulatedColor += texture(environmentMap, sampleDir).rgb * weight;
        totalWeight += weight;
    }

    return accumulatedColor / totalWeight;
}

void main()
{
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(cameraPosition - fragPosition);
    vec3 R = reflect(-V, N);

    vec3 diffuseEnv = textureSample(environmentMap, N, roughness, 5).rgb;

    vec3 albedo = baseColor;
    if (useMaterial && hasTexCoords && textureFlags.x == 1)
    {
        albedo = texture(colorMap, fragTexCoord).rgb;
    }


    float NdotV = max(dot(N, V), 0.0);
    float ior = 1.5;
    float f0_dielectric = pow((1.0 - ior) / (1.0 + ior), 2.0); // ~0.04 for ior=1.5
    vec3 F0 = mix(vec3(f0_dielectric), albedo, metallic); // mix baseColor for metals
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0); // Schlick

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    vec3 specEnv = textureSample(environmentMap, R, roughness, roughness * 5).rgb;

    vec3 envDiffuse  = diffuseEnv * albedo * kD * envBrightness * (1.0 / PI);
    vec3 envSpecular = specEnv * F * envBrightness;

    vec3 color = envDiffuse + envSpecular;

    // tone mapping (Reinhard) + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    color *= environmentIntensity;

    fragColor = vec4(color, 1.0);
}
