//
// Created by Trunway on 2025/4/23.
//

#include "physicsUtil.h"

float calculateImpulse(
        float massA, float massB,
        const glm::vec3& velocityA, const glm::vec3& velocityB,
        const glm::vec3& normal,
        float restitution
) {
    glm::vec3 relativeVelocity = velocityB - velocityA;
    float velAlongNormal = glm::dot(relativeVelocity, normal);

    if (velAlongNormal > 0)
        return 0.0f;

    float invMassA = (massA > 0) ? 1.0f / massA : 0.0f;
    float invMassB = (massB > 0) ? 1.0f / massB : 0.0f;

    float impulseScalar = -(1.0f + restitution) * velAlongNormal;
    impulseScalar /= invMassA + invMassB;

    return impulseScalar;
}
