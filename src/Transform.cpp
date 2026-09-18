#include"Transform.h"
#include<glm/gtc/matrix_transform.hpp>

glm::mat4 Transform::getMatrix() const
{
	glm::mat4 matrix = glm::mat4(1.0f);
	//position
	matrix = glm::translate(matrix, position);
	//rotation
	matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	matrix = glm::scale(matrix, scale);

	return matrix;
};