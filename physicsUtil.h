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
        float restitution = 1f // coefficient of restitution, 1 = elastic, 0 = inelastic
);
#endif //VULKANMAZEGAME_PHYSICSUTIL_H
