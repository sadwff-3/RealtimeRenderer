#version 330 core

in vec2 TexCoord;

out vec4 FragColor;
uniform float exposure;
uniform sampler2D screenTexture;

void main()
{
    vec3 hdrColor = texture(screenTexture, TexCoord).rgb;

    hdrColor *= exposure;

    vec3 mapped = hdrColor / (hdrColor + vec3(1.0));

    vec3 gammaCorrected = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(gammaCorrected, 1.0);
}