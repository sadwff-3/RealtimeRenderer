#version 330 core

layout (location = 0) in vec3 aPos;

layout (location = 2) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

out vec3 FragPos;

void main()
{
    TexCoord = aTexCoord;
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    gl_Position = lightSpaceMatrix * worldPos;
}