#include"Model.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <filesystem>


static glm::mat4 aiMatrixToGlm(const aiMatrix4x4& m)
{
	glm::mat4 result;

	result[0][0] = m.a1; result[1][0] = m.a2; result[2][0] = m.a3; result[3][0] = m.a4;
	result[0][1] = m.b1; result[1][1] = m.b2; result[2][1] = m.b3; result[3][1] = m.b4;
	result[0][2] = m.c1; result[1][2] = m.c2; result[2][2] = m.c3; result[3][2] = m.c4;
	result[0][3] = m.d1; result[1][3] = m.d2; result[2][3] = m.d3; result[3][3] = m.d4;

	return result;
}

Model::Model(const std::string& path, bool useDirectXNormals) : directXNormals(useDirectXNormals)
{
	defaultDiffuse = std::make_unique<Texture>(255, 255, 255);
	defaultNormal = std::make_unique<Texture>(128, 128, 255);
	defaultMetallic = std::make_unique<Texture>(0, 0, 0);
	defaultRoughness = std::make_unique<Texture>(128, 128, 128);
	defaultAO = std::make_unique<Texture>(255, 255, 255);
	loadModel(path);
}

void Model::loadModel(const std::string& path)
{
	std::string suffix = std::filesystem::path(path).extension().string();
	std::transform(suffix.begin(), suffix.end(), suffix.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	
	if (suffix == ".gltf" || suffix == ".glb") {loadGltf(path);if (loaded) return;std::cerr << "fastgltf failed, attempting Assimp fallback: " << path << '\n';}

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);

	if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mRootNode == nullptr)
	{
		std::cerr << "ASSIMP ERROR" << importer.GetErrorString() << '\n';
		return;
	}

	directory = path.substr(0, path.find_last_of("/\\"));

	processNode(scene->mRootNode, scene, glm::mat4(1.0f));

	loaded = true;
}

void Model::processNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform)
{
	glm::mat4 localTransform = aiMatrixToGlm(node->mTransformation);
	glm::mat4 globalTransform = parentTransform * localTransform;

	for (unsigned int i = 0;i < node->mNumMeshes;++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene, globalTransform));
	}

	for (unsigned int i = 0;i < node->mNumChildren;++i)
	{
		processNode(node->mChildren[i], scene, globalTransform);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform)
{

	std::vector<Vertex> vertices;
	std::vector<unsigned int>indices;

	Texture* diffuseTexture = defaultDiffuse.get();
	Texture* normalTexture = defaultNormal.get();
	Texture* metallicTexture = defaultMetallic.get();
	Texture* roughnessTexture = defaultRoughness.get();
	Texture* aoTexture = defaultAO.get();

	if (mesh->mMaterialIndex < scene->mNumMaterials)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		
		unsigned int baseColorCount = material->GetTextureCount(aiTextureType_BASE_COLOR);
		unsigned int diffuseCount = material->GetTextureCount(aiTextureType_DIFFUSE);
		
		if (baseColorCount > 0)
		{
			aiString texturePath;
			material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);

			std::string fullPath = directory + "/" + texturePath.C_Str();
			diffuseTexture = loadTexture(fullPath, true);
		}

		else if (diffuseCount > 0)
		{
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);

			std::string fullPath = directory + "/" + texturePath.C_Str();
			diffuseTexture = loadTexture(fullPath, true);
		}

		unsigned int normalCount = material->GetTextureCount(aiTextureType_NORMALS);
		unsigned int heightCount = material->GetTextureCount(aiTextureType_HEIGHT);

		if (normalCount > 0)
		{
			aiString normalPath;
			material->GetTexture(aiTextureType_NORMALS, 0, &normalPath);

			std::string fullNormalPath = directory + "/" + normalPath.C_Str();
			normalTexture = loadTexture(fullNormalPath, false);
		}
		else if (heightCount > 0)
		{
			aiString normalPath;
			material->GetTexture(aiTextureType_HEIGHT, 0, &normalPath);

			std::string fullNormalPath = directory + "/" + normalPath.C_Str();
			normalTexture = loadTexture(fullNormalPath, false);
		}

		unsigned int metallicCount = material->GetTextureCount(aiTextureType_METALNESS);

		if (metallicCount > 0)
		{
			aiString metallicPath;
			material->GetTexture(aiTextureType_METALNESS, 0, &metallicPath);

			std::string fullMetallicPath = directory + "/" + metallicPath.C_Str();
			metallicTexture = loadTexture(fullMetallicPath, false);
		}

		unsigned int roughnessCount = material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS);

		if (roughnessCount > 0)
		{
			aiString roughnessPath;
			material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &roughnessPath);

			std::string fullRoughnessPath = directory + "/" + roughnessPath.C_Str();
			roughnessTexture = loadTexture(fullRoughnessPath, false);
		}

		unsigned int aoCount = material->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION);

		if (aoCount > 0)
		{
			aiString aoPath;
			material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &aoPath);

			std::string fullAOPath = directory + "/" + aoPath.C_Str();
			aoTexture = loadTexture(fullAOPath, false);
		}
	
	}

	vertices.reserve(mesh->mNumVertices);
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

	for (unsigned int i = 0;i < mesh->mNumVertices;++i)
	{
		Vertex vertex{};
		glm::vec4 localPosition(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
		vertex.position = glm::vec3(transform * localPosition);

		if (mesh->HasNormals())
		{
			glm::vec3 localNormal(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			vertex.normal = glm::normalize(normalMatrix * localNormal);
		}
		if (mesh->mTextureCoords[0])
		{
			vertex.texCoord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		}
		else
		{
			vertex.texCoord = glm::vec2(0.0f);
		}

		vertices.push_back(vertex);
	}

	for (unsigned int i = 0;i < mesh->mNumFaces;++i)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0;j < face.mNumIndices;++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	meshMaterials.emplace_back(diffuseTexture, normalTexture, metallicTexture, roughnessTexture, aoTexture);

	return Mesh(vertices, indices);
}

void Model::draw(Shader& shader) const
{

	for (std::size_t i = 0; i < meshes.size(); ++i)
	{
		meshMaterials[i].bind(shader);
		meshes[i].draw();
	}
}

Texture* Model::loadTexture(const std::string& path, bool srgb)
{
	std::string cacheKey = path + (srgb ? "_srgb" : "_linear");

	auto it = loadedTextures.find(cacheKey);

	if (it != loadedTextures.end())
	{
		return it->second.get();
	}

	auto texture = std::make_unique<Texture>(path.c_str(), srgb);
	Texture* texturePtr = texture.get();

	loadedTextures.emplace(cacheKey, std::move(texture));

	return texturePtr;
}

bool Model::isLoaded() const
{
	return loaded;
}