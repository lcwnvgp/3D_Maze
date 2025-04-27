//
// Created by Trunway on 2025/4/23.
//

#include "physicsUtil.h"

float calculateImpulse(
        float massA, float massB,
        const glm::vec3& velocityA, const glm::vec3& velocityB,
        const glm::vec3& normal, // normal pointing from A to B
        float restitution// coefficient of restitution, 1 = elastic, 0 = inelastic
) {
    glm::vec3 relativeVelocity = velocityB - velocityA;
    float velAlongNormal = glm::dot(relativeVelocity, normal);

    // No impulse if objects are separating
    if (velAlongNormal > 0)
        return 0.0f;

    float invMassA = (massA > 0) ? 1.0f / massA : 0.0f;
    float invMassB = (massB > 0) ? 1.0f / massB : 0.0f;

    // Calculate impulse scalar
    float impulseScalar = -(1.0f + restitution) * velAlongNormal;
    impulseScalar /= invMassA + invMassB;

    return impulseScalar;
}

// This function now returns a vector of glm::vec3 (full 3D forces)
std::vector<glm::vec3> solveSupportForcesLCP(
        double mass,
        const glm::vec3& acceleration,
        const std::vector<glm::vec3>& contacts,
        int maxIterations,
        double tolerance
) {
    int contactCount = contacts.size();
    if (contactCount == 0) {
        std::cout << "No contacts." << std::endl;
        return {};
    }

    // Step 1: Build W matrix (n x n) and b vector (n)
    std::vector<std::vector<double>> W(contactCount, std::vector<double>(contactCount, 0.0));
    std::vector<double> b(contactCount, 0.0);

    for (int i = 0; i < contactCount; ++i) {
        glm::vec3 ni = contacts[i];
        b[i] = glm::dot(ni, acceleration);
        for (int j = 0; j < contactCount; ++j) {
            glm::vec3 nj = contacts[j];
            W[i][j] = glm::dot(ni, nj) / mass;
        }
    }

    // Step 2: Initialize forces (scalars) to zero
    std::vector<double> lambdas(contactCount, 0.0);

    // Step 3: Projected Gauss-Seidel solve
    for (int iter = 0; iter < maxIterations; ++iter) {
        double maxError = 0.0;
        for (int i = 0; i < contactCount; ++i) {
            double wii = W[i][i];
            if (wii == 0.0) continue; // Avoid division by zero

            double sum = b[i];
            for (int j = 0; j < contactCount; ++j) {
                if (j != i) {
                    sum += W[i][j] * lambdas[j];
                }
            }

            double oldLambda = lambdas[i];
            double newLambda = std::max(0.0, -sum / wii); // Clamp to non-negative

            lambdas[i] = newLambda;
            maxError = std::max(maxError, std::abs(newLambda - oldLambda));
        }

        if (maxError < tolerance) {
            break; // Converged
        }
    }

    // Step 4: Convert scalar lambdas to full 3D forces
    std::vector<glm::vec3> supportForces(contactCount);
    for (int i = 0; i < contactCount; ++i) {
        supportForces[i] = float(lambdas[i]) * contacts[i];
    }

    return supportForces;
}
