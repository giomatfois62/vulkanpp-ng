#include "vk_camera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

using namespace vke;

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) :
    position(position), up(up), worldUp(up), yaw(yaw), pitch(pitch)
{
    updateVectors();
}

glm::mat4 Camera::view()
{
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::projection()
{
    return glm::perspective(glm::radians(zoom), aspectRatio, nearPlane, farPlane);
}

void Camera::processKeyboard(CameraMovement direction, float dt)
{
    float velocity = movementSpeed * dt;

    switch (direction) {
        case CameraMovement::FORWARD:
            position += front * velocity;
            break;
        case CameraMovement::BACKWARD:
            position -= front * velocity;
            break;
        case CameraMovement::LEFT:
            position -= right * velocity;
            break;
        case CameraMovement::RIGHT:
            position += right * velocity;
            break;
        case CameraMovement::UP:
            position += up * velocity;
            break;
        case CameraMovement::DOWN:
            position -= up * velocity;
            break;
    }
}

void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch)
{
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    yaw += xOffset;
    pitch += yOffset;

    // Constrain pitch to avoid flipping
    if (constrainPitch) {
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

    // Update camera vectors based on updated Euler angles
    updateVectors();
}

void Camera::processMouseWheel(float yOffset)
{
    zoom -= yOffset;
    zoom = glm::clamp(zoom, 1.0f, 90.0f);
}

void Camera::updateVectors()
{
    // Calculate the new front vector
    glm::vec3 newFront{
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    };
    front = glm::normalize(newFront);

    // Recalculate the right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}
