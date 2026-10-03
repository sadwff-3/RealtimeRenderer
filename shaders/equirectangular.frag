#version 450 core

out vec4 FragColor;

in vec3 LocalPos;

uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591549431, 0.3183098862);

vec2 sampleSphericalMap(vec3 direction)
{
    vec3 v = normalize(direction);

    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));

    uv *= invAtan;

    uv += 0.5;

    return uv;
}


void main()
{
    vec2 uv = sampleSphericalMap(LocalPos);

    vec3 color = texture(equirectangularMap, uv).rgb;

    FragColor = vec4(color, 1.0);
}