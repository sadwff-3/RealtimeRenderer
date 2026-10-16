#include"Model.h"
#include<assimp/Importer.hpp>
#include<assimp/postprocess.h>
#include<iostream>
#include<algorithm>
#include<cctype>
#include<filesystem>
#include<cmath>


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
	calculateBounds();
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

    // Assimp's *N paths refer to images embedded in the scene, not disk files.
    auto loadMaterialTexture = [&](const aiString& imagePath, bool srgb, Texture* fallback) -> Texture* {
        const aiTexture* embedded = scene->GetEmbeddedTexture(imagePath.C_Str());
        if (!embedded) {
            if (imagePath.C_Str()[0] == '*') {
                std::cerr << "Invalid Assimp embedded texture: " << imagePath.C_Str() << '\n';
                return fallback;
            }
            Texture* texture = loadTexture(directory + "/" + imagePath.C_Str(), srgb);
            return texture && texture->valid() ? texture : fallback;
        }
        const std::string key = "assimp:" + directory + ":" + imagePath.C_Str() + (srgb ? ":srgb" : ":linear");
        if (const auto found = loadedTextures.find(key); found != loadedTextures.end()) {
            return found->second.get();
        }
        if (!embedded->pcData || embedded->mWidth == 0) return fallback;
        std::unique_ptr<Texture> texture;
        if (embedded->mHeight == 0) {
            texture = std::make_unique<Texture>(reinterpret_cast<const std::uint8_t*>(embedded->pcData),
                static_cast<std::size_t>(embedded->mWidth), srgb, false);
        } else {
            // Encode raw Assimp BGRA texels as a top-origin, uncompressed TGA in memory.
            // Reuse the existing image decoder and GPU upload path.
            const std::size_t width = embedded->mWidth, height = embedded->mHeight;
            if (width > 65535 || height > 65535 || width * height > (2147483647u - 18u) / 4u) {
                std::cerr << "Assimp embedded image is too large\n";
                return fallback;
            }
            std::vector<std::uint8_t> bytes(18 + width * height * 4, 0);
            bytes[2] = 2;
            bytes[12] = static_cast<std::uint8_t>(width); bytes[13] = static_cast<std::uint8_t>(width >> 8);
            bytes[14] = static_cast<std::uint8_t>(height); bytes[15] = static_cast<std::uint8_t>(height >> 8);
            bytes[16] = 32; bytes[17] = 0x28;
            for (std::size_t i = 0; i < width * height; ++i) {
                const aiTexel& pixel = embedded->pcData[i];
                bytes[18 + i * 4] = pixel.b; bytes[19 + i * 4] = pixel.g;
                bytes[20 + i * 4] = pixel.r; bytes[21 + i * 4] = pixel.a;
            }
            texture = std::make_unique<Texture>(bytes.data(), bytes.size(), srgb, false);
        }
        if (!texture || !texture->valid()) return fallback;
        Texture* result = texture.get();
        loadedTextures.emplace(key, std::move(texture));
        return result;
    };

	if (mesh->mMaterialIndex < scene->mNumMaterials)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		
		unsigned int baseColorCount = material->GetTextureCount(aiTextureType_BASE_COLOR);
		unsigned int diffuseCount = material->GetTextureCount(aiTextureType_DIFFUSE);
		
		if (baseColorCount > 0)
		{
			aiString texturePath;
			material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);

			diffuseTexture = loadMaterialTexture(texturePath, true, defaultDiffuse.get());
		}

		else if (diffuseCount > 0)
		{
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);

			diffuseTexture = loadMaterialTexture(texturePath, true, defaultDiffuse.get());
		}

		unsigned int normalCount = material->GetTextureCount(aiTextureType_NORMALS);
		unsigned int heightCount = material->GetTextureCount(aiTextureType_HEIGHT);

		if (normalCount > 0)
		{
			aiString normalPath;
			material->GetTexture(aiTextureType_NORMALS, 0, &normalPath);

			normalTexture = loadMaterialTexture(normalPath, false, defaultNormal.get());
		}
		else if (heightCount > 0)
		{
			aiString normalPath;
			material->GetTexture(aiTextureType_HEIGHT, 0, &normalPath);

			normalTexture = loadMaterialTexture(normalPath, false, defaultNormal.get());
		}

		unsigned int metallicCount = material->GetTextureCount(aiTextureType_METALNESS);

		if (metallicCount > 0)
		{
			aiString metallicPath;
			material->GetTexture(aiTextureType_METALNESS, 0, &metallicPath);

			metallicTexture = loadMaterialTexture(metallicPath, false, defaultMetallic.get());
		}

		unsigned int roughnessCount = material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS);

		if (roughnessCount > 0)
		{
			aiString roughnessPath;
			material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &roughnessPath);

			roughnessTexture = loadMaterialTexture(roughnessPath, false, defaultRoughness.get());
		}

		unsigned int aoCount = material->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION);

		if (aoCount > 0)
		{
			aiString aoPath;
			material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &aoPath);

			aoTexture = loadMaterialTexture(aoPath, false, defaultAO.get());
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


void Model::draw(Shader& shader, MeshPass pass, std::optional<std::size_t> onlyMesh) const
{
	if (onlyMesh && *onlyMesh >= meshes.size())
		return;

	const GLboolean previousCull = glIsEnabled(GL_CULL_FACE);
	GLint previousCullMode = GL_BACK;
	glGetIntegerv(GL_CULL_FACE_MODE, &previousCullMode);
	glCullFace(GL_BACK);

	shader.use();

	const char* uniformNames[] =
	{
		"materialID",
		"baseColorFactor",
		"metallicFactor",
		"roughnessFactor",
		"normalYSign",
		"alphaMode",
		"doubleSided",
		"alphaCutoff",
		"albedoMap",
		"normalMap",
		"metallicMap",
		"roughnessMap",
		"aoMap",
		"emissiveFactor",
		"emissiveStrength",
		"emissiveMap",
		"hasEmissiveMap"
	};

	GLint locations[17];

	for (int i = 0; i < 17; ++i)
		locations[i] = glGetUniformLocation(shader.ID, uniformNames[i]);

	const int textureUnits[] = { 0, 1, 3, 4, 5 };

	for (int i = 0; i < 5; ++i)
		glUniform1i(locations[8 + i], textureUnits[i]);

	glUniform1i(locations[15], 6);

	Texture* lastTextures[5] = {};
	int lastDoubleSided = -1;

	const std::size_t first = onlyMesh.value_or(0);
	const std::size_t last = onlyMesh ? first + 1 : meshes.size();

	for (std::size_t i = first; i < last; ++i)
	{
		const Material& material = meshMaterials[i];

		const bool isTransparent = material.alphaMode == 2;
		if (isTransparent != (pass == MeshPass::Transparent))
			continue;

		const int doubleSided = material.doubleSided ? 1 : 0;

		if (lastDoubleSided != doubleSided)
		{
			if (material.doubleSided)
				glDisable(GL_CULL_FACE);
			else
				glEnable(GL_CULL_FACE);

			lastDoubleSided = doubleSided;
		}

		// 保留每个 mesh 的材质 ID 和全部材质参数更新。
		glUniform1i(locations[0], static_cast<int>(i + 1));
		glUniform4fv(locations[1], 1, &material.baseColorFactor.x);
		glUniform1f(locations[2], material.metallicFactor);
		glUniform1f(locations[3], material.roughnessFactor);
		glUniform1f(locations[4], material.normalYSign);
		glUniform1i(locations[5], material.alphaMode);
		glUniform1i(locations[6], doubleSided);
		glUniform1f(locations[7], material.alphaCutoff);
		glUniform3fv(locations[13], 1, &material.emissiveFactor.x);
		glUniform1f(locations[14], material.emissiveStrength);
		glUniform1i(locations[16], material.emissiveMap ? 1 : 0);

		Texture* textures[] =
		{
			material.albedo,
			material.normalMap,
			material.metallicMap,
			material.roughnessMap,
			material.aoMap
		};

		for (int slot = 0; slot < 5; ++slot)
		{
			if (lastTextures[slot] != textures[slot])
			{
				glActiveTexture(GL_TEXTURE0 + textureUnits[slot]);
				textures[slot]->bind();
				lastTextures[slot] = textures[slot];
			}
		}

		glActiveTexture(GL_TEXTURE6);

		if (material.emissiveMap)
		{
			material.emissiveMap->bind();
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		meshes[i].draw();
	}

	glCullFace(previousCullMode);

	if (previousCull)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}


void Model::drawShadow(Shader& shader) const
{
	shader.setInt("albedoMap", 0);
	for (std::size_t i = 0; i < meshes.size(); ++i)
	{
		const Material& material = meshMaterials[i];
		shader.setInt("alphaMode", material.alphaMode);
		shader.setFloat("alphaCutoff", material.alphaCutoff);
		shader.setVec4("baseColorFactor", material.baseColorFactor);
		if (material.alphaMode == 1)
		{
			glActiveTexture(GL_TEXTURE0);
			material.albedo->bind();
		}
		if (material.alphaMode == 2)
			continue;
		meshes[i].draw();
	}
}


static bool outsideShadow(const Mesh& mesh, const glm::mat4& localToShadowClip, float clipMargin)
{
    const glm::vec3 center = (mesh.getBoundsMin() + mesh.getBoundsMax()) * 0.5f;
    const glm::vec3 extent = (mesh.getBoundsMax() - mesh.getBoundsMin()) * 0.5f;
    const float limit = 1.0f + clipMargin;
    const glm::vec4 rowW(localToShadowClip[0][3], localToShadowClip[1][3], localToShadowClip[2][3], localToShadowClip[3][3]);

    // Test homogeneous side planes: -limit*w <= x,y <= limit*w.
    // Keep depth unconstrained for CSM casters; this also handles perspective point-light faces.
    for (int axis = 0; axis < 2; ++axis)
    {
        const glm::vec4 rowAxis(localToShadowClip[0][axis], localToShadowClip[1][axis], localToShadowClip[2][axis], localToShadowClip[3][axis]);
        for (float sign : { -1.0f, 1.0f })
        {
            const glm::vec4 plane = limit * rowW + sign * rowAxis;
            const float distance = glm::dot(glm::vec3(plane), center) + plane.w;
            const float radius = glm::dot(glm::abs(glm::vec3(plane)), extent);
            if (distance + radius < 0.0f) return true;
        }
    }
    return false;
}


void Model::drawShadowCulled(Shader& shader, const glm::mat4& localToShadowClip, float clipMargin) const
{
	shader.setInt("albedoMap", 0);

	const GLint alphaModeLocation = glGetUniformLocation(shader.ID, "alphaMode");
	const GLint alphaCutoffLocation = glGetUniformLocation(shader.ID, "alphaCutoff");
	const GLint baseColorLocation = glGetUniformLocation(shader.ID, "baseColorFactor");

	int lastAlphaMode = -1;

	for (std::size_t i = 0; i < meshes.size(); ++i)
	{
		if (outsideShadow(meshes[i], localToShadowClip, clipMargin))
			continue;

		const Material& material = meshMaterials[i];

		if (lastAlphaMode != material.alphaMode)
		{
			glUniform1i(alphaModeLocation, material.alphaMode);
			lastAlphaMode = material.alphaMode;
		}

		// 阴影 Shader 只有透明裁切材质才需要这些参数和纹理。
		if (material.alphaMode == 1)
		{
			glUniform1f(alphaCutoffLocation, material.alphaCutoff);
			glUniform4fv(baseColorLocation, 1, &material.baseColorFactor.x);

			glActiveTexture(GL_TEXTURE0);
			material.albedo->bind();
		}
		if (material.alphaMode == 2)
			continue;

		meshes[i].draw();
	}
}


void Model::drawGeometry() const
{
	for (const auto& mesh : meshes)
	{
		mesh.draw();
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


void Model::calculateBounds()
{
	if (meshes.empty())
	{
		boundsMin = glm::vec3(0.0f);
		boundsMax = glm::vec3(0.0f);
		return;
	}

	boundsMin = meshes[0].getBoundsMin();

	boundsMax = meshes[0].getBoundsMax();

	for (const Mesh& mesh : meshes)
	{
		boundsMin = glm::min(boundsMin, mesh.getBoundsMin());

		boundsMax = glm::max(boundsMax, mesh.getBoundsMax());
	}
}


const glm::vec3& Model::getBoundsMin() const
{
	return boundsMin;
}


const glm::vec3& Model::getBoundsMax() const
{
	return boundsMax;
}


std::size_t Model::getMaterialCount() const
{
	return meshMaterials.size();
}


Material& Model::getMaterial(std::size_t index)
{
	return meshMaterials[index];
}


const Material& Model::getMaterial(std::size_t index) const
{
	return meshMaterials[index];
}


glm::vec3 Model::getMeshLocalCenter(std::size_t index) const
{
	const Mesh& mesh = meshes.at(index);
	return (mesh.getBoundsMin() + mesh.getBoundsMax()) * 0.5f;
}