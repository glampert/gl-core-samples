
/* -------------------------------------------------------------
 * Simple plane-projected-shadow GLSL Fragment Shader
 * ------------------------------------------------------------- */

// From vertex shader:
layout(location = 0) in vec4 v_ShadowColor;

// Fragment color output:
out vec4 out_FragColor;

void main()
{
    out_FragColor = v_ShadowColor;
}

