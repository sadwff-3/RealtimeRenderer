#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoord;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;


vec3 fallbackTangent(vec3 N)
{
    vec3 helper = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);

    return normalize(cross(helper, N));
}


void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);

    vec3 N = normalize(mat3(transpose(inverse(model))) * aNormal);
    mat3 model3 = mat3(model);
    vec3 rawT = model3 * aTangent;
    vec3 rawB = model3 * aBitangent;

    
    vec3 T = rawT - N * dot(N, rawT);

    if (dot(T, T) > 1e-10)
        T = normalize(T);
    else
        T = fallbackTangent(N);

    
    float handedness = dot(cross(N, T), rawB) < 0.0 ? -1.0 : 1.0;

    vec3 B = handedness * normalize(cross(N, T));

    TBN = mat3(T, B, N);

    TexCoord = aTexCoord;

    gl_Position = projection * view * worldPos;
}