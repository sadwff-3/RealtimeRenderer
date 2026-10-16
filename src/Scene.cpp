#include"Scene.h"
#include"SceneObject.h"

#include<algorithm>


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


bool Scene::removeObject(SceneObject* object)
{
	if (object == nullptr)
		return false;

	auto it = std::find_if(objects.begin(), objects.end(), [object](const std::unique_ptr<SceneObject>& current) {return current.get() == object;});

	if (it == objects.end())
		return false;

	lightObjects.erase(
		std::remove(lightObjects.begin(), lightObjects.end(), object),
		lightObjects.end());

	objects.erase(it);

	return true;
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
