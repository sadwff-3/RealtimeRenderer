#include "AssetManager.h"

Model* AssetManager::loadModel(const std::string& path, bool directXNormals)
{
    const auto key = path + (directXNormals ? "#dx-normal" : "#gl-normal");
    auto it = models.find(key);

    if (it != models.end())
    {
        return it->second.get();
    }

    auto model = std::make_unique<Model>(path, directXNormals);

    if (!model->isLoaded())
    {
        return nullptr;
    }

    Model* modelPtr = model.get();

    models.emplace(key, std::move(model));

    return modelPtr;
}