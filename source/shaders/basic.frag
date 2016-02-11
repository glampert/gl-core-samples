
/* -------------------------------------------------------------
 * Basic texture-mapping-only GLSL Fragment Shader
 * ------------------------------------------------------------- */

// Input from vertex shader (varyings):
layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_TexCoords;

// Fragment color output:
out vec4 out_FragColor;

// Uniform variables:
uniform sampler2D u_BaseTexture; // @ tmu:0

void main()
{
    out_FragColor = texture(u_BaseTexture, v_TexCoords) * v_Color;
}

