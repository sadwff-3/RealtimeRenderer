#pragma once

#include "Mesh.h"
#include"Texture.h"
#include "Material.h"
#include "Shader.h"

#include<glm/glm.hpp> 
#include<memory>
#include<unordered_map>
#include <assimp/scene.h>

#include <string>
#include <vector>

class Model
{
public:
    explicit Model(const std::string& path, bool directXNormals = false);

    void draw(Shader& shader) const;

    bool isLoaded() const;

private:
    std::vector<Mesh> meshes;
    std::vector<Material> meshMaterials;
    bool loaded = false;
    bool directXNormals = false;
    std::string directory;

    std::unordered_map<std::string, std::unique_ptr<Texture>> loadedTextures;
    std::unique_ptr<Texture> defaultNormal;
    std::unique_ptr<Texture> defaultDiffuse;
    std::unique_ptr<Texture> defaultMetallic;
    std::unique_ptr<Texture> defaultRoughness;
    std::unique_ptr<Texture> defaultAO;

    void loadModel(const std::string& path);
    void loadGltf(const std::string& path);
    void processNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform);

    Texture* loadTexture(const std::string& path, bool srgb);
};