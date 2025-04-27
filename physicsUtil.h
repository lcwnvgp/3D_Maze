//
// Created by Trunway on 2025/4/23.
//

#ifndef VULKANMAZEGAME_PHYSICSUTIL_H
#define VULKANMAZEGAME_PHYSICSUTIL_H
#include <glm/glm.hpp> // Use glm::vec3 for 3D vectors
#include <iostream>
#include <vector>
#include <algorithm>    // for std::max

float calculateImpulse(
        float massA, float massB,
        const glm::vec3& velocityA, const glm::vec3& velocityB,
        const glm::vec3& normal, // normal pointing from A to B
        float restitution = 1.0f // coefficient of restitution, 1 = elastic, 0 = inelastic
);

struct Contact {
    glm::vec3 normal; // Normal vector pointing toward the ball
};

std::vector<glm::vec3> solveSupportForcesLCP(
        double mass,
        const glm::vec3& acceleration,
        const std::vector<glm::vec3>& contacts,
        int maxIterations = 100,
        double tolerance = 1e-5
);

#endif //VULKANMAZEGAME_PHYSICSUTIL_H
