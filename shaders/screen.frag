#version 330 core

in vec2 TexCoord;

out vec4 FragColor;
uniform float exposure;
uniform sampler2D screenTexture;
uniform int toneMappingMode;
uniform int debugView;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(screenTexture, TexCoord).rgb;

    hdrColor *= exposure;

    if(debugView != 0)
    {
        vec3 displayColor = hdrColor;

        if(debugView == 1)
        {
            displayColor = pow(max(hdrColor, vec3(0.0)), vec3(1.0 / 2.2));
        }

        FragColor = vec4(displayColor, 1.0);
        return;
    }

    vec3 mapped;

    if (toneMappingMode == 0)
    {
        mapped = hdrColor / (hdrColor + vec3(1.0));
    }
    else
    {
        mapped = ACESFilm(hdrColor);
    }

    vec3 gammaCorrected = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(gammaCorrected, 1.0);
}