
/* -------------------------------------------------------------
 * Basic flat-shaded + outline GLSL Vertex Shader
 * ------------------------------------------------------------- */

// Vertex inputs/attributes:
layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec4 in_Color; // xyz used as the barycentric coords instead; w unused.
layout(location = 3) in vec2 in_TexCoords;

// Varyings:
layout(location = 0) out vec3 v_BarycentricCoords;
layout(location = 1) out vec3 v_FlatShadeColor;
layout(location = 2) out vec2 v_TexCoords;

// Uniform variables:
uniform mat4 u_MvpMatrix;
uniform mat4 u_ModelViewMatrix;

const vec3 lightPosition = vec3(0.0, 1.0, 0.0);
const vec3 ambientColor  = vec3(0.4, 0.4, 0.4);
const vec3 diffuseColor  = vec3(0.8, 0.8, 0.8);

void main()
{
    mat3 viewMatrix     = mat3(u_ModelViewMatrix);

    vec3 lightDir       = lightPosition - in_Position;
    vec3 lightVS        = normalize(viewMatrix * lightDir);  // view space light
    vec3 normalVS       = normalize(viewMatrix * in_Normal); // view space normal

    float lightContrib  = max(dot(normalVS, lightVS), 0.0);
    v_FlatShadeColor    = (ambientColor * diffuseColor) + (lightContrib * diffuseColor);

    v_BarycentricCoords = in_Color.xyz;
    v_TexCoords         = in_TexCoords;
    gl_Position         = u_MvpMatrix * vec4(in_Position, 1.0);
}

