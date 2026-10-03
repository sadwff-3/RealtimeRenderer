#version 450 core

out vec4 FragColor;

in vec3 LocalPos;

uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main()
{
    vec3 N = normalize(LocalPos);

    vec3 helper = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);

    vec3 right = normalize(cross(helper, N));

    vec3 up = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);

    float sampleCount = 0.0;
    float sampleDelta = 0.025;

    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

            vec3 sampleDirection = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += textureLod(environmentMap, sampleDirection, 4.0).rgb * cos(theta) * sin(theta);

            sampleCount += 1.0;
        }
    }

    irradiance = PI * irradiance / sampleCount;

    FragColor = vec4(irradiance, 1.0);
}