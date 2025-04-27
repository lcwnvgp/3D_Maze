//
// Created by Trunway on 2025/4/23.
//
#define GLM_ENABLE_EXPERIMENTAL

#include "RollingBall.h"
#include <glm/gtx/norm.hpp>  // For distance2
#include <limits>
#include "physicsUtil.h"

RollingBall::RollingBall(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue graphicsQueue,
                         const std::string& modelPath, float ballMass)
        : Model(device, physicalDevice, commandPool, graphicsQueue, modelPath),
          radius(0.0f), center(0.0f)
{
    computeBoundingSphere();
    mass = ballMass;
}

void RollingBall::computeBoundingSphere() {
    const auto& verts = GetVertices();
    if (verts.empty()) return;

    // Compute center as the average of all positions
    center = glm::vec3(0.0f);
    for (const auto& v : verts) {
        center += v.position;
    }
    center /= static_cast<float>(verts.size());

    // Compute radius as max distance to any vertex
    float maxDist2 = 0.0f;
    for (const auto& v : verts) {
        float dist2 = glm::distance2(center, v.position);
        if (dist2 > maxDist2) maxDist2 = dist2;
    }
    radius = std::sqrt(maxDist2);
}

float RollingBall::getRadius() const {
    return radius;
}

glm::vec3 RollingBall::getCenterPosition() const {
    return center + translation;
}

glm::vec3 RollingBall::getTranslation() const {
    return translation;
}

glm::mat4 RollingBall::updatePhysicalStates(float deltaTime) {
    for (int i = 0; i < rotateVelocities.size(); i++) {
        float theta = glm::length(rotateVelocities[i] * deltaTime) / radius;
        glm::quat rotationQuat = glm::angleAxis(theta, rotateAxes[i]);
        ballRotation = rotationQuat * ballRotation;
    }

    translation += velocity * deltaTime;
    velocity += acceleration * deltaTime;
//    velocity = std::min(20.0f, glm::length(velocity)) * glm::normalize(velocity);
    // center is the position of ball center when the world is initialized
    // centerPosition is the current position of the ball center
    glm::mat4 modelMatrix =
            glm::translate(glm::mat4(1.0f), getCenterPosition()) *         // 3. Move ball to world position
            glm::mat4_cast(ballRotation) *                             // 2. Rotate ball
            glm::translate(glm::mat4(1.0f), -center);       // 1. Move center to (0,0,0) first


    acceleration = glm::vec3 (0.0f, -9.81f, 0.0f); //gravity
    // only consider air friction (friction actually moves ball!!!)
    acceleration += -velocity * 1.0f / mass;
    return modelMatrix;
}

void RollingBall::updateAcceleration(const std::vector<glm::vec3>& supForces) {
    for (auto supForce : supForces) {
        if (glm::length(supForce) > 1e-5) {
            acceleration += supForce / mass;
            // (friction actually moves ball!!!)
            // friction only happens when velocity is not zero at its direction
//            if (glm::length(velocity - glm::dot(velocity, normals[i]) * normals[i]) > 1e-5) {
//                acceleration += glm::normalize(-velocity) *
//                                glm::length(supForces[i]) *
//                                touchedObjs[i].friction_coefficient /
//                                mass;
//            }
        }
    }
}

void RollingBall::updateVelocity(const std::vector<Model*>& touchedObjs,
                                 const std::vector<glm::vec3>& normals) {
    // normals have to be unit vectors
    // normals must point to the ball
    for (int i = 0; i < touchedObjs.size(); i++) {
        float impulse = calculateImpulse(touchedObjs[i]->mass, mass,
                                         touchedObjs[i]->velocity, velocity,
                                         normals[i]);

        velocity += impulse * normals[i] / mass;
    }
}

void RollingBall::updateRotV(const std::vector<glm::vec3>& normals,
                             const std::vector<glm::vec3>& supForces) {
    rotateVelocities.clear();
    rotateAxes.clear();
    for (int i = 0; i < normals.size(); i++) {
        if (glm::length(supForces[i]) > 1e-5) {
            // velocity perpendicular to support force
            glm::vec3 part_v = velocity - glm::dot(velocity, normals[i]) * normals[i];
            rotateVelocities.push_back(part_v);
            // from base to the tip, rotate CCW
            rotateAxes.push_back(glm::normalize(glm::cross(part_v, normals[i])));
        }
    }
}

void RollingBall::collisionUpdate(const std::vector<Model*>& touchedObjs,
                                  const std::vector<glm::vec3>& normals) {

    std::vector<Model*> staObjs;
    std::vector<Model*> dynObjs;
    std::vector<glm::vec3> staNormals;
    std::vector<glm::vec3> dynNormals;
    for (int i = 0; i < touchedObjs.size(); i++) {
        if (std::abs(glm::dot(normals[i], velocity - touchedObjs[i]->velocity)) > 1e-1) {
            dynObjs.push_back(touchedObjs[i]);
            dynNormals.push_back(normals[i]);
        } else {
            staObjs.push_back(touchedObjs[i]);
            staNormals.push_back(normals[i]);
//            velocity = velocity - glm::dot(velocity, normals[i]) * normals[i];
        }
    }
    std::vector<glm::vec3> supForces;
    // do resting contact first or the velocity will be changed
    if (!staObjs.empty()) {
//        std::cout << "-----------------------------" << std::endl;
//        std::cout << "resting contact!!!" << std::endl;
//        std::cout << "-----------------------------" << std::endl;
        supForces = solveSupportForcesLCP(mass, acceleration, staNormals);
        updateAcceleration(supForces);
    }

    if (!dynObjs.empty()) {
//        std::cout << "-----------------------------" << std::endl;
//        std::cout << "(" << velocity.x << ", " << velocity.y << ", " << velocity.z << ")" << std::endl;
        updateVelocity(dynObjs, dynNormals);
//        std::cout << "(" << velocity.x << ", " << velocity.y << ", " << velocity.z << ")" << std::endl;
//        std::cout << "-----------------------------" << std::endl;
    }

    if (!supForces.empty()) {
        updateRotV(staNormals, supForces);
    }

}