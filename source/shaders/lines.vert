
/* -------------------------------------------------------------
 * Basic line drawing GLSL Vertex Shader
 * ------------------------------------------------------------- */

// Vertex inputs/attributes:
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec4 in_Color;

// Varyings:
layout(location = 0) out vec4 v_Color;

// Uniform variables:
uniform mat4 u_MvpMatrix;

void main()
{
    v_Color     = in_Color;
    gl_Position = u_MvpMatrix * vec4(in_Position, 1.0);
}

