#include "Camera.h"
#include <GLFW/glfw3.h>

Camera::Camera() 
    : position(glm::vec3(0.0f, 0.0f, 3.0f)),
      front(glm::vec3(0.0f, 0.0f, -1.0f)),
      up(glm::vec3(0.0f, 1.0f, 0.0f)),
      worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f),
      pitch(0.0f),
      movementSpeed(2.5f),
      mouseSensitivity(0.1f),
      zoom(45.0f) {
    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    // 这里可以添加自动移动或动画逻辑
}

void Camera::handleInput(int key, int action) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        float velocity = movementSpeed;
        switch (key) {
            case GLFW_KEY_W:
                position += front * velocity;
                break;
            case GLFW_KEY_S:
                position -= front * velocity;
                break;
            case GLFW_KEY_A:
                position -= right * velocity;
                break;
            case GLFW_KEY_D:
                position += right * velocity;
                break;
        }
    }
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(zoom), 800.0f / 600.0f, 0.1f, 100.0f);
}

void Camera::updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
} 