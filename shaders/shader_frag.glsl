#version 410

layout(std140) uniform Material// Must match RS_GPUMaterial in src/model.h
{
    vec3 baseColor;// offset 0
    float metallic;// offset 12
    float roughness;// offset 16
    float transmission;// offset 20
    vec3 emissive;// offset 32 (padding added by alignment)
    float _padding;// offset 44
    ivec4 textureFlags;// offset 48: [hasBaseColor, hasNormal, hasMetallicRoughness, hasEmissive]
};

uniform sampler2D colorMap;
uniform sampler2D normalMap;
uniform bool hasTexCoords;
uniform bool useMaterial;

uniform vec3 cameraPosition;

uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float lightIntensity;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in mat3 TBN;

layout(location = 0) out vec4 fragColor;


const float PI = 3.1415926;

// Cook-Torrance: http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx
float chiGGX(float v)
{
    if (v > 0)
    return 1.0;
    else
    return 0.0;
}

float saturate(float v)
{
    return clamp(v, 0.0, 1.0);
}

vec3 saturate3(vec3 v)
{
    return clamp(v, vec3(0.0), vec3(1.0));
}

float D_GGX(vec3 N, vec3 H, float a)
{
    // Distribution function (GGX)
    float a2 = a * a;
    float NoH = dot(N, H);
    float NoH2 = NoH * NoH;
    float den = NoH2 * a2 + (1 - NoH2);

    return (chiGGX(NoH) * a2) / (PI * den * den);
}

float G_GGX_Partial(float NdotV, float roughness)
{
    // Smith-GGX geometry function (single direction)
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotV2 = NdotV * NdotV;

    float nom = 2.0 * NdotV;
    float denom = NdotV + sqrt(a2 + (1.0 - a2) * NdotV2);

    return nom / denom;
}

vec3 F_Schlick(float cosT, vec3 F0)
{
    return F0 + (1 - F0) * pow(1 - cosT, 5);
}

void main()
{
    // Cook-Torrance BRDF for a point light source
    // f_r = kd * f_lambert + ks * f_cook_torrance

    vec3 N = normalize(fragNormal);
    // Normal mapping
    if (useMaterial && hasTexCoords && textureFlags.y == 1)
    {
        vec3 normalSample = texture(normalMap, fragTexCoord).rgb;
        normalSample = normalSample * 2.0 - 1.0; // Transform from [0,1] to [-1,1]
        normalSample.y = -normalSample.y; // Invert Y for OpenGL
        normalSample = normalize(normalSample);
        N = normalize(TBN * normalSample);
    }

    fragColor = vec4(N * 0.5 + 0.5, 1.0); // Visualize normals
    return;

    vec3 V = normalize(cameraPosition - fragPosition);
    vec3 L = normalize(lightPosition - fragPosition);
    vec3 H = normalize(V + L);

    // Calculate angles
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // Base color (albedo)
    vec3 albedo = baseColor;
    if (useMaterial && hasTexCoords && textureFlags.x == 1)
    {
        albedo = texture(colorMap, fragTexCoord).rgb;
    }

    // Fresnel reflectance at normal incidence
    float ior = 1.5;
    float f0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    vec3 F0 = mix(vec3(f0), albedo, metallic);

    // Cook-Torrance specular term
    float a = roughness * roughness;

    // D: GGX distribution
    float D = D_GGX(N, H, a);

    // F: Fresnel (Schlick)
    vec3 F = F_Schlick(VdotH, F0);

    // G: Geometry term (Smith's method)
    float G = G_GGX_Partial(NdotV, roughness) * G_GGX_Partial(NdotL, roughness);

    // Cook-Torrance specular BRDF
    vec3 numerator = D * F * G;
    float denominator = max(4.0 * NdotV, 0.001);
    vec3 specular = numerator / denominator;

    // Lambertian diffuse BRDF
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic); // Energy conservation
    vec3 diffuse = kD * albedo / PI;

    // Combine with light contribution (rendering equation)
    vec3 radiance = lightColor * lightIntensity;
    vec3 color = (diffuse + specular) * radiance * NdotL;

    // Add emissive
    color += emissive;

    // Tone mapping
    color = color / (color + vec3(1.0));

    color = pow(color, vec3(1.0/2.2));

    fragColor = vec4(color, 1.0);
}
