#pragma once

#include <glm/glm.hpp>

struct PointLight
{
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
};

struct DirectionalLight
{
    glm::vec3 direction{-0.2f, -1.0f, -0.3f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

enum class LightType{Directional, Point, Spot};

struct Light
{
    LightType type = LightType::Point;

    glm::vec3 color{1.0f};
    float intensity = 1.0f;

    float range = 10.0f;

    float innerAngle = 15.0f;
    float outerAngle = 25.0f;

    bool castShadow = false;
};