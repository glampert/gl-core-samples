
/* -------------------------------------------------------------
 * GLSL Vertex Shader for the projected spotlight texture demo
 * ------------------------------------------------------------- */

// This must match two other places: the FS and C++ code !!!
// GLSL 4+ allows this to be a uniform variable, but a constant should still be more efficient.
const int numOfLights = 2;

// Vertex inputs/attributes:
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_TexCoords;

// Varyings:
layout(location = 0) out vec3 v_Normal;
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec2 v_TexCoords;
layout(location = 3) out vec4 v_ProjTexCoords[numOfLights]; // takes slots 3 & 4
layout(location = 5) out vec3 v_LightDir[numOfLights];      // takes 5 & 6

// Uniform variables:
uniform mat4 u_MvpMatrix;
uniform vec3 u_LightPositionModelSpace[numOfLights];
uniform mat4 u_LightProjectionMatrix[numOfLights];

void main()
{
    v_Normal    = in_Normal;
    v_Color     = in_Color;
    v_TexCoords = in_TexCoords;
    gl_Position = u_MvpMatrix * vec4(in_Position, 1.0);

    // Texture projection for the spotlights:
    for (int i = 0; i < numOfLights; ++i)
    {
        // Transform vertex position into projective texture space.
        // This matrix combines the light view, projection and bias matrices.
        v_ProjTexCoords[i] = u_LightProjectionMatrix[i] * vec4(in_Position, 1.0);

        // Light direction vector to simulate intensity based on distance/angle:
        v_LightDir[i] = normalize(in_Position - u_LightPositionModelSpace[i]);
    }
}

