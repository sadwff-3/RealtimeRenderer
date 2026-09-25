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


void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);

    vec3 N = normalize(mat3(transpose(inverse(model))) * aNormal);
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 B = normalize(mat3(model) * aBitangent);
    
    TBN = mat3(T, B, N);
    TexCoord = aTexCoord;

    gl_Position = projection * view * worldPos;
}