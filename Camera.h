#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    Camera();
    void setPosition(const glm::vec3& position);
    void setUp(const glm::vec3& up);
    void setPerspective(float fov, float aspect, float near, float far);
    const glm::mat4& getViewMatrix() const { return viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return projectionMatrix; }
    void moveForward(float distance);
    void moveBackward(float distance);
    void moveLeft(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    void moveDown(float distance);
    void rotate(float pitch, float yaw);
    void update(float deltaTime);
    void handleInput(int key, int action);
    void handleMouseMovement(double xpos, double ypos);
    void handleMouseScroll(double yoffset);
    void setMouseButton(int button, bool state);
    bool isDragging = false; 
    void followBall(const glm::vec3& ballPosition, const glm::vec3& ballDirection);

private:
    glm::vec3 position;
    glm::vec3 up; 
    glm::vec3 right; 
    glm::vec3 front; 
    glm::quat m_orientation;

    float pitch; 
    float yaw;   

    float movementSpeed = 2.5f; 
    float mouseSensitivity = 0.1f; 
    bool firstMouse = true; 
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool m_isLeftMouseButtonDown = false;
    bool m_isMiddleMouseButtonDown = false;
    bool m_isRightMouseButtonDown = false;
    float m_panSpeed = 0.05f; 
    float m_zoomSpeed = 1.0f; 
    float m_orbitSpeed = 0.1f; 
    glm::vec3 m_orbitTarget = glm::vec3(0.0f, 0.0f, 0.0f); 
    float m_distanceToTarget = 5.0f; 

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

    glm::vec3 ballPosition; 
    glm::vec3 ballDirection; 

    void updateVectors(); 
    void updateViewMatrix(); 
    void performOrbit(float xoffset, float yoffset); 
    void performPan(float xoffset, float yoffset);   
    void performZoomDrag(float yoffset); 
};