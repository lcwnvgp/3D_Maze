#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream> 

Camera::Camera()
    : position(0.0f, 0.0f, 10.0f),
      up(0.0f, 1.0f, 0.0f), 
      pitch(0.0f), 
      yaw(-90.0f), 
      movementSpeed(2.5f),
      mouseSensitivity(0.1f),
      firstMouse(true),
      lastX(1600.0f / 2.0f), 
      lastY(1200.0f / 2.0f),
      isDragging(false),
      m_isLeftMouseButtonDown(false),
      m_isMiddleMouseButtonDown(false),
      m_isRightMouseButtonDown(false),
      m_panSpeed(0.005f), 
      m_zoomSpeed(0.05f), 
      m_orbitSpeed(0.1f), 
      m_orbitTarget(0.0f, 0.0f, 0.0f)
{
    updateVectors();
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    if (m_distanceToTarget < 0.001f) {
         m_distanceToTarget = 5.0f; 
         position = m_orbitTarget - front * m_distanceToTarget; 
    }
    updateViewMatrix();
    setPerspective(45.0f, 1600.0f / 1200.0f, 0.1f, 1000.0f);
}
void Camera::setMouseButton(int button, bool state) {
    bool anyRelevantButtonWasDown = m_isLeftMouseButtonDown || m_isMiddleMouseButtonDown || m_isRightMouseButtonDown;
    if (button == GLFW_MOUSE_BUTTON_LEFT) m_isLeftMouseButtonDown = state;
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) m_isMiddleMouseButtonDown = state;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) m_isRightMouseButtonDown = state;
    bool anyRelevantButtonIsDown = m_isLeftMouseButtonDown || m_isMiddleMouseButtonDown || m_isRightMouseButtonDown;
    if (!anyRelevantButtonWasDown && anyRelevantButtonIsDown) {
         firstMouse = true; 
    }
    isDragging = anyRelevantButtonIsDown;
}
void Camera::handleMouseMovement(double xpos, double ypos) {
    if (!isDragging) {
         lastX = xpos; 
         lastY = ypos;
         return;
    }
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY; 
    lastX = xpos;
    lastY = ypos;
    if (m_isLeftMouseButtonDown) {
        performOrbit(xoffset, yoffset);
    } else if (m_isMiddleMouseButtonDown) {
        performPan(xoffset, yoffset);
    } else if (m_isRightMouseButtonDown) {
        performZoomDrag(yoffset);
    }
}
void Camera::handleMouseScroll(double yoffset) {
    m_distanceToTarget -= (float)yoffset * m_zoomSpeed * 5.0f; 
    if (m_distanceToTarget < 0.1f) { 
        m_distanceToTarget = 0.1f;
    }

    position = m_orbitTarget - front * m_distanceToTarget;
    updateViewMatrix();
}
void Camera::performOrbit(float xoffset, float yoffset) {
    float yawAngleDelta = xoffset * m_orbitSpeed;
    float pitchAngleDelta = -yoffset * m_orbitSpeed; 
    yaw += yawAngleDelta;
    pitch += pitchAngleDelta;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    updateVectors();
    position = m_orbitTarget - front * m_distanceToTarget;
    updateViewMatrix();
}
void Camera::performPan(float xoffset, float yoffset) {
    glm::vec3 panVector = right * xoffset * m_panSpeed + up * yoffset * m_panSpeed; 
    position += panVector;
    m_orbitTarget += panVector; 
    updateViewMatrix();
}
void Camera::performZoomDrag(float yoffset) {
     m_distanceToTarget += yoffset * m_zoomSpeed * 5.0f; 
     if (m_distanceToTarget < 0.1f) {
         m_distanceToTarget = 0.1f;
     }

     position = m_orbitTarget - front * m_distanceToTarget;
     updateViewMatrix();
}

void Camera::setPosition(const glm::vec3& pos) {
    position = pos;
    updateViewMatrix();
}

void Camera::setUp(const glm::vec3& u) {
    up = u; 
    updateVectors(); 
    updateViewMatrix();
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
    projectionMatrix = glm::perspective(glm::radians(fov), aspect, near, far);
}

void Camera::moveForward(float distance) {
    position += front * distance;
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    updateViewMatrix();
}

void Camera::moveBackward(float distance) {
    position -= front * distance;
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    updateViewMatrix();
}

void Camera::moveLeft(float distance) {
    position -= right * distance;
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    updateViewMatrix();
}

void Camera::moveRight(float distance) {
    position += right * distance;
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    updateViewMatrix();
}

void Camera::moveUp(float distance) {
    position += up * distance;
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    updateViewMatrix();
}

void Camera::moveDown(float distance) {
    position -= up * distance;
    m_distanceToTarget = glm::length(position - m_orbitTarget);
    updateViewMatrix();
}

void Camera::rotate(float p, float y) {
    pitch += p;
    yaw += y;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    updateVectors();
    position = m_orbitTarget - front * m_distanceToTarget;
    updateViewMatrix();
}

void Camera::updateVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))); 
    up = glm::normalize(glm::cross(right, front)); 
}

void Camera::updateViewMatrix() {
    viewMatrix = glm::lookAt(position, m_orbitTarget, up); 
}

void Camera::update(float deltaTime) {
}

void Camera::handleInput(int key, int action) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        float velocity = movementSpeed * 0.1f; 
        switch (key) {
            case GLFW_KEY_W:
                moveForward(velocity);
                break;
            case GLFW_KEY_S:
                moveBackward(velocity);
                break;
            case GLFW_KEY_A:
                moveLeft(velocity);
                break;
            case GLFW_KEY_D:
                moveRight(velocity);
                break;
            case GLFW_KEY_SPACE:
                moveUp(velocity); 
                break;
            case GLFW_KEY_LEFT_CONTROL:
                moveDown(velocity); 
                break;
        }
    }
}
