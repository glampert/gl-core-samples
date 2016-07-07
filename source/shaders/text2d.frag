
/* -------------------------------------------------------------
 * GLSL Fragment Shader used for 2D screen-aligned text
 * ------------------------------------------------------------- */

// Input from vertex shader (varyings):
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_TexCoords;

// Fragment color output:
out vec4 out_FragColor;

// Uniform variables:
uniform sampler2D u_GlyphTexture; // @ tmu:0

void main()
{
    out_FragColor = v_Color;
    out_FragColor.a = texture(u_GlyphTexture, v_TexCoords).r;
}

