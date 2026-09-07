#pragma once

#include "Mesh.h"
#include"Texture.h"
#include "Material.h"
#include "Shader.h"

#include<memory>
#include<unordered_map>
#include <assimp/scene.h>

#include <string>
#include <vector>

class Model
{
public:
    explicit Model(const std::string& path);

    void draw(Shader& shader) const;

private:
    std::vector<Mesh> meshes;
    std::vector<Material> meshMaterials;

    std::string directory;

    std::unordered_map<std::string, std::unique_ptr<Texture>> loadedTextures;
    std::unique_ptr<Texture> defaultNormal;
    std::unique_ptr<Texture> defaultDiffuse;

    void loadModel(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);

    Texture* loadTexture(const std::string& path);
};