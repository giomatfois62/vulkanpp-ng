#ifndef VK_CAMERA_HPP
#define VK_CAMERA_HPP

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace vke {

struct Camera {
    enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

    Camera(
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f),  // Start at world origin
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),        // Y-axis as world up
        float yaw = -90.0f,                                 // Look along negative Z-axis (OpenGL convention)
        float pitch = 0.0f                                  // Level horizon
    );

    glm::vec3 position;     // Camera's location in world coordinates
    glm::vec3 front;        // Forward direction (where camera is looking)
    glm::vec3 up;           // Camera's local up direction (for roll control)
    glm::vec3 right;        // Camera's local right direction (perpendicular to front and up)
    glm::vec3 worldUp;      // Global up vector reference (typically Y-axis)

    // Rotation representation using Euler angles
    // Provides intuitive control while managing gimbal lock and other rotation complexities
    float yaw = -90.0f;             // Horizontal rotation around the world up-axis (left-right looking)
    float pitch = 0.0f;             // Vertical rotation around the camera's right axis (up-down looking)

    float movementSpeed = 2.5f;     // Units per second for translation movement
    float mouseSensitivity = 0.1f;  // Multiplier for mouse input to rotation angle conversion
    float zoom = 45.0f;             // Field of view control for perspective projection

    glm::mat4 view();
    glm::mat4 projection(float aspectRatio, float nearPlane = 0.1f, float farPlane = 100.0f);

    void processKeyboard(CameraMovement direction, float dt);
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch);
    void processMouseWheel(float yOffset);
    void updateVectors();

    struct {
        glm::mat4 view;
        glm::mat4 proj;
    } matrices;
};

} // end namespace vke

#endif // VK_CAMERA_HPP
