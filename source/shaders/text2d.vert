
/* -------------------------------------------------------------
 * GLSL Vertex Shader used for 2D screen-aligned text
 * ------------------------------------------------------------- */

// Vertex inputs/attributes:
layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_TexCoords;

// Varyings:
layout(location = 1) out vec4 v_Color;
layout(location = 2) out vec2 v_TexCoords;

// Uniform variables:
uniform vec3 u_ScreenDimensions;

void main()
{
    // Map to normalized clip coordinates:
    float x = ((2.0 * (in_Position.x - 0.5)) / u_ScreenDimensions.x) - 1.0;
    float y = 1.0 - ((2.0 * (in_Position.y - 0.5)) / u_ScreenDimensions.y);

    gl_Position = vec4(x, y, 0.0, 1.0);
    v_Color     = in_Color;
    v_TexCoords = in_TexCoords;
}

