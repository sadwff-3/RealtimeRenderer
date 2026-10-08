#pragma once

#include<string>
#include<optional>

#include"Light.h"
#include"Transform.h"

class Model;

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

class SceneObject
{
public:
    std::string name;
    Transform transform;
    Model* model;

    std::optional<Light> light;

    SceneObject(const std::string& name, Model* model);

    bool getWorldBounds(AABB& outBounds) const;
};