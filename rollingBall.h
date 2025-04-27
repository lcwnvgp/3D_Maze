//
// Created by Trunway on 2025/4/23.
//

#pragma once

#include "Model.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

class RollingBall : public Model {
public:
    RollingBall(VkDevice device, VkPhysicalDevice physicalDevice,
                VkCommandPool commandPool, VkQueue graphicsQueue,
                const std::string& modelPath, float ballMass = 1.0);

    float getRadius() const;
    glm::vec3 getCenterPosition() const;
    glm::vec3 getTranslation() const;
    glm::mat4 updatePhysicalStates(float deltaTime); // Time in seconds
    void updateAcceleration(const std::vector<glm::vec3>& supForces);
    void updateVelocity(const std::vector<Model*>& touchedObjs,
                        const std::vector<glm::vec3>& normals);
    void updateRotV(const std::vector<glm::vec3>& normals,
                    const std::vector<glm::vec3>& supForces);
    void collisionUpdate(const std::vector<Model*>& touchedObjs,
                         const std::vector<glm::vec3>& normals);
    glm::vec3 acceleration = glm::vec3(0.0f);

private:
    void computeBoundingSphere();

    float radius;
    glm::vec3 center;
    glm::vec3 translation = glm::vec3(0.0f, 5.0f, 0.0f);
    glm::quat ballRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    std::vector<glm::vec3> rotateVelocities; //only on the direction of negative friction
    std::vector<glm::vec3> rotateAxes;
};
