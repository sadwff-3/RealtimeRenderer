#pragma once

#include <glm/gtc/quaternion.hpp>
#include<glm/glm.hpp>

class Transform
{
public:
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 getMatrix() const;
    glm::vec3 getForward() const;
    glm::vec3 getEulerDegrees() const;
    void setEulerDegrees(const glm::vec3& eulerDegrees);
    void setFromMatrix(const glm::mat4& matrix);

};