#include"Material.h"
#include<glad/gl.h>

Material::Material(Texture* albedoTexture, Texture* normalTexture, float ambient, float specular, float shine) :albedo(albedoTexture), normalMap(normalTexture), ambientStrength(ambient), specularStrength(specular), shininess(shine)
{

}

void Material::bind(Shader& shader) const
{
	shader.use();

	shader.setInt("texture1", 0);
	shader.setInt("normalMap", 1);
	shader.setFloat("ambientStrength", ambientStrength);
    shader.setFloat("specularStrength",specularStrength);
    shader.setFloat("shininess",shininess);

	glActiveTexture(GL_TEXTURE0);
	albedo->bind();
	glActiveTexture(GL_TEXTURE1);
	normalMap->bind();
}