#pragma once

#include"Light.h"

#include <memory>
#include <string>
#include <vector>

class Model;
class SceneObject;

class Scene
{
public:
    SceneObject& addObject(const std::string& name, Model* model);

    SceneObject& addLight(const std::string& name, const Light& light);

    bool removeObject(SceneObject* object);

    std::vector<std::unique_ptr<SceneObject>>& getObjects();
    const std::vector<std::unique_ptr<SceneObject>>& getObjects() const;
    const std::vector<SceneObject*>& getLights() const;

private:
    std::vector<std::unique_ptr<SceneObject>> objects;
    std::vector<SceneObject*> lightObjects;
};