
/* -------------------------------------------------------------
 * Basic pass-through GLSL Vertex Shader
 * ------------------------------------------------------------- */

// Vertex inputs/attributes:
layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_TexCoords;

// Varyings:
layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoords;

// Uniform variables:
uniform mat4 u_MvpMatrix;

void main()
{
    v_Color     = in_Color;
    v_TexCoords = in_TexCoords;
    gl_Position = u_MvpMatrix * vec4(in_Position, 1.0);
}

