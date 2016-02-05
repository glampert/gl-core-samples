
/* -------------------------------------------------------------
 * Basic point drawing GLSL Vertex Shader
 * ------------------------------------------------------------- */

// Vertex inputs/attributes:
layout(location = 0) in vec4 in_PositionPointSize;
layout(location = 1) in vec4 in_Color;

// Varyings:
layout(location = 0) out vec4 v_Color;

// Uniform variables:
uniform mat4 u_MvpMatrix;

void main()
{
    v_Color      = in_Color;
    gl_PointSize = in_PositionPointSize.w;
    gl_Position  = u_MvpMatrix * vec4(in_PositionPointSize.xyz, 1.0);
}

