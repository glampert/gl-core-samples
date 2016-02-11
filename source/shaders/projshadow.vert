
/* -------------------------------------------------------------
 * Simple plane-projected-shadow GLSL Vertex Shader
 * ------------------------------------------------------------- */

layout(location = 0) in vec3 in_Position;
layout(location = 0) out vec4 v_ShadowColor;

uniform mat4 u_MvpMatrix;
uniform vec3 u_LightPosModelSpace;
uniform vec4 u_ShadowParams; // x=shadow falloff; y=shadow opacity; zw=unused.

void main()
{
    gl_Position = u_MvpMatrix * vec4(in_Position, 1.0);

    // Same idea behind the flashlight falloff effect.
    // We fade the shadow away based on its distance from the light source.
    float dist = length(u_LightPosModelSpace - in_Position) * u_ShadowParams.x;

    // Ensure between [0,1]:
    dist = clamp(dist, 0.0, 1.0);

    v_ShadowColor.xyz = vec3(0.0);
    v_ShadowColor.w   = u_ShadowParams.y * dist;
}

