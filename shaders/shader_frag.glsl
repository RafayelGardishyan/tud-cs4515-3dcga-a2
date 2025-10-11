#version 410

layout(std140) uniform Material // Must match RS_GPUMaterial in src/model.h
{
    vec3 baseColor;         // offset 0
    float metallic;         // offset 12
    float roughness;        // offset 16
    float transmission;     // offset 20
    vec3 emissive;          // offset 32 (padding added by alignment)
    float _padding;         // offset 44
    ivec4 textureFlags;     // offset 48: [hasBaseColor, hasNormal, hasMetallicRoughness, hasEmissive]
};

uniform sampler2D colorMap;
uniform bool hasTexCoords;
uniform bool useMaterial;

uniform vec3 cameraPosition;

uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float lightIntensity;

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(cameraPosition - fragPosition); // View direction in world space
    vec3 lightDir = normalize(lightPosition - fragPosition); // Light direction in world space
    vec3 halfDir = normalize(lightDir + viewDir); // Halfway vector

    // Base color
    vec3 albedo = baseColor;
    if (useMaterial && hasTexCoords && textureFlags.x == 1)
    {
        albedo *= texture(colorMap, fragTexCoord).rgb;
    } // else use baseColor from material

    // Ambient term (simple approximation)
    vec3 ambient = 0.03 * albedo;

    // Diffuse term (lambert; d = N * L)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * albedo * lightColor * lightIntensity;

    // Specular term (Cook-Torrance) - https://graphicscompendium.com/gamedev/15-pbr && http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx
    float rough = clamp(roughness, 0.05, 1.0);
    float NdotH = max(dot(normal, halfDir), 0.0);
    float NdotV = max(dot(normal, viewDir), 0.0);
    float NdotL = max(dot(normal, lightDir), 0.0);
    float VdotH = max(dot(viewDir, halfDir), 0.0);

    // Fresnel (Schlick's approximation)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Distribution function (GGX)
    float alpha = rough * rough;
    float alpha2 = alpha * alpha;
    float denom = (NdotH * NdotH) * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (3.14159265 * denom * denom);

    // Geometry function (Schlick-GGX)
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;
    float G_V = NdotV / (NdotV * (1.0 - k) + k);
    float G_L = NdotL / (NdotL * (1.0 - k) + k);
    float G = G_V * G_L;

    vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.001);

    vec3 color = ambient + diffuse + specular;
    color = color * (1.0 - transmission) + albedo * transmission; // Simple transmission model
    color += emissive; // Add emissive component
    color = color / (color + vec3(1.0)); // Tone mapping
    color = pow(color, vec3(1.0/2.2)); // Gamma correction

    fragColor = vec4(color, 1.0);
}
