#pragma once

#include<glm/glm.hpp>

class Transform
{
public:
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);

    glm::mat4 getMatrix() const;
    glm::vec3 getForward() const;

};