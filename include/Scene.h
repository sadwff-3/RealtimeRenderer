#pragma once

#include <memory>
#include <string>
#include <vector>

class Model;
class SceneObject;

class Scene
{
public:
    SceneObject& addObject(const std::string& name, Model* model);

    std::vector<std::unique_ptr<SceneObject>>& getObjects();
    const std::vector<std::unique_ptr<SceneObject>>& getObjects() const;

private:
    std::vector<std::unique_ptr<SceneObject>> objects;
};