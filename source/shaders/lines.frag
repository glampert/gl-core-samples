
/* -------------------------------------------------------------
 * Basic line drawing GLSL Fragment Shader
 * ------------------------------------------------------------- */

// Input from vertex shader (varyings):
layout(location = 0) in vec4 v_Color;

// Fragment color output:
out vec4 out_FragColor;

void main()
{
    out_FragColor = v_Color;
}

