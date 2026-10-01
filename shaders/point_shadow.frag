#version 330 core

in vec3 FragPos;

uniform vec3 lightPos;
uniform float farPlane;

void main()
{
    float lightDistance = length(FragPos - lightPos);

    lightDistance /= farPlane;

    gl_FragDepth = lightDistance;
}