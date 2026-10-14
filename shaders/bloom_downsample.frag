#version 450 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D sourceTexture;
uniform bool firstPass;
uniform float threshold;
uniform float softKnee;

vec3 sampleHDR(vec2 offset)
{
    vec2 halfTexel = 0.5 / vec2(textureSize(sourceTexture, 0));
    vec2 uv = clamp(TexCoord + offset, halfTexel, vec2(1.0) - halfTexel);
    vec3 color = max(texture(sourceTexture, uv).rgb, vec3(0.0));

    if (!firstPass)
        return color;

    float brightness = max(color.r, max(color.g, color.b));
    float knee = threshold * softKnee;

    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-5);

    float contribution = max(brightness - threshold, soft) / max(brightness, 1e-5);

    return color * contribution;
}

void main()
{
    vec2 t = 1.0 / vec2(textureSize(sourceTexture, 0));

    vec3 c = sampleHDR(vec2(0.0)) * 0.125;

    c += (sampleHDR(t * vec2(-2, -2)) + sampleHDR(t * vec2(2, -2)) + sampleHDR(t * vec2(-2, 2)) + sampleHDR(t * vec2(2, 2))) * 0.03125;

    c += (sampleHDR(t * vec2(0, -2)) + sampleHDR(t * vec2(-2, 0)) + sampleHDR(t * vec2(2, 0)) + sampleHDR(t * vec2(0, 2))) * 0.0625;

    c += (sampleHDR(t * vec2(-1, -1)) + sampleHDR(t * vec2(1, -1)) + sampleHDR(t * vec2(-1, 1)) + sampleHDR(t * vec2(1, 1))) * 0.125;

    FragColor = vec4(c, 1.0);
}