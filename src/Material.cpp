#include"Material.h"
#include<glad/gl.h>

Material::Material(Texture* albedoTexture, Texture* normalTexture, Texture* matallicTexture, Texture* roughnessTexture, Texture* aoTexture) :albedo(albedoTexture), normalMap(normalTexture), metallicMap(matallicTexture), roughnessMap(roughnessTexture), aoMap(aoTexture)
{

}

void Material::bind(Shader& shader) const
{
	shader.use();

	shader.setVec4("baseColorFactor", baseColorFactor);
	shader.setFloat("metallicFactor", metallicFactor);
	shader.setFloat("roughnessFactor", roughnessFactor);
	shader.setFloat("normalYSign", normalYSign);
	shader.setInt("alphaMode", alphaMode);
	shader.setFloat("alphaCutoff", alphaCutoff);

	shader.setInt("albedoMap", 0);
	shader.setInt("normalMap", 1);
	shader.setInt("metallicMap", 3);
	shader.setInt("roughnessMap", 4);
	shader.setInt("aoMap", 5);

	glActiveTexture(GL_TEXTURE0);
	albedo->bind();
	glActiveTexture(GL_TEXTURE1);
	normalMap->bind();
	glActiveTexture(GL_TEXTURE3);
	metallicMap->bind();
	glActiveTexture(GL_TEXTURE4);
	roughnessMap->bind();
	glActiveTexture(GL_TEXTURE5);
	aoMap->bind();
}