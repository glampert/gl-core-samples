
/* -------------------------------------------------------------
 * Normal-mapping GLSL Fragment Shader, used by the DOOM3 models
 * ------------------------------------------------------------- */

// NOTE: We have the same constant in the C++ code, so they must match!!!
const int MaxLights = 2;

// Light types (GLSL lacks enum unfortunately), matching the C++ values:
const int LightType_Point      = 0;
const int LightType_Flashlight = 1;

// Input from vertex shader (varyings):
layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoords;
layout(location = 2) in vec3 v_VertexPosModelSpace;
layout(location = 3) in vec3 v_ViewDirTangentSpace;
layout(location = 4) in vec3 v_LightDirTangentSpace[MaxLights]; // slots 4 & 5
layout(location = 6) in vec4 v_LightProjTexCoords[MaxLights];   // slots 6 & 7

// Texture samples:
uniform sampler2D u_BaseTexture;                   // @ tmu:0
uniform sampler2D u_NormalTexture;                 // @ tmu:1
uniform sampler2D u_SpecularTexture;               // @ tmu:2
uniform sampler2D u_LightCookieTexture[MaxLights]; // @ tmu:3

// Material:
uniform float u_MatShininess;
uniform vec4  u_MatAmbientColor;
uniform vec4  u_MatDiffuseColor;
uniform vec4  u_MatSpecularColor;
uniform vec4  u_MatEmissiveColor;

// Lights:
uniform int   u_NumOfLights;
uniform int   u_LightType[MaxLights];
uniform float u_LightAttenConst[MaxLights];
uniform float u_LightAttenLinear[MaxLights];
uniform float u_LightAttenQuadratic[MaxLights];
uniform vec4  u_LightColor[MaxLights];
uniform vec3  u_LightPosModelSpace[MaxLights];

// Fragment color output:
out vec4 out_FragColor;

// ========================================================
// Scale and bias the normal map texture:
// ========================================================

vec3 sampleNormalMap(in vec2 texCoords)
{
    return normalize(texture(u_NormalTexture, texCoords).rgb * 2.0 - 1.0);
}

// ========================================================
// Compute point light contribution:
// ========================================================

float pointLight(
    in vec3  p,              // Point in space that is to be lit.
    in vec3  lightPos,       // Position of the light source.
    in float attenConst,     // Constant light attenuation factor.
    in float attenLinear,    // Linear light attenuation factor.
    in float attenQuadratic) // Quadratic light attenuation factor.
{
    float d = length(lightPos - p);
    return (1.0 / (attenConst + attenLinear * d + attenQuadratic * (d * d)));
}

// ========================================================
// Shades a fragment using the Blinn-Phong light model:
// ========================================================

vec4 shade(
    in vec3 N,            // Surface normal.
    in vec3 H,            // Half-vector.
    in vec3 L,            // Light direction vector.
    in vec4 specular,     // Surface's specular reflection color, modulated with specular map sample.
    in vec4 diffuse,      // Surface's diffuse  reflection color, modulated with diffuse  map sample.
    in vec4 emissive,     // Surface's emissive contribution. Emission color modulated with an emission map.
    in vec4 ambient,      // Ambient contribution.
    in float shininess,   // Surface shininess (Phong-power).
    in vec4 lightContrib) // Light contribution, computed based on light source type.
{
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float specContrib = (NdotL > 0.0) ? 1.0 : 0.0;

    vec4 K = emissive + (diffuse * ambient);
    K += lightContrib * (diffuse * NdotL + (specular * pow(NdotH, shininess) * specContrib));
    return K;
}

// ========================================================
// main():
// ========================================================

void main()
{
    vec4 shadedColor = vec4(0.0);

    // Switch on the light type and compute the contribution:
    for (int l = 0; l < u_NumOfLights; ++l)
    {
        if (u_LightType[l] == LightType_Point)
        {
            // Normal-mapped point light:
            float lightAmount = pointLight(v_VertexPosModelSpace,
                                           u_LightPosModelSpace[l],
                                           u_LightAttenConst[l],
                                           u_LightAttenLinear[l],
                                           u_LightAttenQuadratic[l]);

            vec4 lightContrib = lightAmount * u_LightColor[l];

            vec3 V = v_ViewDirTangentSpace;
            vec3 L = v_LightDirTangentSpace[l];

            vec3 H = normalize(L + V);
            vec3 N = sampleNormalMap(v_TexCoords);

            vec4 diffuse  = texture(u_BaseTexture,     v_TexCoords) * u_MatDiffuseColor;
            vec4 specular = texture(u_SpecularTexture, v_TexCoords) * u_MatSpecularColor;

            shadedColor += shade(N, H, L, specular, diffuse,
                                 u_MatEmissiveColor, u_MatAmbientColor,
                                 u_MatShininess, lightContrib);
        }
        else if (u_LightType[l] == LightType_Flashlight)
        {
            // u_LightColor.w is reserved for the falloff factor.
            vec4 lightColor = vec4(u_LightColor[l].xyz, 1.0);
            vec4 lightCookieColor = textureProj(u_LightCookieTexture[l], v_LightProjTexCoords[l].xyz) * lightColor;

            // Cheap-O flashlight falloff effect by simply scaling the
            // distance of the point being lit from the light source.
            float dist = length(u_LightPosModelSpace[l] - v_VertexPosModelSpace) * u_LightColor[l].w;

            // Ensure between [0,1]:
            dist = clamp(dist, 0.0, 1.0);

            // Need to use the inverse distance, otherwise the bright end would be the farthest from the light.
            shadedColor += lightCookieColor * (1.0 - dist);
        }
    }

    out_FragColor = shadedColor * v_Color;
    out_FragColor.a = 1.0; // Assumed fully opaque.
}

