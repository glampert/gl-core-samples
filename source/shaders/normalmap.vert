
/* -------------------------------------------------------------
 * Normal-mapping GLSL Vertex Shader, used by the DOOM3 models
 * ------------------------------------------------------------- */

// NOTE: We have the same constant in the C++ code, so they must match!!!
const int MaxLights = 2;

// Light types (GLSL lacks enum unfortunately), matching the C++ values:
const int LightType_Point      = 0;
const int LightType_Flashlight = 1;

// Vertex inputs/attributes:
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_TexCoords;
layout(location = 4) in vec3 in_Tangent;
layout(location = 5) in vec3 in_BiTangent;

// Varyings:
layout(location = 0) out vec4 v_Color;                           // Forwarded vertex color.
layout(location = 1) out vec2 v_TexCoords;                       // Forwarded vertex texture coordinates.
layout(location = 2) out vec3 v_VertexPosModelSpace;             // Forwarded vertex position in model coordinates.
layout(location = 3) out vec3 v_ViewDirTangentSpace;             // Tangent-space view direction.
layout(location = 4) out vec3 v_LightDirTangentSpace[MaxLights]; // Tangent-space light direction (slots 4 & 5).
layout(location = 6) out vec4 v_LightProjTexCoords[MaxLights];   // Takes slots 6 & 7.

// Uniform variables:
uniform mat4 u_MvpMatrix;
uniform vec3 u_EyePosModelSpace;

// Light vars:
uniform int  u_NumOfLights;
uniform int  u_LightType[MaxLights];
uniform vec3 u_LightPosModelSpace[MaxLights];
uniform mat4 u_LightProjectionMatrix[MaxLights];

// ========================================================
// main():
// ========================================================

void main()
{
    // Pass on unchanged:
    v_Color = in_Color;
    v_TexCoords = in_TexCoords;
    v_VertexPosModelSpace = in_Position;

    // Transform vertex position to clip-space for GL:
    gl_Position = u_MvpMatrix * vec4(in_Position, 1.0);

    // Transform view direction into tangent space:
    vec3 viewDir = u_EyePosModelSpace - in_Position;
    v_ViewDirTangentSpace = vec3(dot(in_Tangent,   viewDir),
                                 dot(in_BiTangent, viewDir),
                                 dot(in_Normal,    viewDir));

    // Set up the light data for each light source:
    for (int l = 0; l < u_NumOfLights; ++l)
    {
        if (u_LightType[l] == LightType_Point)
        {
            // Transform light direction into tangent space:
            vec3 lightDir = u_LightPosModelSpace[l] - in_Position;
            v_LightDirTangentSpace[l] = vec3(dot(in_Tangent, lightDir),
                                             dot(in_BiTangent, lightDir),
                                             dot(in_Normal, lightDir));
        }
        else if (u_LightType[l] == LightType_Flashlight)
        {
            // Transform vertex position into projective texture space.
            // This matrix combines the light view, projection and bias matrices.
            v_LightProjTexCoords[l] = u_LightProjectionMatrix[l] * vec4(in_Position, 1.0);
        }
    }
}

