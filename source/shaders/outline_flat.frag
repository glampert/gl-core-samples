
/* -------------------------------------------------------------
 * Basic flat-shaded + outline GLSL Fragment Shader
 * ------------------------------------------------------------- */

// Input from vertex shader (varyings):
layout(location = 0) in vec3 v_BarycentricCoords;
layout(location = 1) in vec3 v_FlatShadeColor;
layout(location = 2) in vec2 v_TexCoords;

// Uniform variables:
uniform int       u_RenderOutline;
uniform sampler2D u_BaseTexture; // @ tmu:0

// Fragment color output:
out vec4 out_FragColor;

// From: [http://codeflow.org/entries/2012/aug/02/easy-wireframe-display-with-barycentric-coordinates/]
float edgeFactor(float lineThickness)
{
    vec3 d  = fwidth(v_BarycentricCoords);
    vec3 a3 = smoothstep(vec3(0.0), d * lineThickness, v_BarycentricCoords);
    return min(min(a3.x, a3.y), a3.z);
}

void main()
{
    vec3 outlineColor = vec3(1.0, 0.0, 0.0);
    vec3 fillColor    = texture(u_BaseTexture, v_TexCoords).rgb;
    fillColor         = fillColor * v_FlatShadeColor;

    if (bool(u_RenderOutline))
    {
        float t         = edgeFactor(2.0);
        out_FragColor   = vec4(mix(outlineColor, fillColor, t), 1.0);
        out_FragColor.a = mix(1.0, 0.0, t);
    }
    else
    {
        out_FragColor = vec4(fillColor, 1.0);
    }
}

