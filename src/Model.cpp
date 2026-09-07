#include"Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <iostream>

Model::Model(const std::string& path)
{
	defaultDiffuse = std::make_unique<Texture>(255, 255, 255);
	defaultNormal = std::make_unique<Texture>(128, 128, 255);
	loadModel(path);
}

void Model::loadModel(const std::string& path)
{
	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);

	if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mRootNode == nullptr)
	{
		std::cerr << "ASSIMP ERROR" << importer.GetErrorString() << '\n';
		return;
	}

	directory = path.substr(0, path.find_last_of("/\\"));

	processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0;i < node->mNumMeshes;++i)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
	}

	for (unsigned int i = 0;i < node->mNumChildren;++i)
	{
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{

	std::vector<Vertex> vertices;
	std::vector<unsigned int>indices;

	Texture* diffuseTexture = defaultDiffuse.get();
	Texture* normalTexture = defaultNormal.get();

	if (mesh->mMaterialIndex < scene->mNumMaterials)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		unsigned int diffuseCount = material->GetTextureCount(aiTextureType_DIFFUSE);

		if (diffuseCount > 0)
		{
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);

			std::string fullPath = directory + "/" + texturePath.C_Str();
			diffuseTexture = loadTexture(fullPath);
		}

		unsigned int normalCount = material->GetTextureCount(aiTextureType_NORMALS);
		unsigned int heightCount = material->GetTextureCount(aiTextureType_HEIGHT);

		if (normalCount > 0)
		{
			aiString normalPath;
			material->GetTexture(aiTextureType_NORMALS, 0, &normalPath);

			std::string fullNormalPath = directory + "/" + normalPath.C_Str();
			normalTexture = loadTexture(fullNormalPath);
		}
		else if (heightCount > 0)
		{
			aiString normalPath;
			material->GetTexture(aiTextureType_HEIGHT, 0, &normalPath);

			std::string fullNormalPath = directory + "/" + normalPath.C_Str();
			normalTexture = loadTexture(fullNormalPath);
		}
	
	}

	vertices.reserve(mesh->mNumVertices);

	for (unsigned int i = 0;i < mesh->mNumVertices;++i)
	{
		Vertex vertex{};
		
		vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		if (mesh->HasNormals())
		{
			vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
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

	meshMaterials.emplace_back(diffuseTexture, normalTexture, 0.2f, 0.3f, 32.0f);

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

Texture* Model::loadTexture(const std::string& path)
{
	auto it = loadedTextures.find(path);

	if (it != loadedTextures.end())
	{
		return it->second.get();
	}

	auto texture = std::make_unique<Texture>(path.c_str());
	Texture* texturePtr = texture.get();

	loadedTextures.emplace(path, std::move(texture));

	return texturePtr;
}