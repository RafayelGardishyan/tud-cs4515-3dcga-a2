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
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform bool hasTexCoords;
uniform bool useMaterial;

// Global texture toggles
uniform bool enableColorTextures;
uniform bool enableNormalTextures;
uniform bool enableMetallicTextures;

uniform bool enableGammaCorrection;
uniform bool enableToneMapping;

uniform vec3 cameraPosition;

uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float lightIntensity;
uniform int lightType;
uniform vec3 lightDirection;
uniform float spotlightCosCutoff;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;
in mat3 TBN;

layout(location = 0) out vec4 fragColor;


const float PI = 3.1415926;
const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_SPOT = 1;

uniform bool enableShadows;
uniform bool enableShadowPCF;
uniform sampler2D shadowMap;
uniform samplerCube shadowCubemap;
uniform mat4 lightSpaceMatrix;
uniform float shadowFarPlane;
uniform vec2 shadowMapTexelSize;

float offset_lookup(sampler2D shadowMapTex, vec2 baseCoord, vec2 offset)
{
    vec2 sampleCoord = baseCoord + offset * shadowMapTexelSize;
    return texture(shadowMapTex, sampleCoord).r;
}

float cubemap_offset_lookup(samplerCube cubeMap, vec3 direction, vec3 offset)
{
    vec3 offsetDirection = normalize(direction + offset);
    return texture(cubeMap, offsetDirection).r;
}

float computeSpotShadow(vec3 worldPos, vec3 N, vec3 L)
{
    vec4 fragLightSpace = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = fragLightSpace.xyz / fragLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.002 * (1.0 - dot(N, L)), 0.0005);

    if (!enableShadowPCF)
        return (currentDepth - bias) > closestDepth ? 0.0 : 1.0;

    float occlusion = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float depthSample = offset_lookup(shadowMap, projCoords.xy, vec2(x, y));
            occlusion += (currentDepth - bias) > depthSample ? 1.0 : 0.0;
        }
    }

    float averageOcclusion = occlusion / 9.0;
    return 1.0 - averageOcclusion;
}

float computePointShadow(vec3 worldPos, vec3 N, vec3 L)
{
    vec3 fragToLight = worldPos - lightPosition;
    float currentDepth = length(fragToLight);
    float bias = max(0.05 * (1.0 - dot(N, L)), 0.01);

    float closestDepth = texture(shadowCubemap, fragToLight).r * shadowFarPlane;
    if (!enableShadowPCF)
        return (currentDepth - bias) > closestDepth ? 0.0 : 1.0;

    const float sampleRadius = 0.02;
    const vec3 sampleOffsetDirections[6] = vec3[](
        vec3(sampleRadius, 0.0, 0.0),
        vec3(-sampleRadius, 0.0, 0.0),
        vec3(0.0, sampleRadius, 0.0),
        vec3(0.0, -sampleRadius, 0.0),
        vec3(0.0, 0.0, sampleRadius),
        vec3(0.0, 0.0, -sampleRadius)
    );

    float occlusion = 0.0;
    for (int i = 0; i < 6; ++i) {
        float depthSample = cubemap_offset_lookup(shadowCubemap, fragToLight, sampleOffsetDirections[i]) * shadowFarPlane;
        occlusion += (currentDepth - bias) > depthSample ? 1.0 : 0.0;
    }

    float averageOcclusion = occlusion / 6.0;
    return 1.0 - averageOcclusion;
}

float computeShadow(vec3 worldPos, vec3 N, vec3 L)
{
    if (!enableShadows)
        return 1.0;

    if (lightType == LIGHT_TYPE_SPOT)
        return computeSpotShadow(worldPos, N, L);

    return computePointShadow(worldPos, N, L);
}

float computeSpotAttenuation(vec3 worldPos)
{
    if (lightType != LIGHT_TYPE_SPOT)
        return 1.0;

    vec3 lightToFragment = normalize(worldPos - lightPosition);
    float cosTheta = dot(normalize(lightDirection), lightToFragment);
    float softness = 0.02;
    return smoothstep(spotlightCosCutoff, spotlightCosCutoff + softness, cosTheta);
}

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
    if (useMaterial && hasTexCoords && textureFlags.y == 1 && enableNormalTextures)
    {
        vec3 normalSample = texture(normalMap, fragTexCoord).rgb;
        normalSample = normalSample * 2.0 - 1.0; // Transform from [0,1] to [-1,1]
        normalSample = normalize(normalSample);
        N = normalize(TBN * normalSample);
    }

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
    if (useMaterial && hasTexCoords && textureFlags.x == 1 && enableColorTextures)
    {
        albedo = texture(colorMap, fragTexCoord).rgb;
    }

    float m_metallic = metallic;
    float m_roughness = roughness;

    if (useMaterial && hasTexCoords && textureFlags.z == 1 && enableMetallicTextures)
    {
        // Sample metallic and roughness from separate textures (R channel of each)
        m_metallic = texture(metallicMap, fragTexCoord).r;
        m_roughness = texture(roughnessMap, fragTexCoord).r;
    }

    m_roughness = clamp(m_roughness, 0.05, 1.0); // Avoid 0 roughness

    // Fresnel reflectance at normal incidence
    float ior = 1.5;
    float f0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    vec3 F0 = mix(vec3(f0), albedo, m_metallic);

    // Cook-Torrance specular term
    float a = m_roughness * m_roughness;

    // D: GGX distribution
    float D = D_GGX(N, H, a);

    // F: Fresnel (Schlick)
    vec3 F = F_Schlick(VdotH, F0);

    // G: Geometry term (Smith's method)
    float G = G_GGX_Partial(NdotV, m_roughness) * G_GGX_Partial(NdotL, m_roughness);

    // Cook-Torrance specular BRDF
    vec3 numerator = D * F * G;
    float denominator = max(4.0 * NdotV, 0.001);
    vec3 specular = numerator / denominator;

    // Lambertian diffuse BRDF
    vec3 kD = (vec3(1.0) - F) * (1.0 - m_metallic); // Energy conservation
    vec3 diffuse = kD * albedo / PI;

    // Combine with light contribution (rendering equation)
    vec3 radiance = lightColor * lightIntensity;
    vec3 color = (diffuse + specular) * radiance * NdotL;
    float shadowMultiplier = computeShadow(fragPosition, N, L);
    float spotlightMultiplier = computeSpotAttenuation(fragPosition);
    color *= shadowMultiplier * spotlightMultiplier;

    // Add emissive
    color += emissive;

    // Tone mapping
    if (enableToneMapping)
        color = color / (color + vec3(1.0));

    if (enableGammaCorrection)
        color = pow(color, vec3(1.0/2.2));

    fragColor = vec4(color, 1.0);
}
