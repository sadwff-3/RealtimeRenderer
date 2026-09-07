#pragma once

#include <glm/glm.hpp>

enum class CameraMovement
{
    Forward,
    Backward,
    Left,
    Right
};

class Camera
{
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;

    float Yaw;
    float Pitch;
  
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = -90.0f,
        float pitch = 0.0f
    );

    glm::mat4 getViewMatrix() const;

    void processKeyboard(
        CameraMovement direction,
        float deltaTime
    );

    void processMouseMovement(
        float xoffset,
        float yoffset
    );

     void processMouseScroll(float yoffset);
private:
    glm::vec3 WorldUp;

    void updateCameraVectors();
};