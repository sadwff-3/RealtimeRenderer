#include"Scene.h"
#include"SceneObject.h"


SceneObject& Scene::addObject(const std::string& name, Model* model)
{
	objects.push_back(std::make_unique<SceneObject>(name, model));
	return *objects.back();
}


SceneObject& Scene::addLight(const std::string& name, const Light& light)
{
	SceneObject& object = addObject(name, nullptr);
	object.light = light;
	lightObjects.push_back(&object);
	return object;
}


std::vector<std::unique_ptr<SceneObject>>& Scene::getObjects()
{
	return objects;
}


const std::vector<std::unique_ptr<SceneObject>>& Scene::getObjects() const
{
	return objects;
}


const std::vector<SceneObject*>& Scene::getLights() const
{
	return lightObjects;
}