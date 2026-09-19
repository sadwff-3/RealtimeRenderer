#pragma once

#include<string>
#include"Transform.h"

class Model;

class SceneObject
{
public:
    std::string name;
    Transform transform;
    Model* model;

    SceneObject(const std::string& name, Model* model);
};