#pragma once

#include "Model.h"
#include <memory>
#include <string>
#include <unordered_map>

class AssetManager
{
public:
    Model* loadModel(const std::string& path, bool directXNormals = false);

private:
    std::unordered_map<std::string, std::unique_ptr<Model>> models;
};