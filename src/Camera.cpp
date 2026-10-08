#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 position,glm::vec3 up,float yaw,float pitch): Position(position),Front(glm::vec3(0.0f, 0.0f, -1.0f)),WorldUp(up),Yaw(yaw),Pitch(pitch),MovementSpeed(2.5f),MouseSensitivity(0.1f), Zoom(45.0f)
{
    updateCameraVectors();
}


glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(Position,Position + Front,Up);
}


glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(Zoom), aspectRatio, NearPlane, FarPlane);
}


void Camera::processKeyboard(CameraMovement direction,float deltaTime)
{
    float velocity =MovementSpeed * deltaTime;

    if (direction == CameraMovement::Forward)
    {
        Position += Front * velocity;
    }

    if (direction == CameraMovement::Backward)
    {
        Position -= Front * velocity;
    }

    if (direction == CameraMovement::Left)
    {
        Position -= Right * velocity;
    }

    if (direction == CameraMovement::Right)
    {
        Position += Right * velocity;
    }

    if (direction == CameraMovement::Up)
    {
        Position += WorldUp * velocity;
    }

    if (direction == CameraMovement::Down)
    {
        Position -= WorldUp * velocity;
    }
}


void Camera::processMouseMovement(float xoffset,float yoffset)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    if (Pitch > 89.0f)
        Pitch = 89.0f;

    if (Pitch < -89.0f)
        Pitch = -89.0f;

    updateCameraVectors();
}


void Camera::processMouseScroll(float yoffset)
{
    Zoom -= yoffset;

    if (Zoom < 1.0f)
        Zoom = 1.0f;

    if (Zoom > 90.0f)
        Zoom = 90.0f;
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;

    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));

    front.y = sin(glm::radians(Pitch));

    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));

    Front = glm::normalize(front);

    Right = glm::normalize(glm::cross(Front, WorldUp));

    Up = glm::normalize(glm::cross(Right, Front));
}