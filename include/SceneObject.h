#pragma once

#include<string>
#include<optional>

#include"Light.h"
#include"Transform.h"

class Model;

class SceneObject
{
public:
    std::string name;
    Transform transform;
    Model* model;

    std::optional<Light> light;

    SceneObject(const std::string& name, Model* model);
};