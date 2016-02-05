
/* -------------------------------------------------------------
 * GLSL Fragment Shader for the projected spotlight texture demo
 * ------------------------------------------------------------- */

// Factor modulated with the spotlight base map color
// to roughly simulate how bright the light is.
const float lightStrength = 1.5;

// This must match two other places: the VS and C++ code !!!
// GLSL 4+ allows this to be a uniform variable, but a constant should still be more efficient.
const int numOfLights = 2;

// Input from vertex shader (varyings):
layout(location = 0) in vec3 v_Normal;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in vec2 v_TexCoords;
layout(location = 3) in vec4 v_ProjTexCoords[numOfLights]; // takes slots 3 & 4
layout(location = 5) in vec3 v_LightDir[numOfLights];      // takes 5 & 6

// Fragment color output:
out vec4 out_FragColor;

// Uniform variables:
uniform sampler2D u_ColorTexture;                  // @ tmu:0
uniform sampler2D u_ProjectedTexture[numOfLights]; // @ tmu:1

// Used to mix the spotLightColor and baseColor textures.
vec4 blend(vec4 c0, vec4 c1)
{
    return (c0 + c1) / 2.0;
}

void main()
{
    // Base texture color:
    vec4 baseColor = texture(u_ColorTexture, v_TexCoords);

    // Compute the contribution of each light source:
    vec4 finalColor = vec4(0.0);
    for (int i = 0; i < numOfLights; ++i)
    {
        // Projective fetch of spotlight:
        vec4 spotLightColor = textureProj(u_ProjectedTexture[i], v_ProjTexCoords[i].xyz);

        // Combine with light brightness/strength adjust:
        spotLightColor *= lightStrength;

        // Compute N.L:
        float NdotL = max(0.0, -dot(v_Normal, v_LightDir[i]));

        // Add to final output color:
        finalColor += blend(spotLightColor, baseColor) * v_Color * NdotL;

        //
		// TEST: Uncomment this and comment the above line
        // to view the projected spotlight texture only:
        //
        //finalColor += spotLightColor * v_Color * NdotL;
    }

    out_FragColor = finalColor;
}

