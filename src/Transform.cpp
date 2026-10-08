#define GLM_ENABLE_EXPERIMENTAL

#include "Transform.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>


glm::mat4 Transform::getMatrix() const
{
	glm::mat4 matrix(1.0f);

	matrix = glm::translate(matrix, position);
	matrix *= glm::toMat4(rotation);
	matrix = glm::scale(matrix, scale);

	return matrix;
}


glm::vec3 Transform::getForward() const
{
	return glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
}


glm::vec3 Transform::getEulerDegrees() const
{
	return glm::degrees(glm::eulerAngles(rotation));
}


void Transform::setEulerDegrees(const glm::vec3& eulerDegrees)
{
	rotation = glm::normalize(glm::quat(glm::radians(eulerDegrees)));
}


void Transform::setFromMatrix(const glm::mat4& matrix)
{
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::quat orientation;

	glm::decompose(matrix, scale, orientation, position, skew, perspective);
	rotation = glm::normalize(orientation);
}