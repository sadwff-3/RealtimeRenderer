#version 450 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sourceTexture;

void main()
{
    vec2 t = 1.0 / vec2(textureSize(sourceTexture, 0));

    vec3 c = texture(sourceTexture, TexCoord).rgb * 4.0;

    c += (texture(sourceTexture, TexCoord + t * vec2(-1, 0)).rgb + texture(sourceTexture, TexCoord + t * vec2(1, 0)).rgb + texture(sourceTexture, TexCoord + t * vec2(0, -1)).rgb + texture(sourceTexture, TexCoord + t * vec2(0, 1)).rgb) * 2.0;

    c += texture(sourceTexture, TexCoord + t * vec2(-1, -1)).rgb + texture(sourceTexture, TexCoord + t * vec2(1, -1)).rgb + texture(sourceTexture, TexCoord + t * vec2(-1, 1)).rgb + texture(sourceTexture, TexCoord + t * vec2(1, 1)).rgb;

    FragColor = vec4(c / 16.0, 1.0);
}