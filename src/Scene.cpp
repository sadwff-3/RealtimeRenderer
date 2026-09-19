#include"Scene.h"
#include"SceneObject.h"


SceneObject& Scene::addObject(const std::string& name, Model* model)
{
	objects.push_back(std::make_unique<SceneObject>(name, model));
	return *objects.back();
}

std::vector<std::unique_ptr<SceneObject>>& Scene::getObjects()
{
	return objects;
}

const std::vector<std::unique_ptr<SceneObject>>& Scene::getObjects() const
{
	return objects;
}